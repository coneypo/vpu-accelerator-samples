#include <jpeg_enc_node.hpp>
#include <va_common/va_display.h>

VaapiSurfaceAllocator::VaapiSurfaceAllocator(VADisplay* dpy):m_dpy(dpy){ };

VaapiSurfaceAllocator::~VaapiSurfaceAllocator(){
    if(m_surfaces.size()){
        free();
    }
 };

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
    else{
        if(m_surfaces.capacity() < m_surfaces.size()+ config.surfaceNum){
            m_surfaces.reserve(m_surfaces.size()+ config.surfaceNum);
        }
        for(std::size_t i =0; i<config.surfaceNum;++i){
            m_surfaces.push_back(surfaces[i]);
        }
        return true;
    }
}

void VaapiSurfaceAllocator::free(){
    for(std::vector<VASurfaceID>::iterator it = m_surfaces.begin(); it != m_surfaces.end(); ++i){
        vaDestroySurfaces(*m_dpy,&(*it),1);
    }
    std::vector<VASurfaceID>().swap(m_surfaces);
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
            Surface* newItem = new Surface{surfaces[i], m_freeSurfaces};
            m_freeSurfaces = newItem;
            return true;
        }
    }
    else{
        std::cout<<"Fail to allocate surfaces in surface pool"<<std::endl;
        return false;
    }
}

bool SurfacePool::getFreeSurfaceUnsafe(SurfacePool::Surface& surface){
    surface = *m_freeSurfaces;
    m_freeSurfaces = surface.next;
    surface.next = nullptr;
    return true;
}

bool SurfacePool::getFreeSurface(SurfacePool::Surface& surface){
    std::unique_lock<std::mutex> lk(m_mutex);
    m_cv.wait(lk, [&]{return m_freeSurfaces!=nullptr;});
    getFreeSurfaceUnsafe(surface);
    lk.unlock();
    return true;
}

bool SurfacePool::tryGetFreeSurface(SurfacePool::Surface& surface){
    std::lock_guard<std::mutex> lg(m_mutex);
    if(m_freeSurfaces){
        return getFreeSurfaceUnsafe(surface);
    }
    else
        return false;
}


JpegEncNode::JpegEncNode(std::size_t inPortNum, std::size_t outPortNum, std::size_t totalThreadNum):
        hva::hvaNode_t(inPortNum, outPortNum, totalThreadNum){

}

std::shared_ptr<hva::hvaNodeWorker_t> JpegEncNode::createNodeWorker() const {
    return std::shared_ptr<hva::hvaNodeWorker_t>(new JpegEncNodeWorker((JpegEncNode*)this) );
}

JpegEncNodeWorker::JpegEncNodeWorker(hva::hvaNode_t* parentNode):
        hva::hvaNodeWorker_t(parentNode){

}

void JpegEncNodeWorker::init(){

}

bool JpegEncNode::initVaapiCallOnce(){

    /* 1. Initialize the va driver */
    m_dpy = va_open_display();

    VAStatus va_status = vaInitialize(m_vaDpy, &m_vaMajorVer, &m_vaMinorVer);
    if(va_status != VA_STATUS_SUCCESS){
        std::cout<<"VaInitialize failed"<<std::endl;
        return false;
    }

    /* 2. Query for the entrypoints for the JPEGBaseline profile */
    VAEntrypoint entrypoints[5];
    int num_entrypoints = -1;
    va_status = vaQueryConfigEntrypoints(va_dpy, VAProfileJPEGBaseline, entrypoints, &num_entrypoints);
    if(va_status != VA_STATUS_SUCCESS){
        std::cout<<"vaQueryConfigEntrypoints failed"<<std::endl;
        return false;
    }    // We need picture level encoding (VAEntrypointEncPicture). Find if it is supported. 
    int enc_entrypoint = 0
    for (; enc_entrypoint < num_entrypoints; enc_entrypoint++) {
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
    va_status = vaCreateConfig(va_dpy, VAProfileJPEGBaseline, VAEntrypointEncPicture, 
                               &attrib[0], 2, &m_jpegConfigId);
    CHECK_VASTATUS(va_status, "vaQueryConfigEntrypoints");

}
