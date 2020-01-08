#include <jpeg_enc_node.hpp>
#include <va_common/va_display.h>
#include <cstdio>

VaapiSurfaceAllocator::VaapiSurfaceAllocator(VADisplay* dpy):m_dpy(dpy){ }

VaapiSurfaceAllocator::~VaapiSurfaceAllocator(){
    if(m_surfaces.size()){
        free();
    }
 }

bool VaapiSurfaceAllocator::alloc(const VaapiSurfaceAllocatorConfig& config, VASurfaceID surfaces[]){
    if(!m_dpy){
        std::cout<<"VA Display unset. Unable to allocate surfaces"<<std::endl;
        return false;
    }

    VASurfaceAttrib fourcc;
    fourcc.type =VASurfaceAttribPixelFormat;
    fourcc.flags=VA_SURFACE_ATTRIB_SETTABLE;
    fourcc.value.type=VAGenericValueTypeInteger;
    fourcc.value.value.i=VA_FOURCC_NV12; //currently we only consider nv12

    VAStatus va_status = vaCreateSurfaces(*m_dpy, config.surfaceType, config.width, config.height, 
                                 &surfaces[0], config.surfaceNum, &fourcc, 1);
    if(va_status != VA_STATUS_SUCCESS){
        std::cout<<"Failed to allocate surfaces: "<<va_status<<std::endl;
        return false;
    }

    // va_status = vaCreateContext(*m_dpy,)

    if(m_surfaces.capacity() < m_surfaces.size()+ config.surfaceNum){
        m_surfaces.reserve(m_surfaces.size()+ config.surfaceNum);
    }
    for(std::size_t i =0; i<config.surfaceNum;++i){
        m_surfaces.push_back(surfaces[i]);
    }
    return true;
    
}

void VaapiSurfaceAllocator::free(){
    for(std::vector<VASurfaceID>::iterator it = m_surfaces.begin(); it != m_surfaces.end(); ++it){
        vaDestroySurfaces(*m_dpy,&(*it),1);
    }
    std::vector<VASurfaceID>().swap(m_surfaces);
}

bool VaapiSurfaceAllocator::getSurfacesAddr(VASurfaceID*& surfAddr){
    if(m_surfaces.empty())
        return false;

    surfAddr = &(m_surfaces[0]);
    return true;
}

SurfacePool::SurfacePool(VADisplay* dpy, VaapiSurfaceAllocator* allocator): m_dpy(dpy),m_allocator(allocator),
        m_freeSurfaces(nullptr), m_usedSurfaces(nullptr){ }

SurfacePool::~SurfacePool(){ }

bool SurfacePool::init(VaapiSurfaceAllocator::Config& config){
    std::lock_guard<std::mutex> lg(m_mutex);    
    if(!m_dpy){
        std::cout<<"VA Display unset. Unable to initialize surface pool"<<std::endl;
        return false;
    }
    if(!m_allocator){
        std::cout<<"VAAPI Allocator unset.Unable to initialize surface pool"<<std::endl;
        return false;
    }

    if(!config.surfaceNum){
        std::cout<<"Requested number of surface is 0"<<std::endl;
        return true;
    }

    VASurfaceID surfaces[config.surfaceNum];
    if(m_allocator->alloc(config, surfaces)){
        for(std::size_t i = 0; i < config.surfaceNum; ++i){
            Surface* newItem = new Surface{surfaces[i], i, m_freeSurfaces};
            m_freeSurfaces = newItem;
        }
        return true;
    }
    else{
        std::cout<<"Fail to allocate surfaces in surface pool"<<std::endl;
        return false;
    }
}

bool SurfacePool::getFreeSurfaceUnsafe(SurfacePool::Surface** surface){
    *surface = m_freeSurfaces;
    m_freeSurfaces = (*surface)->next;
    (*surface)->next = nullptr;
    return true;
}

bool SurfacePool::getFreeSurface(SurfacePool::Surface** surface){
    std::unique_lock<std::mutex> lk(m_mutex);
    m_cv.wait(lk, [&]{return m_freeSurfaces!=nullptr;});
    getFreeSurfaceUnsafe(surface);
    lk.unlock();
    return true;
}

bool SurfacePool::tryGetFreeSurface(SurfacePool::Surface** surface){
    std::lock_guard<std::mutex> lg(m_mutex);
    if(m_freeSurfaces){
        return getFreeSurfaceUnsafe(surface);
    }
    else
        return false;
}

bool SurfacePool::moveToUsed(SurfacePool::Surface** surface){
    if(!*surface){
        return false;
    }
    std::lock_guard<std::mutex> lg(m_usedListMutex);
    return moveToUsedUnsafe(surface);
}

bool SurfacePool::moveToUsedUnsafe(SurfacePool::Surface** surface){
    (*surface)->next = m_usedSurfaces;
    m_usedSurfaces = *surface;
    return true;
}

bool SurfacePool::getUsedSurface(SurfacePool::Surface** surface){
    std::lock_guard<std::mutex> lg(m_usedListMutex);
    return getUsedSurfaceUnsafe(surface);
}

bool SurfacePool::getUsedSurfaceUnsafe(SurfacePool::Surface** surface){
    if(!m_usedSurfaces){
        surface = nullptr;
        return true;
    }
    else{
        *surface = m_usedSurfaces;
        m_usedSurfaces = (*surface)->next;
        (*surface)->next = nullptr;
        return true;
    }
}

bool SurfacePool::moveToFree(Surface** surface){
    if(!*surface){
        return false;
    }
    std::lock_guard<std::mutex> lg(m_mutex);
    return moveToFreeUnsafe(surface);
}

bool SurfacePool::moveToFreeUnsafe(Surface** surface){
    (*surface)->next = m_freeSurfaces;
    m_freeSurfaces = *surface;
    return true;
}


JpegEncNode::JpegEncNode(std::size_t inPortNum, std::size_t outPortNum, std::size_t totalThreadNum):
        hva::hvaNode_t(inPortNum, outPortNum, totalThreadNum), m_ready(false), m_picHeight(0u), m_picWidth(0u){
    if(!initVaapi()){
        std::cout<<"Vaapi init failed"<<std::endl;
        return;
    }
    m_allocator = new VaapiSurfaceAllocator(&m_vaDpy); //not allocated any surfaces yet
    m_pool = new SurfacePool(&m_vaDpy, m_allocator); //not init yet
    m_picPool = new JpegEncPicture[16];
    memset(m_picPool, 0, sizeof(JpegEncPicture)*16);
}

JpegEncNode::~JpegEncNode(){
    if(!m_allocator)
        delete m_allocator;

    if(!m_pool)
        delete m_pool;

    if(!m_picPool)
        delete[] m_picPool;

    vaDestroyConfig(m_vaDpy,m_jpegConfigId);
    vaTerminate(m_vaDpy);
    va_close_display(m_vaDpy);
}

std::shared_ptr<hva::hvaNodeWorker_t> JpegEncNode::createNodeWorker() const {
    return std::shared_ptr<hva::hvaNodeWorker_t>(new JpegEncNodeWorker((JpegEncNode*)this) );
}

bool JpegEncNode::initVaapi(){

    /* 1. Initialize the va driver */
    m_vaDpy = va_open_display();

    VAStatus va_status = vaInitialize(m_vaDpy, &m_vaMajorVer, &m_vaMinorVer);
    if(va_status != VA_STATUS_SUCCESS){
        std::cout<<"VaInitialize failed"<<std::endl;
        return false;
    }

    /* 2. Query for the entrypoints for the JPEGBaseline profile */
    VAEntrypoint entrypoints[5];
    int num_entrypoints = -1;
    va_status = vaQueryConfigEntrypoints(m_vaDpy, VAProfileJPEGBaseline, entrypoints, &num_entrypoints);
    if(va_status != VA_STATUS_SUCCESS){
        std::cout<<"vaQueryConfigEntrypoints failed"<<std::endl;
        return false;
    }    // We need picture level encoding (VAEntrypointEncPicture). Find if it is supported. 
    int enc_entrypoint = 0;
    for(; enc_entrypoint < num_entrypoints; enc_entrypoint++) {
        if (entrypoints[enc_entrypoint] == VAEntrypointEncPicture)
            break;
    }
    if (enc_entrypoint == num_entrypoints) {
        /* No JPEG Encode (VAEntrypointEncPicture) entry point found */
        std::cout<<"No Jpeg entry point found"<<std::endl;
        return false;
    }
    
    /* 3. Query for the Render Target format supported */
    VAConfigAttrib attrib[2];
    attrib[0].type = VAConfigAttribRTFormat;
    attrib[1].type = VAConfigAttribEncJPEG;
    vaGetConfigAttributes(m_vaDpy, VAProfileJPEGBaseline, VAEntrypointEncPicture, &attrib[0], 2);

    // RT should be one of below.
    if(!((attrib[0].value & VA_RT_FORMAT_YUV420) || (attrib[0].value & VA_RT_FORMAT_YUV422) || (attrib[0].value & VA_RT_FORMAT_RGB32)
        ||(attrib[0].value & VA_RT_FORMAT_YUV444) || (attrib[0].value & VA_RT_FORMAT_YUV400))) 
    {
        /* Did not find the supported RT format */
        std::cout<<"No RT format supported found"<<std::endl;
        return false;     
    }

    VAConfigAttribValEncJPEG jpeg_attrib_val;
    jpeg_attrib_val.value = attrib[1].value;

    /* Set JPEG profile attribs */
    jpeg_attrib_val.bits.arithmatic_coding_mode = 0;
    jpeg_attrib_val.bits.progressive_dct_mode = 0;
    jpeg_attrib_val.bits.non_interleaved_mode = 1;
    jpeg_attrib_val.bits.differential_mode = 0;

    attrib[1].value = jpeg_attrib_val.value;
    
    /* 4. Create Config for the profile=VAProfileJPEGBaseline, entrypoint=VAEntrypointEncPicture,
     * with RT format attribute */
    va_status = vaCreateConfig(m_vaDpy, VAProfileJPEGBaseline, VAEntrypointEncPicture, 
                               &attrib[0], 2, &m_jpegConfigId);
    if(va_status != VA_STATUS_SUCCESS){
        std::cout<<"VA Create Config failed"<<std::endl;
        return false;
    }
    return true;
}

bool JpegEncNode::initVaJpegCtx(){
    VASurfaceID* surfAddr = nullptr;
    m_allocator->getSurfacesAddr(surfAddr);
    if(!m_vaDpy || !m_jpegConfigId){
        std::cout<<"VA display or va config unset!"<<std::endl;
        return false;
    }
    if(!m_picWidth || !m_picHeight){
        std::cout<<"Pic Width and Pic Height unset!"<<std::endl;
        return false;
    }
    VAStatus va_status = vaCreateContext(m_vaDpy, m_jpegConfigId, m_picWidth, m_picHeight, 
                                VA_PROGRESSIVE, surfAddr, 16, &m_jpegCtxId);
    if(va_status != VA_STATUS_SUCCESS){
        std::cout<<"Create va context failed!"<<std::endl;
        return false;
    }
    return true;
}


JpegEncNodeWorker::JpegEncNodeWorker(hva::hvaNode_t* parentNode):
        hva::hvaNodeWorker_t(parentNode){
    m_jpegCtr.store(0);
}

void JpegEncNodeWorker::init(){

}

struct InfoROI_t {
    int widthImage = 0;
    int heightImage = 0;
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
    int indexROI = 0;
    int totalROINum = 0;
    int frameId = 0;
};

void JpegEncNodeWorker::process(std::size_t batchIdx){
    std::vector<std::shared_ptr<hva::hvaBlob_t>> vInput= hvaNodeWorker_t::getParentPtr()->getBatchedInput(batchIdx, std::vector<size_t> {0});
    if(vInput.size()==0u){
        // last time before stop fetches nothing
        SurfacePool::Surface* usedSurface = nullptr;
        ((JpegEncNode*)m_parentNode)->m_pool->getUsedSurface(&usedSurface);
        if(!usedSurface){
            std::cout<<"ERROR! No free surfaces and no used surfaces!" <<std::endl;
            return;
        }

        do{
            if(!saveToFile(usedSurface)){
                std::cout<<"Failed to save jpeg tagged "<<m_jpegCtr.load()<<std::endl;
            }

            ((JpegEncNode*)m_parentNode)->m_pool->moveToFree(&usedSurface);

            ((JpegEncNode*)m_parentNode)->m_pool->getUsedSurface(&usedSurface);
        }while(usedSurface);

        return;
    }
    if(!((JpegEncNode*)m_parentNode)->m_ready){
        InfoROI_t* meta = vInput[0]->get<unsigned char, InfoROI_t>(0)->getMeta();
        if(!meta->widthImage || !meta->heightImage){
            std::cout<<"Picture width or height unset!"<<std::endl;
            return;
        }
        else{
            VaapiSurfaceAllocator::Config config = {VA_RT_FORMAT_YUV420, 0u, 0u, 16u}; //To-do: make surfaces num virable
            ((JpegEncNode*)m_parentNode)->m_picWidth = config.width = meta->widthImage;
            ((JpegEncNode*)m_parentNode)->m_picHeight = config.height = meta->heightImage;
            if(!((JpegEncNode*)m_parentNode)->m_pool->init(config)){
                std::cout<<"Surface pool init failed!"<<std::endl;
                return;
            }
            VASurfaceID* surfAddr = nullptr;
            if(!((JpegEncNode*)m_parentNode)->initVaJpegCtx()){
                std::cout<<"Init jpeg context failed!"<<std::endl;
                return;
            }
            ((JpegEncNode*)m_parentNode)->m_ready = true;
        }
    }

    SurfacePool::Surface* surface = nullptr;
    if(((JpegEncNode*)m_parentNode)->m_pool->tryGetFreeSurface(&surface)){
        VASurfaceID vaSurf = surface->surfaceId;
        if(!vaSurf){
            std::cout<<"Surface got is invalid!"<<std::endl;
        }
        const unsigned char* img = vInput[0]->get<unsigned char, std::pair<unsigned, unsigned>>(1)->getPtr();
        if(!prepareSurface(vaSurf, img)){
            std::cout<<"Fail to prepare input surface!"<<std::endl;
            return;
        }
        unsigned picWidth = ((JpegEncNode*)m_parentNode)->m_picWidth;
        unsigned picHeight = ((JpegEncNode*)m_parentNode)->m_picHeight;
        JpegEncPicture* picPool = ((JpegEncNode*)m_parentNode)->m_picPool;
        std::size_t index = surface->index;
        // Encoded buffer
        VAStatus va_status = vaCreateBuffer(((JpegEncNode*)m_parentNode)->m_vaDpy, ((JpegEncNode*)m_parentNode)->m_jpegCtxId,
                VAEncCodedBufferType, picWidth*picHeight*3/2, 1, NULL, &(picPool[index].codedBufId));
        if(va_status != VA_STATUS_SUCCESS){
            std::cout<<"Create coded buffer failed!"<<std::endl;
            return;
        }

        // Picture parameter buffer
        VAEncPictureParameterBufferJPEG pic_param;
        memset(&pic_param, 0, sizeof(pic_param));
        pic_param.coded_buf = picPool[index].codedBufId;
        jpegenc_pic_param_init(&pic_param, picWidth, picHeight, 50); //to-do: make quality virable
        va_status = vaCreateBuffer(((JpegEncNode*)m_parentNode)->m_vaDpy, ((JpegEncNode*)m_parentNode)->m_jpegCtxId,
                VAEncPictureParameterBufferType, sizeof(VAEncPictureParameterBufferJPEG), 1, &pic_param, &(picPool[index].picParamBufId));
        if(va_status != VA_STATUS_SUCCESS){
            std::cout<<"Create picture param failed!"<<std::endl;
            return;
        }

        //Quantization matrix
        va_status = vaCreateBuffer(((JpegEncNode*)m_parentNode)->m_vaDpy, ((JpegEncNode*)m_parentNode)->m_jpegCtxId,
                VAQMatrixBufferType, sizeof(VAQMatrixBufferJPEG), 1, Defaults::getDefaults().qMatrix(), &(picPool[index].qMatrixBufId));
        if(va_status != VA_STATUS_SUCCESS){
            std::cout<<"Create quantization matrix failed!"<<std::endl;
            return;
        }

        //Huffman table
        va_status = vaCreateBuffer(((JpegEncNode*)m_parentNode)->m_vaDpy, ((JpegEncNode*)m_parentNode)->m_jpegCtxId, VAHuffmanTableBufferType, 
                               sizeof(VAHuffmanTableBufferJPEGBaseline), 1, Defaults::getDefaults().huffmanTbl(), &(picPool[index].huffmanTblBufId));
        if(va_status != VA_STATUS_SUCCESS){
            std::cout<<"Create huffman table failed!"<<std::endl;
            return;
        }

        //Slice parameter
        va_status = vaCreateBuffer(((JpegEncNode*)m_parentNode)->m_vaDpy, ((JpegEncNode*)m_parentNode)->m_jpegCtxId, VAEncSliceParameterBufferType, 
                            sizeof(VAEncSliceParameterBufferJPEG), 1, Defaults::getDefaults().sliceParam(), &(picPool[index].sliceParamBufId));
        if(va_status != VA_STATUS_SUCCESS){
            std::cout<<"Create slice parameter failed!"<<std::endl;
            return;
        }

        // packed headers
        VAEncPackedHeaderParameterBuffer packed_header_param_buffer;
        unsigned int length_in_bits;
        unsigned char *packed_header_buffer = NULL;

        //to-do: currently set quality to 50
        length_in_bits = build_packed_jpeg_header_buffer(&packed_header_buffer, picWidth, picHeight, Defaults::getDefaults().sliceParam()->restart_interval, 50);
        packed_header_param_buffer.type = VAEncPackedHeaderRawData;
        packed_header_param_buffer.bit_length = length_in_bits;
        packed_header_param_buffer.has_emulation_bytes = 0;
        
        /* 11. Create raw buffer for header */
        va_status = vaCreateBuffer(((JpegEncNode*)m_parentNode)->m_vaDpy,
                                ((JpegEncNode*)m_parentNode)->m_jpegCtxId,
                                VAEncPackedHeaderParameterBufferType,
                                sizeof(packed_header_param_buffer), 1, &packed_header_param_buffer,
                                &(picPool[index].headerParamBufId));
        if(va_status != VA_STATUS_SUCCESS){
            std::cout<<"Create header parameter buffer failed!"<<std::endl;
            return;
        }

        va_status = vaCreateBuffer(((JpegEncNode*)m_parentNode)->m_vaDpy,
                                ((JpegEncNode*)m_parentNode)->m_jpegCtxId,
                                VAEncPackedHeaderDataBufferType,
                                (length_in_bits + 7) / 8, 1, packed_header_buffer,
                                &(picPool[index].headerDataBufId));
        if(va_status != VA_STATUS_SUCCESS){
            std::cout<<"Create header buffer failed!"<<std::endl;
            return;
        }

        // begine encoding
        va_status = vaBeginPicture(((JpegEncNode*)m_parentNode)->m_vaDpy, ((JpegEncNode*)m_parentNode)->m_jpegCtxId, vaSurf);
        if(va_status != VA_STATUS_SUCCESS){
            std::cout<<"Begin picture failed!"<<std::endl;
            return;
        }

        va_status = vaRenderPicture(((JpegEncNode*)m_parentNode)->m_vaDpy,((JpegEncNode*)m_parentNode)->m_jpegCtxId, &(picPool[index].picParamBufId), 1);
        if(va_status != VA_STATUS_SUCCESS){
            std::cout<<"Render picture failed!"<<std::endl;
            return;
        }
    
        va_status = vaRenderPicture(((JpegEncNode*)m_parentNode)->m_vaDpy,((JpegEncNode*)m_parentNode)->m_jpegCtxId, &(picPool[index].qMatrixBufId), 1);
        if(va_status != VA_STATUS_SUCCESS){
            std::cout<<"Render picture failed!"<<std::endl;
            return;
        }

        va_status = vaRenderPicture(((JpegEncNode*)m_parentNode)->m_vaDpy,((JpegEncNode*)m_parentNode)->m_jpegCtxId, &(picPool[index].huffmanTblBufId), 1);
        if(va_status != VA_STATUS_SUCCESS){
            std::cout<<"Render picture failed!"<<std::endl;
            return;
        }
    
        va_status = vaRenderPicture(((JpegEncNode*)m_parentNode)->m_vaDpy,((JpegEncNode*)m_parentNode)->m_jpegCtxId, &(picPool[index].sliceParamBufId), 1);
        if(va_status != VA_STATUS_SUCCESS){
            std::cout<<"Render picture failed!"<<std::endl;
            return;
        }
        
        va_status = vaRenderPicture(((JpegEncNode*)m_parentNode)->m_vaDpy,((JpegEncNode*)m_parentNode)->m_jpegCtxId, &(picPool[index].headerParamBufId), 1);
        if(va_status != VA_STATUS_SUCCESS){
            std::cout<<"Render picture failed!"<<std::endl;
            return;
        }
        
        va_status = vaRenderPicture(((JpegEncNode*)m_parentNode)->m_vaDpy,((JpegEncNode*)m_parentNode)->m_jpegCtxId, &(picPool[index].headerDataBufId), 1);
        if(va_status != VA_STATUS_SUCCESS){
            std::cout<<"Render picture failed!"<<std::endl;
            return;
        }
        
        va_status = vaEndPicture(((JpegEncNode*)m_parentNode)->m_vaDpy,((JpegEncNode*)m_parentNode)->m_jpegCtxId);
        if(va_status != VA_STATUS_SUCCESS){
            std::cout<<"End picture failed!"<<std::endl;
            return;
        }

        ((JpegEncNode*)m_parentNode)->m_pool->moveToUsed(&surface);
        surface = nullptr;

    }
    else{
        SurfacePool::Surface* usedSurface = nullptr;
        ((JpegEncNode*)m_parentNode)->m_pool->getUsedSurface(&usedSurface);
        if(!usedSurface){
            std::cout<<"ERROR! No free surfaces and no used surfaces!" <<std::endl;
            return;
        }

        do{
            if(!saveToFile(usedSurface)){
                std::cout<<"Failed to save jpeg tagged "<<m_jpegCtr.load()<<std::endl;
            }

            ((JpegEncNode*)m_parentNode)->m_pool->moveToFree(&usedSurface);

            ((JpegEncNode*)m_parentNode)->m_pool->getUsedSurface(&usedSurface);
        }while(usedSurface);
    }

}

bool JpegEncNodeWorker::saveToFile(SurfacePool::Surface* surface){
    VASurfaceStatus surface_status = (VASurfaceStatus) 0;
    VACodedBufferSegment *coded_buffer_segment;
    std::size_t index = surface->index;
    JpegEncPicture* picPool = ((JpegEncNode*)m_parentNode)->m_picPool;

    VAStatus va_status = vaSyncSurface(((JpegEncNode*)m_parentNode)->m_vaDpy, surface->surfaceId);
    if(va_status != VA_STATUS_SUCCESS){
        std::cout<<"Sync surface failed!"<<std::endl;
        return false;
    }

    va_status = vaQuerySurfaceStatus(((JpegEncNode*)m_parentNode)->m_vaDpy, surface->surfaceId, &surface_status);
    if(va_status != VA_STATUS_SUCCESS){
        std::cout<<"Query surface status failed!"<<std::endl;
        return false;
    }

    va_status = vaMapBuffer(((JpegEncNode*)m_parentNode)->m_vaDpy, picPool[index].codedBufId, (void **)(&coded_buffer_segment));
    if(va_status != VA_STATUS_SUCCESS){
        std::cout<<"Map coded buffer failed!"<<std::endl;
        return false;
    }

    if (coded_buffer_segment->status & VA_CODED_BUF_STATUS_SLICE_OVERFLOW_MASK) {
        vaUnmapBuffer(((JpegEncNode*)m_parentNode)->m_vaDpy, picPool[index].codedBufId);
        std::cout<<"ERROR......Coded buffer too small"<<std::endl;
        return false;
    }

    int slice_data_length = coded_buffer_segment->size;
    std::size_t w_items = 0;
    std::stringstream ss;
    ss << "jpegenc"<< m_jpegCtr.fetch_add(1);
    FILE* jpeg_fp = fopen(ss.str().c_str(), "wb");  
    do {
        w_items = fwrite(coded_buffer_segment->buf, slice_data_length, 1, jpeg_fp);
    } while (w_items != 1);

    fclose(jpeg_fp);

    va_status = vaUnmapBuffer(((JpegEncNode*)m_parentNode)->m_vaDpy, picPool[index].codedBufId);
    if(va_status != VA_STATUS_SUCCESS){
        std::cout<<"Unmap coded buffer failed!"<<std::endl;
        return false;
    }
    return true;
}

int JpegEncNodeWorker::build_packed_jpeg_header_buffer(unsigned char **header_buffer, int picture_width, int picture_height, uint16_t restart_interval, int quality)
{
    const unsigned num_components = 3;

    bitstream bs;
    int i=0, j=0;
    uint32_t temp=0;
    
    bitstream_start(&bs);
    
    //Add SOI
    bitstream_put_ui(&bs, SOI, 16);
    
    //Add AppData
    bitstream_put_ui(&bs, APP0, 16);  //APP0 marker
    bitstream_put_ui(&bs, 16, 16);    //Length excluding the marker
    bitstream_put_ui(&bs, 0x4A, 8);   //J
    bitstream_put_ui(&bs, 0x46, 8);   //F
    bitstream_put_ui(&bs, 0x49, 8);   //I
    bitstream_put_ui(&bs, 0x46, 8);   //F
    bitstream_put_ui(&bs, 0x00, 8);   //0
    bitstream_put_ui(&bs, 1, 8);      //Major Version
    bitstream_put_ui(&bs, 1, 8);      //Minor Version
    bitstream_put_ui(&bs, 1, 8);      //Density units 0:no units, 1:pixels per inch, 2: pixels per cm
    bitstream_put_ui(&bs, 72, 16);    //X density
    bitstream_put_ui(&bs, 72, 16);    //Y density
    bitstream_put_ui(&bs, 0, 8);      //Thumbnail width
    bitstream_put_ui(&bs, 0, 8);      //Thumbnail height

    // Regarding Quantization matrices: As per JPEG Spec ISO/IEC 10918-1:1993(E), Pg-19:
    // "applications may specify values which customize picture quality for their particular
    // image characteristics, display devices, and viewing conditions"


    //Normalization of quality factor
    quality = (quality < 50) ? (5000/quality) : (200 - (quality*2));
    
    //Add QTable - Y
    JPEGQuantSection quantLuma;
    Defaults::getDefaults().populate_quantdata(&quantLuma, 0);

    bitstream_put_ui(&bs, quantLuma.DQT, 16);
    bitstream_put_ui(&bs, quantLuma.Lq, 16);
    bitstream_put_ui(&bs, quantLuma.Pq, 4);
    bitstream_put_ui(&bs, quantLuma.Tq, 4);
    for(i=0; i<NUM_QUANT_ELEMENTS; i++) {
        //scale the quantization table with quality factor
        temp = (quantLuma.Qk[i] * quality)/100;
        //clamp to range [1,255]
        temp = (temp > 255) ? 255 : temp;
        temp = (temp < 1) ? 1 : temp;
        quantLuma.Qk[i] = (unsigned char)temp;
        bitstream_put_ui(&bs, quantLuma.Qk[i], 8);
    }

    //Add QTable - U/V
    if(1){//if(yuvComp.fourcc_val != VA_FOURCC_Y800) {
        JPEGQuantSection quantChroma;
        Defaults::getDefaults().populate_quantdata(&quantChroma, 1);
        
        bitstream_put_ui(&bs, quantChroma.DQT, 16);
        bitstream_put_ui(&bs, quantChroma.Lq, 16);
        bitstream_put_ui(&bs, quantChroma.Pq, 4);
        bitstream_put_ui(&bs, quantChroma.Tq, 4);
        for(i=0; i<NUM_QUANT_ELEMENTS; i++) {
            //scale the quantization table with quality factor
            temp = (quantChroma.Qk[i] * quality)/100;
            //clamp to range [1,255]
            temp = (temp > 255) ? 255 : temp;
            temp = (temp < 1) ? 1 : temp;
            quantChroma.Qk[i] = (unsigned char)temp;
            bitstream_put_ui(&bs, quantChroma.Qk[i], 8);
        }
    }
    
    //Add FrameHeader
    JPEGFrameHeader frameHdr;
    memset(&frameHdr,0,sizeof(JPEGFrameHeader));
    Defaults::getDefaults().populate_frame_header(&frameHdr, picture_width, picture_height);

    bitstream_put_ui(&bs, frameHdr.SOF, 16);
    bitstream_put_ui(&bs, frameHdr.Lf, 16);
    bitstream_put_ui(&bs, frameHdr.P, 8);
    bitstream_put_ui(&bs, frameHdr.Y, 16);
    bitstream_put_ui(&bs, frameHdr.X, 16);
    bitstream_put_ui(&bs, frameHdr.Nf, 8);
    for(i=0; i<frameHdr.Nf;i++) {
        bitstream_put_ui(&bs, frameHdr.JPEGComponent[i].Ci, 8);
        bitstream_put_ui(&bs, frameHdr.JPEGComponent[i].Hi, 4);
        bitstream_put_ui(&bs, frameHdr.JPEGComponent[i].Vi, 4);
        bitstream_put_ui(&bs, frameHdr.JPEGComponent[i].Tqi, 8);
    }

    //Add HuffTable AC and DC for Y,U/V components
    JPEGHuffSection acHuffSectionHdr, dcHuffSectionHdr;
        
    for(i=0; (i<num_components && (i<=1)); i++) {
        //Add DC component (Tc = 0)
        Defaults::getDefaults().populate_huff_section_header(&dcHuffSectionHdr, i, 0); 
        
        bitstream_put_ui(&bs, dcHuffSectionHdr.DHT, 16);
        bitstream_put_ui(&bs, dcHuffSectionHdr.Lh, 16);
        bitstream_put_ui(&bs, dcHuffSectionHdr.Tc, 4);
        bitstream_put_ui(&bs, dcHuffSectionHdr.Th, 4);
        for(j=0; j<NUM_DC_RUN_SIZE_BITS; j++) {
            bitstream_put_ui(&bs, dcHuffSectionHdr.Li[j], 8);
        }
        
        for(j=0; j<NUM_DC_CODE_WORDS_HUFFVAL; j++) {
            bitstream_put_ui(&bs, dcHuffSectionHdr.Vij[j], 8);
        }

        //Add AC component (Tc = 1)
        Defaults::getDefaults().populate_huff_section_header(&acHuffSectionHdr, i, 1);
        
        bitstream_put_ui(&bs, acHuffSectionHdr.DHT, 16);
        bitstream_put_ui(&bs, acHuffSectionHdr.Lh, 16);
        bitstream_put_ui(&bs, acHuffSectionHdr.Tc, 4);
        bitstream_put_ui(&bs, acHuffSectionHdr.Th, 4);
        for(j=0; j<NUM_AC_RUN_SIZE_BITS; j++) {
            bitstream_put_ui(&bs, acHuffSectionHdr.Li[j], 8);
        }

        for(j=0; j<NUM_AC_CODE_WORDS_HUFFVAL; j++) {
            bitstream_put_ui(&bs, acHuffSectionHdr.Vij[j], 8);
        }

        // if(yuvComp.fourcc_val == VA_FOURCC_Y800)
        //     break;
    }
    
    //Add Restart Interval if restart_interval is not 0
    if(restart_interval != 0) {
        JPEGRestartSection restartHdr;
        restartHdr.DRI = DRI;
        restartHdr.Lr = 4;
        restartHdr.Ri = restart_interval;

        bitstream_put_ui(&bs, restartHdr.DRI, 16); 
        bitstream_put_ui(&bs, restartHdr.Lr, 16);
        bitstream_put_ui(&bs, restartHdr.Ri, 16); 
    }
    
    //Add ScanHeader
    JPEGScanHeader scanHdr;
    Defaults::getDefaults().populate_scan_header(&scanHdr);
 
    bitstream_put_ui(&bs, scanHdr.SOS, 16);
    bitstream_put_ui(&bs, scanHdr.Ls, 16);
    bitstream_put_ui(&bs, scanHdr.Ns, 8);
    
    for(i=0; i<scanHdr.Ns; i++) {
        bitstream_put_ui(&bs, scanHdr.ScanComponent[i].Csj, 8);
        bitstream_put_ui(&bs, scanHdr.ScanComponent[i].Tdj, 4);
        bitstream_put_ui(&bs, scanHdr.ScanComponent[i].Taj, 4);
    }

    bitstream_put_ui(&bs, scanHdr.Ss, 8);
    bitstream_put_ui(&bs, scanHdr.Se, 8);
    bitstream_put_ui(&bs, scanHdr.Ah, 4);
    bitstream_put_ui(&bs, scanHdr.Al, 4);

    bitstream_end(&bs);
    *header_buffer = (unsigned char *)bs.buffer;
    
    return bs.bit_offset;
}


void JpegEncNodeWorker::jpegenc_pic_param_init(VAEncPictureParameterBufferJPEG *pic_param,int width,int height,int quality){
    pic_param->picture_width = width;
    pic_param->picture_height = height;
    pic_param->quality = quality;
    
    pic_param->pic_flags.bits.profile = 0;      //Profile = Baseline
    pic_param->pic_flags.bits.progressive = 0;  //Sequential encoding
    pic_param->pic_flags.bits.huffman = 1;      //Uses Huffman coding
    pic_param->pic_flags.bits.interleaved = 0;  //Input format is interleaved (YUV)
    pic_param->pic_flags.bits.differential = 0; //non-Differential Encoding
    
    pic_param->sample_bit_depth = 8; //only 8 bit sample depth is currently supported
    pic_param->num_scan = 1;
    pic_param->num_components = 3; // Supporting only upto 3 components maximum
    //set component_id Ci and Tqi
    
    //to-do: the conf below does not consider all circumstances
    pic_param->component_id[0] = pic_param->quantiser_table_selector[0] = 0;
    pic_param->component_id[1] = pic_param->quantiser_table_selector[1] = 1;
    pic_param->component_id[2] = 2; pic_param->quantiser_table_selector[2] = 1;
    
    pic_param->quality = quality;
}

bool JpegEncNodeWorker::prepareSurface(VASurfaceID surface, const unsigned char* img){
    if(!surface || !img){
        std::cout<<"Invalid input. Fail to prepare input surface"<<std::endl;
        return false;
    }
    VAImage vaImg;
    VAStatus va_status = vaDeriveImage(((JpegEncNode*)m_parentNode)->m_vaDpy, surface, &vaImg);
    if(va_status != VA_STATUS_SUCCESS){
        std::cout<<"Fail to derive image."<<std::endl;
        return false;
    }
    void* mappedSurf = nullptr;
    va_status = vaMapBuffer(((JpegEncNode*)m_parentNode)->m_vaDpy,vaImg.buf, &mappedSurf);
    if(va_status != VA_STATUS_SUCCESS){
        std::cout<<"Fail to map image."<<std::endl;
        return false;
    }
    unsigned picWidth = ((JpegEncNode*)m_parentNode)->m_picWidth;
    unsigned picHeight = ((JpegEncNode*)m_parentNode)->m_picHeight;
    // unsigned offset = picWidth * alignTo(picHeight);
    memcpy(mappedSurf, img, (unsigned)(picWidth*picHeight*3/2)); // to-do: currently no alignment
    // memcpy(mappedSurf+offset, img+offset, (unsigned)(picWidth * picHeight /2));
    vaUnmapBuffer(((JpegEncNode*)m_parentNode)->m_vaDpy,vaImg.buf);
    vaDestroyImage(((JpegEncNode*)m_parentNode)->m_vaDpy,vaImg.image_id);
}