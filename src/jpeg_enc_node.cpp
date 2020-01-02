#include <jpeg_enc_node.hpp>
#include <va_common/va_display.h>

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
    for(std::vector<VASurfaceID>::iterator it = m_surfaces.begin(); it != m_surfaces.end(); ++i){
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

bool SurfacePool::getFreeSurfaceUnsafe(SurfacePool::Surface*& surface){
    surface = m_freeSurfaces;
    m_freeSurfaces = surface->next;
    surface->next = nullptr;
    return true;
}

bool SurfacePool::getFreeSurface(SurfacePool::Surface*& surface){
    std::unique_lock<std::mutex> lk(m_mutex);
    m_cv.wait(lk, [&]{return m_freeSurfaces!=nullptr;});
    getFreeSurfaceUnsafe(surface);
    lk.unlock();
    return true;
}

bool SurfacePool::tryGetFreeSurface(SurfacePool::Surface*& surface){
    std::lock_guard<std::mutex> lg(m_mutex);
    if(m_freeSurfaces){
        return getFreeSurfaceUnsafe(surface);
    }
    else
        return false;
}

bool SurfacePool::moveToUsed(SurfacePool::Surface*& surface){
    std::lock_guard<std::mutex> lg(m_usedListMutex);
    return moveToUsedUnsafe(surface);
}

bool SurfacePool::moveToUsedUnsafe(SurfacePool::Surface*& surface){
    surface->next = m_usedSurfaces;
    m_usedSurfaces = surface;
    return true;
}

bool SurfacePool::getUsedSurface(SurfacePool::Surface*& surface){
    std::lock_guard<std::mutex> lg(m_usedListMutex);
    return getUsedSurfaceUnsafe(surface);
}

bool SurfacePool::getUsedSurfaceUnsafe(SurfacePool::Surface*& surface){
    if(!m_usedSurfaces){
        surface = nullptr;
        return true;
    }
    else{
        surface = m_usedSurfaces;
        m_usedSurfaces = surface->next;
        surface->next = nullptr;
        return true;
    }
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
        //to-do: do usedsufaces sync here
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
    if(((JpegEncNode*)m_parentNode)->m_pool->tryGetFreeSurface(surface)){
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
                VAQMatrixBufferType, sizeof(VAQMatrixBufferJPEG), 1, Defaults::getDefaults().qMatrix, &(picPool[index].qMatrixBufId));
        if(va_status != VA_STATUS_SUCCESS){
            std::cout<<"Create quantization matrix failed!"<<std::endl;
            return;
        }


    }
    else{
        // to-do: sync on usedsurfaces
    }

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