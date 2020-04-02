#include <jpeg_enc_node.hpp>
#include <va_common/va_display.h>
#include <cstdio>
#include <unistd.h>
#include <sys/syscall.h>
//#include <WorkloadCache.h>
#include <RemoteMemory.h>
#include <fstream>
#include <va/va_drmcommon.h>

#define PIC_POOL_SIZE 2

Defaults::Defaults(){
    jpeg_luma_quant = new uint8_t[NUM_QUANT_ELEMENTS]{
        16, 11, 10, 16, 24,  40,  51,  61,
        12, 12, 14, 19, 26,  58,  60,  55,
        14, 13, 16, 24, 40,  57,  69,  56,
        14, 17, 22, 29, 51,  87,  80,  62,
        18, 22, 37, 56, 68,  109, 103, 77,
        24, 35, 55, 64, 81,  104, 113, 92,
        49, 64, 78, 87, 103, 121, 120, 101,
        72, 92, 95, 98, 112, 100, 103, 99 
    };

    jpeg_zigzag = new uint8_t[64]{
        0,   1,   8,   16,  9,   2,   3,   10,
        17,  24,  32,  25,  18,  11,  4,   5,
        12,  19,  26,  33,  40,  48,  41,  34,
        27,  20,  13,  6,   7,   14,  21,  28,
        35,  42,  49,  56,  57,  50,  43,  36,
        29,  22,  15,  23,  30,  37,  44,  51,
        58,  59,  52,  45,  38,  31,  39,  46,
        53,  60,  61,  54,  47,  55,  62,  63
    };

    jpeg_chroma_quant = new uint8_t[NUM_QUANT_ELEMENTS]{
        17, 18, 24, 47, 99, 99, 99, 99,
        18, 21, 26, 66, 99, 99, 99, 99,
        24, 26, 56, 99, 99, 99, 99, 99,
        47, 66, 99, 99, 99, 99, 99, 99,
        99, 99, 99, 99, 99, 99, 99, 99,
        99, 99, 99, 99, 99, 99, 99, 99,
        99, 99, 99, 99, 99, 99, 99, 99,
        99, 99, 99, 99, 99, 99, 99, 99
    };
    m_qMatrixParam = new VAQMatrixBufferJPEG();
    jpegenc_qmatrix_init();


    jpeg_hufftable_luma_dc = new uint8_t[29]{
        //TcTh (Tc=0 since 0:DC, 1:AC; Th=0)
        0x00,
        //Li
        0x00, 0x01, 0x05, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        //Vi
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B
    };

    jpeg_hufftable_luma_ac = new uint8_t[179]{
        //TcTh (Tc=1 since 0:DC, 1:AC; Th=0)
        0x10,
        //Li
        0x00, 0x02, 0x01, 0x03, 0x03, 0x02, 0x04, 0x03, 0x05, 0x05, 0x04, 0x04, 0x00, 0x00, 0x01, 0x7D,
        //Vi
        0x01, 0x02, 0x03, 0x00, 0x04, 0x11, 0x05, 0x12, 0x21, 0x31, 0x41, 0x06, 0x13, 0x51, 0x61, 0x07, 
        0x22, 0x71, 0x14, 0x32, 0x81, 0x91, 0xA1, 0x08, 0x23, 0x42, 0xB1, 0xC1, 0x15, 0x52, 0xD1, 0xF0, 
        0x24, 0x33, 0x62, 0x72, 0x82, 0x09, 0x0A, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x25, 0x26, 0x27, 0x28,
        0x29, 0x2A, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3A, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 
        0x4A, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5A, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 
        0x6A, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7A, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89,
        0x8A, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99, 0x9A, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7, 
        0xA8, 0xA9, 0xAA, 0xB2, 0xB3, 0xB4, 0xB5, 0xB6, 0xB7, 0xB8, 0xB9, 0xBA, 0xC2, 0xC3, 0xC4, 0xC5, 
        0xC6, 0xC7, 0xC8, 0xC9, 0xCA, 0xD2, 0xD3, 0xD4, 0xD5, 0xD6, 0xD7, 0xD8, 0xD9, 0xDA, 0xE1, 0xE2,
        0xE3, 0xE4, 0xE5, 0xE6, 0xE7, 0xE8, 0xE9, 0xEA, 0xF1, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7, 0xF8, 
        0xF9, 0xFA
    };

    jpeg_hufftable_chroma_dc = new uint8_t[29]{
        //TcTh (Tc=0 since 0:DC, 1:AC; Th=1)
        0x01,
        //Li
        0x00, 0x03, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
        //Vi
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B    
    };

    jpeg_hufftable_chroma_ac = new uint8_t[179]{
        //TcTh (Tc=1 since 0:DC, 1:AC; Th=1)
        0x11,
        //Li
        0x00, 0x02, 0x01, 0x02, 0x04, 0x04, 0x03, 0x04, 0x07, 0x05, 0x04, 0x04, 0x00, 0x01, 0x02, 0x77,
        //Vi
        0x00, 0x01, 0x02, 0x03, 0x11, 0x04, 0x05, 0x21, 0x31, 0x06, 0x12, 0x41, 0x51, 0x07, 0x61, 0x71, 
        0x13, 0x22, 0x32, 0x81, 0x08, 0x14, 0x42, 0x91, 0xA1, 0xB1, 0xC1, 0x09, 0x23, 0x33, 0x52, 0xF0, 
        0x15, 0x62, 0x72, 0xD1, 0x0A, 0x16, 0x24, 0x34, 0xE1, 0x25, 0xF1, 0x17, 0x18, 0x19, 0x1A, 0x26,
        0x27, 0x28, 0x29, 0x2A, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3A, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 
        0x49, 0x4A, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5A, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 
        0x69, 0x6A, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7A, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87,
        0x88, 0x89, 0x8A, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99, 0x9A, 0xA2, 0xA3, 0xA4, 0xA5, 
        0xA6, 0xA7, 0xA8, 0xA9, 0xAA, 0xB2, 0xB3, 0xB4, 0xB5, 0xB6, 0xB7, 0xB8, 0xB9, 0xBA, 0xC2, 0xC3, 
        0xC4, 0xC5, 0xC6, 0xC7, 0xC8, 0xC9, 0xCA, 0xD2, 0xD3, 0xD4, 0xD5, 0xD6, 0xD7, 0xD8, 0xD9, 0xDA,
        0xE2, 0xE3, 0xE4, 0xE5, 0xE6, 0xE7, 0xE8, 0xE9, 0xEA, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7, 0xF8, 
        0xF9, 0xFA
    };
    m_huffmanTbl = new VAHuffmanTableBufferJPEGBaseline();
    jpegenc_hufftable_init();

    m_sliceParam = new VAEncSliceParameterBufferJPEG();
    jpegenc_slice_param_init();
};

Defaults::~Defaults(){
    delete[] jpeg_luma_quant;
    delete[] jpeg_zigzag;
    delete[] jpeg_chroma_quant;

    delete[] jpeg_hufftable_luma_dc;
    delete[] jpeg_hufftable_luma_ac;
    delete[] jpeg_hufftable_chroma_dc;
    delete[] jpeg_hufftable_chroma_ac;

    delete m_qMatrixParam;
    delete m_huffmanTbl;
    delete m_sliceParam;
};

void Defaults::jpegenc_qmatrix_init(){
    m_qMatrixParam->load_lum_quantiser_matrix = 1;
   
    //LibVA expects the QM in zigzag order 
    for(int i=0; i<NUM_QUANT_ELEMENTS; i++) {
        m_qMatrixParam->lum_quantiser_matrix[i] = jpeg_luma_quant[jpeg_zigzag[i]];
    }
    
    //to-do: configuration below does not consider all circumstances
    m_qMatrixParam->load_chroma_quantiser_matrix = 1;
    for(int i=0; i<NUM_QUANT_ELEMENTS; i++) {
        m_qMatrixParam->chroma_quantiser_matrix[i] = jpeg_chroma_quant[jpeg_zigzag[i]];
    }
    
};

void Defaults::jpegenc_hufftable_init(){
    
    m_huffmanTbl->load_huffman_table[0] = 1; //Load Luma Hufftable
    m_huffmanTbl->load_huffman_table[1] = 1; //Load Chroma Hufftable for other formats

    //Load Luma hufftable values
    //Load DC codes
    memcpy(m_huffmanTbl->huffman_table[0].num_dc_codes, jpeg_hufftable_luma_dc+1, 16);
    //Load DC Values
    memcpy(m_huffmanTbl->huffman_table[0].dc_values, jpeg_hufftable_luma_dc+17, 12);
    //Load AC codes
    memcpy(m_huffmanTbl->huffman_table[0].num_ac_codes, jpeg_hufftable_luma_ac+1, 16);
    //Load AC Values
    memcpy(m_huffmanTbl->huffman_table[0].ac_values, jpeg_hufftable_luma_ac+17, 162);
    memset(m_huffmanTbl->huffman_table[0].pad, 0, 2);
      
   
    //Load Chroma hufftable values if needed
    //The case below does not consider all circumstances
    //Load DC codes
    memcpy(m_huffmanTbl->huffman_table[1].num_dc_codes, jpeg_hufftable_chroma_dc+1, 16);
    //Load DC Values
    memcpy(m_huffmanTbl->huffman_table[1].dc_values, jpeg_hufftable_chroma_dc+17, 12);
    //Load AC codes
    memcpy(m_huffmanTbl->huffman_table[1].num_ac_codes, jpeg_hufftable_chroma_ac+1, 16);
    //Load AC Values
    memcpy(m_huffmanTbl->huffman_table[1].ac_values, jpeg_hufftable_chroma_ac+17, 162);
    memset(m_huffmanTbl->huffman_table[1].pad, 0, 2);      
       
};

void Defaults::jpegenc_slice_param_init()
{
    m_sliceParam->restart_interval = 0;
    
    // to-do: the case below does not consider all circumstances
    m_sliceParam->num_components = 3;
    
    m_sliceParam->components[0].component_selector = 1;
    m_sliceParam->components[0].dc_table_selector = 0;
    m_sliceParam->components[0].ac_table_selector = 0;        

    m_sliceParam->components[1].component_selector = 2;
    m_sliceParam->components[1].dc_table_selector = 1;
    m_sliceParam->components[1].ac_table_selector = 1;        

    m_sliceParam->components[2].component_selector = 3;
    m_sliceParam->components[2].dc_table_selector = 1;
    m_sliceParam->components[2].ac_table_selector = 1;        
};


void Defaults::populate_quantdata(JPEGQuantSection *quantVal, int type) const
{
    uint8_t zigzag_qm[NUM_QUANT_ELEMENTS];
    int i;

    quantVal->DQT = DQT;
    quantVal->Pq = 0;
    quantVal->Tq = type;
    if(type == 0) {
        for(i=0; i<NUM_QUANT_ELEMENTS; i++) {
            zigzag_qm[i] = jpeg_luma_quant[jpeg_zigzag[i]];
        }

        memcpy(quantVal->Qk, zigzag_qm, NUM_QUANT_ELEMENTS);
    } else {
        for(i=0; i<NUM_QUANT_ELEMENTS; i++) {
            zigzag_qm[i] = jpeg_chroma_quant[jpeg_zigzag[i]];
        }
        memcpy(quantVal->Qk, zigzag_qm, NUM_QUANT_ELEMENTS);
    }
    quantVal->Lq = 3 + NUM_QUANT_ELEMENTS;
};

void Defaults::populate_frame_header(JPEGFrameHeader *frameHdr, int picture_width, int picture_height) const
{
    // to-do: the case below does not consider all circumstances
    const unsigned num_components = 3;
    const unsigned y_h_subsample = 2; 
    const unsigned y_v_subsample = 2; 
    const unsigned u_h_subsample = 1; 
    const unsigned u_v_subsample = 1; 
    const unsigned v_h_subsample = 1; 
    const unsigned v_v_subsample = 1; 

    int i=0;
    
    frameHdr->SOF = SOF0;
    frameHdr->Lf = 8 + (3 * num_components); //Size of FrameHeader in bytes without the Marker SOF
    frameHdr->P = 8;
    frameHdr->Y = picture_height;
    frameHdr->X = picture_width;
    frameHdr->Nf = num_components;
    
    for(i=0; i<num_components; i++) {
        frameHdr->JPEGComponent[i].Ci = i+1;
        
        if(i == 0) {
            frameHdr->JPEGComponent[i].Hi = y_h_subsample;
            frameHdr->JPEGComponent[i].Vi = y_v_subsample;
            frameHdr->JPEGComponent[i].Tqi = 0;

        } else {
            //Analyzing the sampling factors for U/V, they are 1 for all formats except for Y8. 
            //So, it is okay to have the code below like this. For Y8, we wont reach this code.
            frameHdr->JPEGComponent[i].Hi = u_h_subsample;
            frameHdr->JPEGComponent[i].Vi = u_v_subsample;
            frameHdr->JPEGComponent[i].Tqi = 1;
        }
    }
};

void Defaults::populate_huff_section_header(JPEGHuffSection *huffSectionHdr, int th, int tc) const
{
    int i=0, totalCodeWords=0;
    
    huffSectionHdr->DHT = DHT;
    huffSectionHdr->Tc = tc;
    huffSectionHdr->Th = th;
    
    if(th == 0) { //If Luma

        //If AC
        if(tc == 1) {
            memcpy(huffSectionHdr->Li, jpeg_hufftable_luma_ac+1, NUM_AC_RUN_SIZE_BITS);
            memcpy(huffSectionHdr->Vij, jpeg_hufftable_luma_ac+17, NUM_AC_CODE_WORDS_HUFFVAL);
        }
               
        //If DC        
        if(tc == 0) {
            memcpy(huffSectionHdr->Li, jpeg_hufftable_luma_dc+1, NUM_DC_RUN_SIZE_BITS);
            memcpy(huffSectionHdr->Vij, jpeg_hufftable_luma_dc+17, NUM_DC_CODE_WORDS_HUFFVAL);
        }
        
        for(i=0; i<NUM_AC_RUN_SIZE_BITS; i++) {
            totalCodeWords += huffSectionHdr->Li[i];
        }
        
        huffSectionHdr->Lh = 3 + 16 + totalCodeWords;

    } else { //If Chroma
        //If AC
        if(tc == 1) {
            memcpy(huffSectionHdr->Li, jpeg_hufftable_chroma_ac+1, NUM_AC_RUN_SIZE_BITS);
            memcpy(huffSectionHdr->Vij, jpeg_hufftable_chroma_ac+17, NUM_AC_CODE_WORDS_HUFFVAL);
        }
               
        //If DC        
        if(tc == 0) {
            memcpy(huffSectionHdr->Li, jpeg_hufftable_chroma_dc+1, NUM_DC_RUN_SIZE_BITS);
            memcpy(huffSectionHdr->Vij, jpeg_hufftable_chroma_dc+17, NUM_DC_CODE_WORDS_HUFFVAL);
        }

    }
};

void Defaults::populate_scan_header(JPEGScanHeader *scanHdr) const
{
    // to-do: the case below does not consider all circumstances
    const unsigned num_components = 3;
    
    scanHdr->SOS = SOS;
    scanHdr->Ns = num_components;
    
    //Y Component
    scanHdr->ScanComponent[0].Csj = 1;
    scanHdr->ScanComponent[0].Tdj = 0;
    scanHdr->ScanComponent[0].Taj = 0;
    
    if(num_components > 1) {
        //U Component
        scanHdr->ScanComponent[1].Csj = 2;
        scanHdr->ScanComponent[1].Tdj = 1;
        scanHdr->ScanComponent[1].Taj = 1;
        
        //V Component
        scanHdr->ScanComponent[2].Csj = 3;
        scanHdr->ScanComponent[2].Tdj = 1;
        scanHdr->ScanComponent[2].Taj = 1;
    }
    
    scanHdr->Ss = 0;  //0 for Baseline
    scanHdr->Se = 63; //63 for Baseline
    scanHdr->Ah = 0;  //0 for Baseline
    scanHdr->Al = 0;  //0 for Baseline
    
    scanHdr->Ls = 3 + (num_components * 2) + 3;
    
};

// VaapiSurfaceAllocator::VaapiSurfaceAllocator(VADisplay* dpy):m_dpy(dpy){ }

// VaapiSurfaceAllocator::~VaapiSurfaceAllocator(){
//     if(m_surfaces.size()){
//         free();
//     }
//  }

// bool VaapiSurfaceAllocator::alloc(const VaapiSurfaceAllocatorConfig& config, VASurfaceID surfaces[]){
//     if(!m_dpy){
//         std::cout<<"VA Display unset. Unable to allocate surfaces"<<std::endl;
//         return false;
//     }

//     VASurfaceAttrib fourcc;
//     fourcc.type =VASurfaceAttribPixelFormat;
//     fourcc.flags=VA_SURFACE_ATTRIB_SETTABLE;
//     fourcc.value.type=VAGenericValueTypeInteger;
//     fourcc.value.value.i=VA_FOURCC_NV12; //currently we only consider nv12

//     VAStatus va_status = vaCreateSurfaces(*m_dpy, config.surfaceType, config.width, config.height, 
//                                  &surfaces[0], config.surfaceNum, &fourcc, 1);
//     if(va_status != VA_STATUS_SUCCESS){
//         std::cout<<"Failed to allocate surfaces: "<<va_status<<std::endl;
//         return false;
//     }

//     // va_status = vaCreateContext(*m_dpy,)

//     if(m_surfaces.capacity() < m_surfaces.size()+ config.surfaceNum){
//         m_surfaces.reserve(m_surfaces.size()+ config.surfaceNum);
//     }
//     for(std::size_t i =0; i<config.surfaceNum;++i){
//         m_surfaces.push_back(surfaces[i]);
//     }
//     return true;
    
// }

// void VaapiSurfaceAllocator::free(){
//     for(std::vector<VASurfaceID>::iterator it = m_surfaces.begin(); it != m_surfaces.end(); ++it){
//         vaDestroySurfaces(*m_dpy,&(*it),1);
//     }
//     std::vector<VASurfaceID>().swap(m_surfaces);
// }

// bool VaapiSurfaceAllocator::getSurfacesAddr(VASurfaceID*& surfAddr){
//     if(m_surfaces.empty())
//         return false;

//     surfAddr = &(m_surfaces[0]);
//     return true;
// }

SurfacePool::SurfacePool(VADisplay* dpy, JpegEncPicture* picPool): m_dpy(dpy), m_picPool(picPool),
        m_freeSurfaces(nullptr), m_usedSurfaces(nullptr){ }

SurfacePool::~SurfacePool(){ }

bool SurfacePool::init(SurfacePool::Config& config){
    std::lock_guard<std::mutex> lg(m_mutex);    
    if(!m_dpy){
        std::cout<<"VA Display unset. Unable to initialize surface pool"<<std::endl;
        return false;
    }

    if(!config.surfaceNum){
        std::cout<<"Requested number of surface is 0"<<std::endl;
        return true;
    }

    m_config = config;

    VASurfaceID surfaces[config.surfaceNum];
    // if(m_allocator->alloc(config, surfaces)){
    //     for(std::size_t i = 0; i < config.surfaceNum; ++i){
    //         Surface* newItem = new Surface{surfaces[i], i, m_freeSurfaces};
    //         m_freeSurfaces = newItem;
    //     }
    //     return true;
    // }
    // else{
    //     std::cout<<"Fail to allocate surfaces in surface pool"<<std::endl;
    //     return false;
    // }
    for(std::size_t i = 0; i < config.surfaceNum; ++i){
        Surface* newItem = new Surface{surfaces[i], 0, i, nullptr, m_freeSurfaces};
        m_freeSurfaces = newItem;
    }

    return true;
}
// static bool firstImg = true;

bool SurfacePool::getFreeSurfaceUnsafe(SurfacePool::Surface** surface, int fd, std::shared_ptr<hva::hvaBuf_t<int, VideoMeta>> pBuf){
    *surface = m_freeSurfaces;
    m_freeSurfaces = (*surface)->next;
    (*surface)->next = nullptr;
    (*surface)->pBuf = pBuf;

    VASurfaceAttribExternalBuffers extbuf;
    extbuf.pixel_format = VA_FOURCC_NV12; //va_format->fourcc;
    extbuf.width = m_config.width;
    extbuf.height = m_config.height;

    unsigned alignedWidth = alignTo(m_config.width);
    unsigned alignedHeight = alignTo(m_config.height);

    extbuf.data_size = m_config.fdLength ;// 2625536; //alignedWidth*alignedHeight*3/2;
    extbuf.num_planes = 2;
    // for (i = 0; i < extbuf.num_planes; i++) {
    //     extbuf.pitches[i] = GST_VIDEO_INFO_PLANE_STRIDE (vip, i);
    //     extbuf.offsets[i] = GST_VIDEO_INFO_PLANE_OFFSET (vip, i);
    // }
    // todo: work out the proper pitch and offset
    extbuf.pitches[0] = alignedWidth;
    extbuf.pitches[1] = alignedWidth;
    extbuf.offsets[0] = 0;
    extbuf.offsets[1] = alignedWidth*alignedHeight;

    extbuf.buffers = (uintptr_t *) &fd;
    extbuf.num_buffers = 1;
    extbuf.flags = 0;
    extbuf.private_data = NULL;

    VASurfaceAttrib attrib[3];
    attrib[0].type =VASurfaceAttribPixelFormat;
    attrib[0].flags=VA_SURFACE_ATTRIB_SETTABLE;
    attrib[0].value.type=VAGenericValueTypeInteger;
    attrib[0].value.value.i=VA_FOURCC_NV12; //currently we only consider nv12

    attrib[1].type = VASurfaceAttribExternalBufferDescriptor;
    attrib[1].flags= VA_SURFACE_ATTRIB_SETTABLE;
    attrib[1].value.type = VAGenericValueTypePointer;
    attrib[1].value.value.p = &extbuf;

    attrib[2].type = VASurfaceAttribMemoryType;
    attrib[2].flags= VA_SURFACE_ATTRIB_SETTABLE;
    attrib[2].value.type = VAGenericValueTypeInteger;
    attrib[2].value.value.i = VA_SURFACE_ATTRIB_MEM_TYPE_DRM_PRIME;

    VAStatus va_status = vaCreateSurfaces(*m_dpy, m_config.surfaceType, m_config.width, m_config.height, 
                                 &((*surface)->surfaceId), 1, &(attrib[0]), 3);
    if(va_status != VA_STATUS_SUCCESS){
        std::cout<<"Failed to allocate surfaces: "<<va_status<<std::endl;

        /* in case of an creation failure, return the surface to free pool */
        (*surface)->next = m_freeSurfaces;
        (*surface)->pBuf.reset();
        m_freeSurfaces = *surface;

        // to-do: try re create a surface
        return false;
    }

    // if(firstImg){
    //     VAImage vaImg;
    //     VAStatus status = vaDeriveImage(*m_dpy, (*surface)->surfaceId, &vaImg);
    //     if(va_status != VA_STATUS_SUCCESS){
    //         std::cout<<"Fail to derive image."<<std::endl;
    //         return false;
    //     }
    //     void* mappedSurf = nullptr;
    //     va_status = vaMapBuffer(*m_dpy,vaImg.buf, &mappedSurf);
    //     if(va_status != VA_STATUS_SUCCESS){
    //         std::cout<<"Fail to map image."<<std::endl;
    //         return false;
    //     }

    //     auto myfile = std::fstream("firstimage.yuv", std::ios::out | std::ios::binary);
    //     myfile.write(&(((char*)mappedSurf)[0]), 1088*768*3/2);
    //     myfile.close();

    //     vaUnmapBuffer(*m_dpy, vaImg.buf);
    //     vaDestroyImage(*m_dpy,vaImg.image_id);
    //     firstImg = false;
    // }

    return true;
}

bool SurfacePool::getFreeSurface(SurfacePool::Surface** surface, int fd, std::shared_ptr<hva::hvaBuf_t<int, VideoMeta>> pBuf){
    std::unique_lock<std::mutex> lk(m_mutex);
    m_cv.wait(lk, [&]{return m_freeSurfaces!=nullptr;});
    getFreeSurfaceUnsafe(surface, fd, std::move(pBuf));
    lk.unlock();
    return true;
}

bool SurfacePool::tryGetFreeSurface(SurfacePool::Surface** surface, int fd, std::shared_ptr<hva::hvaBuf_t<int, VideoMeta>> pBuf){
    std::lock_guard<std::mutex> lg(m_mutex);
    if(m_freeSurfaces){
        return getFreeSurfaceUnsafe(surface, fd, std::move(pBuf));
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
        *surface = nullptr;
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
    std::size_t index = (*surface)->index;
    vaDestroyBuffer(*m_dpy, m_picPool[index].codedBufId);
    vaDestroyBuffer(*m_dpy, m_picPool[index].picParamBufId);
    vaDestroyBuffer(*m_dpy, m_picPool[index].qMatrixBufId);
    vaDestroyBuffer(*m_dpy, m_picPool[index].huffmanTblBufId);
    vaDestroyBuffer(*m_dpy, m_picPool[index].sliceParamBufId);
    vaDestroyBuffer(*m_dpy, m_picPool[index].headerParamBufId);
    vaDestroyBuffer(*m_dpy, m_picPool[index].headerDataBufId);
#ifdef HANTRO_JPEGENC_ROI_API
    vaDestroyBuffer(*m_dpy, m_picPool[index].ROIDataBufId);
#endif 

    vaDestroySurfaces(*m_dpy,&((*surface)->surfaceId),1);
    vaDestroyContext(*m_dpy,(*surface)->ctxId);

    (*surface)->next = m_freeSurfaces;
    (*surface)->pBuf.reset();
    m_freeSurfaces = *surface;
    return true;
}


JpegEncNode::JpegEncNode(std::size_t inPortNum, std::size_t outPortNum, std::size_t totalThreadNum, const std::vector<uint64_t>& WIDs):
        hva::hvaNode_t(inPortNum, outPortNum, totalThreadNum),m_workerIdx(0), m_vWID(WIDs){
    // if(!initVaapi()){
    //     std::cout<<"Vaapi init failed"<<std::endl;
    //     return;
    // }
    // m_allocator = new VaapiSurfaceAllocator(&m_vaDpy); //not allocated any surfaces yet
    // m_pool = new SurfacePool(&m_vaDpy, m_allocator); //not init yet
    // m_picPool = new JpegEncPicture[PIC_POOL_SIZE];
    // memset(m_picPool, 0, sizeof(JpegEncPicture)*PIC_POOL_SIZE);
}

JpegEncNode::~JpegEncNode(){
    // if(!m_allocator)
    //     delete m_allocator;

    // if(!m_pool)
    //     delete m_pool;

    // if(!m_picPool)
    //     delete[] m_picPool;

    // vaDestroyConfig(m_vaDpy,m_jpegConfigId);
    // vaTerminate(m_vaDpy);
    // va_close_display(m_vaDpy);
}

std::shared_ptr<hva::hvaNodeWorker_t> JpegEncNode::createNodeWorker() const {
    unsigned workerIdx = m_workerIdx.fetch_add(1);
    return std::shared_ptr<hva::hvaNodeWorker_t>(new JpegEncNodeWorker((JpegEncNode*)this, m_vWID[workerIdx]) );
}

bool JpegEncNodeWorker::initVaapi(){

    /* 0. Init hddl unite workload context*/
    // std::cout<<"Preparing to create WID with pid "<<getpid()<<" and tid "<<syscall(SYS_gettid)<<std::endl;
    
    // ContextHint ctxHint;
    // ctxHint.mediaBitrate = 2.1;
    // ctxHint.ResolutionWidth = 1080;
    // ctxHint.ResolutionHeight = 720;
    // ctxHint.mediaFps = 29.7;

    // if(HDDL_OK != createWorkloadContext(&m_WID, &ctxHint)){
	// std::cout<<"Fail to get workload context ID"<<std::endl;
	// return false;
    // }

    // std::cout<<"WID Received: "<<m_WID<<std::endl;

    std::cout<<"Preparing to bind WID with " << m_WID <<", pid "<<getpid()<<" and tid "<<syscall(SYS_gettid)<<std::endl;
    if(HDDL_OK != HddlUnite::bindWorkloadContext(getpid(), syscall(SYS_gettid), m_WID)){
        std::cout<<"Fail to bind workload context"<<std::endl;
        return false;
    }


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

    // m_allocator = new VaapiSurfaceAllocator(&m_vaDpy); //not allocated any surfaces yet
    m_pool = new SurfacePool(&m_vaDpy, m_picPool); //not init yet

    m_vaDisplayReady = true;

    return true;
}

// bool JpegEncNode::initVaJpegCtx(){
//     VASurfaceID* surfAddr = nullptr;
//     m_allocator->getSurfacesAddr(surfAddr);
//     if(!m_vaDpy || !m_jpegConfigId){
//         std::cout<<"VA display or va config unset!"<<std::endl;
//         return false;
//     }
//     if(!m_picWidth || !m_picHeight){
//         std::cout<<"Pic Width and Pic Height unset!"<<std::endl;
//         return false;
//     }
//     VAStatus va_status = vaCreateContext(m_vaDpy, m_jpegConfigId, m_picWidth, m_picHeight, 
//                                 VA_PROGRESSIVE, surfAddr, 16, &m_jpegCtxId);
//     if(va_status != VA_STATUS_SUCCESS){
//         std::cout<<"Create va context failed!"<<std::endl;
//         return false;
//     }
//     return true;
// }


JpegEncNodeWorker::JpegEncNodeWorker(hva::hvaNode_t* parentNode, uint64_t WID):
        hva::hvaNodeWorker_t(parentNode),m_WID(WID), m_vaDisplayReady(false), m_surfaceAndContextReady(false), m_picHeight(0u),
        m_picWidth(0u), m_fdLength(0){
    m_jpegCtr.store(0);

    m_picPool = new JpegEncPicture[PIC_POOL_SIZE];
    memset(m_picPool, 0, sizeof(JpegEncPicture)*PIC_POOL_SIZE);
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

void JpegEncNodeWorker::processByFirstRun(std::size_t batchIdx){
    if(!initVaapi()){
        std::cout<<"Vaapi init failed"<<std::endl;
        return;
    }
}

void JpegEncNodeWorker::process(std::size_t batchIdx){
    std::vector<std::shared_ptr<hva::hvaBlob_t>> vInput= hvaNodeWorker_t::getParentPtr()->getBatchedInput(batchIdx, std::vector<size_t> {0});
    std::cout<<"KL: jpeg with batchidx"<<batchIdx<<" start process:"<<std::endl;
    if(vInput.size()==0u){
        // last time before stop fetches nothing
        SurfacePool::Surface* usedSurface = nullptr;
        m_pool->getUsedSurface(&usedSurface);
        if(!usedSurface){
            // not likely to occur
            std::cout<<"ERROR! No free surfaces and no used surfaces!" <<std::endl;
            return;
        }

        do{
            if(!saveToFile(usedSurface)){
                std::cout<<"Failed to save jpeg tagged "<<m_jpegCtr.load()<<std::endl;
            }

            m_pool->moveToFree(&usedSurface);

            m_pool->getUsedSurface(&usedSurface);
        }while(usedSurface != nullptr);

        return;
    }

    InferMeta* meta = vInput[0]->get<int, InferMeta>(0)->getMeta();
    if(meta->drop){
        // drop the frame from doing jpeg enc
        std::cout<<"KL: jpeg blob dropped frame with streamid "<<vInput[0]->streamId<<" and frame id "<<vInput[0]->frameId<<std::endl;
        return;
    }
    VideoMeta* videoMeta = vInput[0]->get<int, VideoMeta>(1)->getMeta();
    std::shared_ptr<hva::hvaBuf_t<int, VideoMeta>> pBuf = vInput[0]->get<int, VideoMeta>(1);
    const int* fd = pBuf->getPtr();

    bool reApplyFreeSurface = false;

    for(unsigned i_roi = 0; i_roi< meta->rois.size(); ++i_roi){

        if(!m_surfaceAndContextReady){
            if(!videoMeta->videoWidth || !videoMeta->videoHeight){
                std::cout<<"Picture width or height unset!"<<std::endl;
                return;
            }
            else{
                SurfacePool::Config config = {VA_RT_FORMAT_YUV420, 0u, 0u, 0u, PIC_POOL_SIZE}; //To-do: make surfaces num virable
                m_picWidth = config.width = videoMeta->videoWidth;
                m_picHeight = config.height = videoMeta->videoHeight;
                m_fdLength = config.fdLength = videoMeta->fdActualLength;
                if(!m_pool->init(config)){
                    std::cout<<"Surface pool init failed!"<<std::endl;
                    return;
                }
                // VASurfaceID* surfAddr = nullptr;
                // if(!((JpegEncNode*)m_parentNode)->initVaJpegCtx()){
                //     std::cout<<"Init jpeg context failed!"<<std::endl;
                //     return;
                // }
                m_surfaceAndContextReady = true;
            }
        }

        
        if (meta->rois[i_roi].x == 0 && meta->rois[i_roi].y == 0 && meta->rois[i_roi].width == 0 && meta->rois[i_roi].height == 0){
            continue;
        }

        std::cout << "KL: jpeg blob received with FD: " << *fd << " with streamid " << vInput[0]->streamId << " and frame id " << vInput[0]->frameId << std::endl;
        do{
            reApplyFreeSurface = false;
            SurfacePool::Surface* surface = nullptr;
            if(m_pool->tryGetFreeSurface(&surface, *fd, pBuf)){
                // VASurfaceID vaSurf = surface->surfaceId;
                if(!surface->surfaceId){
                    std::cout<<"Surface got is invalid!"<<std::endl;
                    continue;
                }
                // const unsigned char* img = vInput[0]->get<unsigned char, std::pair<unsigned, unsigned>>(1)->getPtr();
                // if(!prepareSurface(vaSurf, img)){
                //     std::cout<<"Fail to prepare input surface!"<<std::endl;
                //     return;
                // }

                // auto context = HddlUnite::queryWorkloadContext(((JpegEncNode*)m_parentNode)->m_WID);
                // HddlUnite::SMM::RemoteMemory temp(*context, *fd);
                // char* tempData = new char[768*1088*3/2];
                // temp.syncFromDevice(tempData, 768*1088*3/2);
                // std::stringstream ss;
                // ss << "DumpSurf"<<surface->surfaceId<<".nv12";
                // FILE* nv12FP = fopen(ss.str().c_str(), "wb");  
                // unsigned w_items = 0;
                // do {
                //     w_items = fwrite(tempData, 768*1088*3/2, 1, nv12FP);
                // } while (w_items != 1);

                // fclose(nv12FP);

                // delete[] tempData;

                VAStatus va_status;
                unsigned picWidth = m_picWidth;
                unsigned picHeight = m_picHeight;
                va_status = vaCreateContext(m_vaDpy, m_jpegConfigId, picWidth, 
                        picHeight, VA_PROGRESSIVE, &(surface->surfaceId), 1, &(surface->ctxId));

                JpegEncPicture* picPool = m_picPool;
                std::size_t index = surface->index;
                // Encoded buffer
                va_status = vaCreateBuffer(m_vaDpy, surface->ctxId,
                        VAEncCodedBufferType, alignTo(picWidth)*alignTo(picHeight)*3/2+MAX_APP_HDR_SIZE + MAX_FRAME_HDR_SIZE +
                        MAX_QUANT_TABLE_SIZE + MAX_HUFFMAN_TABLE_SIZE + MAX_SCAN_HDR_SIZE, 
                        1, NULL, &(picPool[index].codedBufId));

                if(va_status != VA_STATUS_SUCCESS){
                    std::cout<<"Create coded buffer failed!"<<std::endl;
                    return;
                }

                // Picture parameter buffer
                VAEncPictureParameterBufferJPEG pic_param;
                memset(&pic_param, 0, sizeof(pic_param));
                pic_param.coded_buf = picPool[index].codedBufId;
                jpegenc_pic_param_init(&pic_param, picWidth, picHeight, 50); //to-do: make quality virable
                va_status = vaCreateBuffer(m_vaDpy, surface->ctxId,
                        VAEncPictureParameterBufferType, sizeof(VAEncPictureParameterBufferJPEG), 1, &pic_param, &(picPool[index].picParamBufId));
                if(va_status != VA_STATUS_SUCCESS){
                    std::cout<<"Create picture param failed!"<<std::endl;
                    return;
                }

                //Quantization matrix
                va_status = vaCreateBuffer(m_vaDpy, surface->ctxId,
                        VAQMatrixBufferType, sizeof(VAQMatrixBufferJPEG), 1, Defaults::getDefaults().qMatrix(), &(picPool[index].qMatrixBufId));
                if(va_status != VA_STATUS_SUCCESS){
                    std::cout<<"Create quantization matrix failed!"<<std::endl;
                    return;
                }

                //Huffman table
                va_status = vaCreateBuffer(m_vaDpy,surface->ctxId, VAHuffmanTableBufferType, 
                                        sizeof(VAHuffmanTableBufferJPEGBaseline), 1, Defaults::getDefaults().huffmanTbl(), &(picPool[index].huffmanTblBufId));
                if(va_status != VA_STATUS_SUCCESS){
                    std::cout<<"Create huffman table failed!"<<std::endl;
                    return;
                }

                //Slice parameter
                va_status = vaCreateBuffer(m_vaDpy, surface->ctxId, VAEncSliceParameterBufferType, 
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
                va_status = vaCreateBuffer(m_vaDpy,
                                        surface->ctxId,
                                        VAEncPackedHeaderParameterBufferType,
                                        sizeof(packed_header_param_buffer), 1, &packed_header_param_buffer,
                                        &(picPool[index].headerParamBufId));
                if(va_status != VA_STATUS_SUCCESS){
                    std::cout<<"Create header parameter buffer failed!"<<std::endl;
                    return;
                }

                va_status = vaCreateBuffer(m_vaDpy,
                                        surface->ctxId,
                                        VAEncPackedHeaderDataBufferType,
                                        (length_in_bits + 7) / 8, 1, packed_header_buffer,
                                        &(picPool[index].headerDataBufId));
                if(va_status != VA_STATUS_SUCCESS){
                    std::cout<<"Create header buffer failed!"<<std::endl;
                    return;
                }

#ifdef HANTRO_JPEGENC_ROI_API
                /* 12. Create ROI Buffer */
                // VABufferID buf_id;
                VAEncMiscParameterBuffer *misc_param;
                HANTROEncMiscParameterBufferEmbeddedPreprocess *ROIData;
                va_status = vaCreateBuffer(m_vaDpy, surface->ctxId, VAEncMiscParameterBufferType,
                        sizeof(VAEncMiscParameterBuffer) + sizeof(HANTROEncMiscParameterBufferEmbeddedPreprocess),
                        1, NULL, &(picPool[index].ROIDataBufId));
                if(va_status != VA_STATUS_SUCCESS){
                    std::cout<<"Create ROI Misc buffer failed!"<<std::endl;
                    return;
                }

                va_status = vaMapBuffer(m_vaDpy,(picPool[index].ROIDataBufId),(void **)&misc_param);
                if(va_status != VA_STATUS_SUCCESS){
                    std::cout<<"Map ROI Misc buffer failed!"<<std::endl;
                    return;
                }

                misc_param->type =(VAEncMiscParameterType) HANTROEncMiscParameterTypeEmbeddedPreprocess;
                ROIData= (HANTROEncMiscParameterBufferEmbeddedPreprocess*)misc_param->data;
                int cropx = alignTo(meta->rois[i_roi].x);
                int cropy = alignTo(meta->rois[i_roi].y);
                if (cropx > alignTo(m_picWidth) - 128)
                    cropx = alignTo(m_picWidth) - 128;
                if (cropy > alignTo(m_picHeight) - 128)
                    cropy = alignTo(m_picHeight) - 128;
                int cropw = alignTo(meta->rois[i_roi].x + meta->rois[i_roi].width) - cropx;
                int croph = alignTo(meta->rois[i_roi].y + meta->rois[i_roi].height) - cropy;
                if (cropx + cropw >= m_picWidth){
                    cropw = alignTo(m_picWidth) - 64 - cropx;
                }
                if (cropy + croph >= m_picHeight){
                    croph = alignTo(m_picHeight) - 64 - cropy;
                }
                if(cropw <= 0)
                    cropw = 64;
                if(croph <= 0)
                    croph = 64;
                ROIData->cropping_offset_x = cropx;
                ROIData->cropping_offset_y = cropy;
                ROIData->cropped_width = cropw;
                ROIData->cropped_height = croph;

                va_status = vaUnmapBuffer(m_vaDpy, (picPool[index].ROIDataBufId));
                if(va_status != VA_STATUS_SUCCESS){
                    std::cout<<"Unmap ROI Misc buffer failed!"<<std::endl;
                    return;
                }

#endif //#ifdef HANTRO_JPEGENC_ROI_API

                // begine encoding
                va_status = vaBeginPicture(m_vaDpy, surface->ctxId, surface->surfaceId);
                if(va_status != VA_STATUS_SUCCESS){
                    std::cout<<"Begin picture failed!"<<std::endl;
                    return;
                }

                va_status = vaRenderPicture(m_vaDpy,surface->ctxId, &(picPool[index].picParamBufId), 1);
                if(va_status != VA_STATUS_SUCCESS){
                    std::cout<<"Render picture failed!"<<std::endl;
                    return;
                }

                va_status = vaRenderPicture(m_vaDpy,surface->ctxId, &(picPool[index].qMatrixBufId), 1);
                if(va_status != VA_STATUS_SUCCESS){
                    std::cout<<"Render picture failed!"<<std::endl;
                    return;
                }

                va_status = vaRenderPicture(m_vaDpy,surface->ctxId, &(picPool[index].huffmanTblBufId), 1);
                if(va_status != VA_STATUS_SUCCESS){
                    std::cout<<"Render picture failed!"<<std::endl;
                    return;
                }

                va_status = vaRenderPicture(m_vaDpy,surface->ctxId, &(picPool[index].sliceParamBufId), 1);
                if(va_status != VA_STATUS_SUCCESS){
                    std::cout<<"Render picture failed!"<<std::endl;
                    return;
                }
                
                va_status = vaRenderPicture(m_vaDpy,surface->ctxId, &(picPool[index].headerParamBufId), 1);
                if(va_status != VA_STATUS_SUCCESS){
                    std::cout<<"Render picture failed!"<<std::endl;
                    return;
                }
                
                va_status = vaRenderPicture(m_vaDpy,surface->ctxId, &(picPool[index].headerDataBufId), 1);
                if(va_status != VA_STATUS_SUCCESS){
                    std::cout<<"Render picture failed!"<<std::endl;
                    return;
                }
#ifdef HANTRO_JPEGENC_ROI_API
                va_status = vaRenderPicture(m_vaDpy,surface->ctxId, &(picPool[index].ROIDataBufId), 1);
                if(va_status != VA_STATUS_SUCCESS){
                    std::cout<<"Render picture failed!"<<std::endl;
                    return;
                }
#endif //#ifdef HANTRO_JPEGENC_ROI_API            
                va_status = vaEndPicture(m_vaDpy,surface->ctxId);
                if(va_status != VA_STATUS_SUCCESS){
                    std::cout<<"End picture failed!"<<std::endl;
                    return;
                }

                m_pool->moveToUsed(&surface);
                surface = nullptr;

                std::cout<<"Finished submitting a jpeg with fd " << *fd <<std::endl;

            }
            else{
                std::cout<<"No free surfaces available, now try to recycle used surfaces. current fd: "<<*fd<<std::endl;
                SurfacePool::Surface* usedSurface = nullptr;
                m_pool->getUsedSurface(&usedSurface);
                if(!usedSurface){
                    std::cout<<"ERROR! No free surfaces and no used surfaces!" <<std::endl;
                    return;
                }

                do{
                    if(!saveToFile(usedSurface)){
                        std::cout<<"Failed to save jpeg tagged "<<m_jpegCtr.load()<<std::endl;
                    }

                    m_pool->moveToFree(&usedSurface);

                    m_pool->getUsedSurface(&usedSurface);
                }while(usedSurface!=nullptr);
                reApplyFreeSurface = true;
            }

        }while(reApplyFreeSurface);
    }

}

bool JpegEncNodeWorker::saveToFile(SurfacePool::Surface* surface){
    VASurfaceStatus surface_status = (VASurfaceStatus) 0;
    VACodedBufferSegment *coded_buffer_segment;
    std::size_t index = surface->index;
    JpegEncPicture* picPool = m_picPool;

    VAStatus va_status = vaSyncSurface(m_vaDpy, surface->surfaceId);
    if(va_status != VA_STATUS_SUCCESS){
        std::cout<<"Sync surface failed!"<<std::endl;
        return false;
    }

    va_status = vaQuerySurfaceStatus(m_vaDpy, surface->surfaceId, &surface_status);
    if(va_status != VA_STATUS_SUCCESS){
        std::cout<<"Query surface status failed!"<<std::endl;
        return false;
    }
    std::cout<<"vaQuerySurfaceStatus: "<< surface_status<<std::endl;

    va_status = vaMapBuffer(m_vaDpy, picPool[index].codedBufId, (void **)(&coded_buffer_segment));
    if(va_status != VA_STATUS_SUCCESS){
        std::cout<<"Map coded buffer failed!"<<std::endl;
        return false;
    }

    if (coded_buffer_segment->status & VA_CODED_BUF_STATUS_SLICE_OVERFLOW_MASK) {
        vaUnmapBuffer(m_vaDpy, picPool[index].codedBufId);
        std::cout<<"ERROR......Coded buffer too small"<<std::endl;
        return false;
    }

    int slice_data_length = coded_buffer_segment->size;
    std::size_t w_items = 0;
    std::stringstream ss;
    ss << "jpegenc-"<<std::to_string(m_WID)<<"-"<< m_jpegCtr.fetch_add(1)<<".jpg";
    FILE* jpeg_fp = fopen(ss.str().c_str(), "wb");  
    do {
        w_items = fwrite(coded_buffer_segment->buf, slice_data_length, 1, jpeg_fp);
    } while (w_items != 1);

    fclose(jpeg_fp);

    va_status = vaUnmapBuffer(m_vaDpy, picPool[index].codedBufId);
    if(va_status != VA_STATUS_SUCCESS){
        std::cout<<"Unmap coded buffer failed!"<<std::endl;
        return false;
    }
    return true;
}

void JpegEncNodeWorker::deinit(){
    SurfacePool::Surface* usedSurface = nullptr;
    m_pool->getUsedSurface(&usedSurface);
    if(!usedSurface){
        // all used surfaces are synced
        return;
    }

    do{
        if(!saveToFile(usedSurface)){
            std::cout<<"Failed to save jpeg tagged "<<m_jpegCtr.load()<<std::endl;
        }

        m_pool->moveToFree(&usedSurface);

        m_pool->getUsedSurface(&usedSurface);
    }while(usedSurface != nullptr);

    if(!m_pool)
        delete m_pool;

    if(!m_picPool)
        delete[] m_picPool;

    vaDestroyConfig(m_vaDpy,m_jpegConfigId);
    vaTerminate(m_vaDpy);
    va_close_display(m_vaDpy);

    return;
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
    VAStatus va_status = vaDeriveImage(m_vaDpy, surface, &vaImg);
    if(va_status != VA_STATUS_SUCCESS){
        std::cout<<"Fail to derive image."<<std::endl;
        return false;
    }
    void* mappedSurf = nullptr;
    va_status = vaMapBuffer(m_vaDpy,vaImg.buf, &mappedSurf);
    if(va_status != VA_STATUS_SUCCESS){
        std::cout<<"Fail to map image."<<std::endl;
        return false;
    }
    unsigned picWidth = m_picWidth;
    unsigned picHeight = m_picHeight;

    const unsigned char* y_src = img;
    const unsigned char* uv_src = img + picWidth*picHeight;
    unsigned char* y_dst = (unsigned char*)mappedSurf + vaImg.offsets[0];
    unsigned char* uv_dst = (unsigned char*)mappedSurf + vaImg.offsets[1];

    for(unsigned row = 0; row < picHeight; ++row){
        memcpy(y_dst, y_src, vaImg.width);
        y_dst += vaImg.pitches[0];
        y_src += picWidth;
    }

    for(unsigned row = 0; row < picHeight/2; ++row){
        memcpy(uv_dst, uv_src, vaImg.width);
        uv_dst += vaImg.pitches[1];
        uv_src += picHeight;
    }
    // unsigned offset = picWidth * alignTo(picHeight);
    // memcpy(mappedSurf, img, (unsigned)(picWidth*picHeight*3/2)); // to-do: currently no alignment
    // memcpy(mappedSurf+offset, img+offset, (unsigned)(picWidth * picHeight /2));
    vaUnmapBuffer(m_vaDpy,vaImg.buf);
    vaDestroyImage(m_vaDpy,vaImg.image_id);
    return true;
}
