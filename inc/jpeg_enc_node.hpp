#ifndef JPEG_ENC_NODE_HPP
#define JPEG_ENC_NODE_HPP

#include <hvaPipeline.hpp>
#include <va/va.h>
#include <memory>
#include <vector>
#include <mutex>
#include <condition_variable>

enum JpegEncNodeStatus_t{
    JpegEnc_NoError = 0,
    JpegEnc_VaInitFailed
};

class VaapiSurfaceAllocator{
public:
    struct VaapiSurfaceAllocatorConfig{
        int surfaceType;
        unsigned width;
        unsigned height;
        unsigned surfaceNum;
        // VASurfaceAttrib fourcc;
    };

    typedef VaapiSurfaceAllocatorConfig Config; 

    VaapiSurfaceAllocator(VADisplay* dpy);
    ~VaapiSurfaceAllocator();
    bool alloc(const VaapiSurfaceAllocatorConfig& config, VASurfaceID surfaces[]);
    void free();
    bool getSurfacesAddr(VASurfaceID*& surfAddr);

    VaapiSurfaceAllocator& operator=(const VaapiSurfaceAllocator&) = delete;
    VaapiSurfaceAllocator& operator=(VaapiSurfaceAllocator&&) = delete;

private:
    VADisplay* m_dpy;
    VAContextID* m_ctx;
    std::vector<VASurfaceID> m_surfaces;
};

class SurfacePool{
public:
    struct Surface;
    struct Surface{
        VASurfaceID surfaceId;
        std::size_t index;
        Surface* next;
    };

    SurfacePool(VADisplay* dpy, VaapiSurfaceAllocator* allocator);
    ~SurfacePool();
    bool init(VaapiSurfaceAllocator::Config& config);
    bool getFreeSurface(Surface*& surface);
    bool tryGetFreeSurface(Surface*& surface);
    bool moveToUsed(Surface*& surface);
    bool getUsedSurface(Surface*& surface);
private:
    bool getFreeSurfaceUnsafe(Surface*& surface);
    bool moveToUsedUnsafe(Surface*& surface);
    bool getUsedSurfaceUnsafe(Surface*& surface);

    Surface* m_freeSurfaces;
    Surface* m_usedSurfaces;
    VADisplay* m_dpy;
    VAContextID* m_ctx;
    VaapiSurfaceAllocator* m_allocator;
    std::mutex m_mutex;
    std::condition_variable m_cv;
    std::mutex m_usedListMutex;
};

struct JpegEncPicture{
    std::size_t index;

    // VASurfaceID surfaceId;
    VABufferID codedBufId;
    VABufferID picParamBufId;
    VABufferID qMatrixBufId;
    VABufferID huffmanTblBufId;
    VABufferID sliceParamBufId;
    VABufferID headerParamBufId;
    VABufferID headerDataBufId;

    // VAEncPictureParameterBufferJPEG picParam;
    // VAQMatrixBufferJPEG qMatrix;
    // VAQMatrixBufferJPEG huffmanTbl;
    // VAEncSliceParameterBufferJPEG sliceParam;
    // VAEncPackedHeaderParameterBuffer headerParam;
    // unsigned char* headerData;

};

// class JpegEncPicturePool{
// public:
//     JpegEncPicturePool();
//     ~JpegEncPicturePool();
//     void init();
//     void getPicture();
//     bool tryGetPicture();

// };

inline std::size_t alignTo(std::size_t s) { //to-do: adjust alignment here

    return ((s + 16 - 1) & (~15));
};

class Defaults{
public:
    Defaults(const Defaults&) = delete;
    Defaults& operator=(const Defaults&) = delete;
    
    static const Defaults& getDefaults(){
        static Defaults inst;
        return inst;
    };
    VAQMatrixBufferJPEG* qMatrix() const { return &m_qMatrixParam; };
    VAHuffmanTableBufferJPEGBaseline* huffmanTbl() const { return &m_huffmanTbl; };
private:
    Defaults();

    // for quantization matrix
    void jpegenc_qmatrix_init();
    const int NUM_QUANT_ELEMENTS;
    const uint8_t* jpeg_luma_quant;
    const uint8_t* jpeg_zigzag;
    const uint8_t* jpeg_chroma_quant;
    VAQMatrixBufferJPEG m_qMatrixParam;

    // for huffman table
    void jpegenc_hufftable_init();
    VAHuffmanTableBufferJPEGBaseline m_huffmanTbl;
};

Defaults::Defaults(): NUM_QUANT_ELEMENTS(64){
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

    jpeg_zigzag = new uint8_t[]{
        0,   1,   8,   16,  9,   2,   3,   10,
        17,  24,  32,  25,  18,  11,  4,   5,
        12,  19,  26,  33,  40,  48,  41,  34,
        27,  20,  13,  6,   7,   14,  21,  28,
        35,  42,  49,  56,  57,  50,  43,  36,
        29,  22,  15,  23,  30,  37,  44,  51,
        58,  59,  52,  45,  38,  31,  39,  46,
        53,  60,  61,  54,  47,  55,  62,  63
    };

    jpeg_chroma_quant = new uint8_t[NUM_QUANT_ELEMENTS] = {
        17, 18, 24, 47, 99, 99, 99, 99,
        18, 21, 26, 66, 99, 99, 99, 99,
        24, 26, 56, 99, 99, 99, 99, 99,
        47, 66, 99, 99, 99, 99, 99, 99,
        99, 99, 99, 99, 99, 99, 99, 99,
        99, 99, 99, 99, 99, 99, 99, 99,
        99, 99, 99, 99, 99, 99, 99, 99,
        99, 99, 99, 99, 99, 99, 99, 99
    };

    jpegenc_qmatrix_init();
};

Defaults::~Defaults(){
    delete[] jpeg_luma_quant;
    delete[] jpeg_zigzag;
    delete[] jpeg_chroma_quant;
};

void Defaults::jpegenc_qmatrix_init(){
    m_qMatrixParam.load_lum_quantiser_matrix = 1;
   
    //LibVA expects the QM in zigzag order 
    for(int i=0; i<NUM_QUANT_ELEMENTS; i++) {
        m_qMatrixParam.lum_quantiser_matrix[i] = jpeg_luma_quant[jpeg_zigzag[i]];
    }
    
    //to-do: configuration below does not consider all circumstances
    m_qMatrixParam.load_chroma_quantiser_matrix = 1;
    for(int i=0; i<NUM_QUANT_ELEMENTS; i++) {
        m_qMatrixParam.chroma_quantiser_matrix[i] = jpeg_chroma_quant[jpeg_zigzag[i]];
    }
    
};

void Defaults::jpegenc_hufftable_init(){
    
    m_huffmanTbl.load_huffman_table[0] = 1; //Load Luma Hufftable
    m_huffmanTbl.load_huffman_table[1] = 1; //Load Chroma Hufftable for other formats
    
   //Load Luma hufftable values
   //Load DC codes
   memcpy(hufftable_param->huffman_table[0].num_dc_codes, jpeg_hufftable_luma_dc+1, 16);
   //Load DC Values
   memcpy(hufftable_param->huffman_table[0].dc_values, jpeg_hufftable_luma_dc+17, 12);
   //Load AC codes
   memcpy(hufftable_param->huffman_table[0].num_ac_codes, jpeg_hufftable_luma_ac+1, 16);
   //Load AC Values
   memcpy(hufftable_param->huffman_table[0].ac_values, jpeg_hufftable_luma_ac+17, 162);
   memset(hufftable_param->huffman_table[0].pad, 0, 2);
      
   
   //Load Chroma hufftable values if needed
   if(yuvComp.fourcc_val != VA_FOURCC_Y800) {
       //Load DC codes
       memcpy(hufftable_param->huffman_table[1].num_dc_codes, jpeg_hufftable_chroma_dc+1, 16);
       //Load DC Values
       memcpy(hufftable_param->huffman_table[1].dc_values, jpeg_hufftable_chroma_dc+17, 12);
       //Load AC codes
       memcpy(hufftable_param->huffman_table[1].num_ac_codes, jpeg_hufftable_chroma_ac+1, 16);
       //Load AC Values
       memcpy(hufftable_param->huffman_table[1].ac_values, jpeg_hufftable_chroma_ac+17, 162);
       memset(hufftable_param->huffman_table[1].pad, 0, 2);      
       
   }
    
}

class JpegEncNode : public hva::hvaNode_t{
    friend class JpegEncNodeWorker;
public:
    JpegEncNode(std::size_t inPortNum, std::size_t outPortNum, std::size_t totalThreadNum);

    ~JpegEncNode();

    virtual std::shared_ptr<hva::hvaNodeWorker_t> createNodeWorker() const override;

    bool initVaapi();

    bool initVaJpegCtx();

private:
    VADisplay m_vaDpy;
    int m_vaMajorVer;
    int m_vaMinorVer;
    VAConfigID m_jpegConfigId;
    VAContextID m_jpegCtxId;
    VaapiSurfaceAllocator* m_allocator;
    SurfacePool* m_pool;
    bool m_ready;
    unsigned m_picWidth;
    unsigned m_picHeight;
    JpegEncPicture* m_picPool;
};

class JpegEncNodeWorker : public hva::hvaNodeWorker_t{
public:

    JpegEncNodeWorker(hva::hvaNode_t* parentNode);

    virtual void process(std::size_t batchIdx) override;

    virtual void init() override;

    virtual void deinit() override;

private:
    bool prepareSurface(VASurfaceID surface, const unsigned char* img);
    void jpegenc_pic_param_init(VAEncPictureParameterBufferJPEG *pic_param,int width,int height,int quality);

#endif //#ifndef JPEG_ENC_NODE_HPP