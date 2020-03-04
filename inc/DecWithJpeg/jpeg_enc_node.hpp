#ifndef JPEG_ENC_NODE_HPP
#define JPEG_ENC_NODE_HPP

#include <hvaPipeline.hpp>
#include <va/va.h>
#include <memory>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <WorkloadContext.h>

//#define NEW_HDDL_WCTX
// #define HANTRO_JPEGENC_ROI_API

#ifdef HANTRO_JPEGENC_ROI_API

#include <va_hantro/va_hantro.h>

#endif //#ifdef HANTRO_JPEGENC_ROI_API

enum JpegEncNodeStatus_t{
    JpegEnc_NoError = 0,
    JpegEnc_VaInitFailed
};

// class VaapiSurfaceAllocator{
// public:
//     struct VaapiSurfaceAllocatorConfig{
//         int surfaceType;
//         unsigned width;
//         unsigned height;
//         unsigned surfaceNum;
//         // VASurfaceAttrib fourcc;
//     };

//     typedef VaapiSurfaceAllocatorConfig Config; 

//     VaapiSurfaceAllocator(VADisplay* dpy);
//     ~VaapiSurfaceAllocator();
//     bool alloc(const VaapiSurfaceAllocatorConfig& config, VASurfaceID surfaces[]);
//     void free();
//     bool getSurfacesAddr(VASurfaceID*& surfAddr);

//     VaapiSurfaceAllocator& operator=(const VaapiSurfaceAllocator&) = delete;
//     VaapiSurfaceAllocator& operator=(VaapiSurfaceAllocator&&) = delete;

// private:
//     VADisplay* m_dpy;
//     VAContextID* m_ctx;
//     std::vector<VASurfaceID> m_surfaces;
// };

struct JpegEncPicture;

class SurfacePool{
public:
    struct Surface;
    struct Surface{
        VASurfaceID surfaceId;
        VAContextID ctxId;
        std::size_t index;
        std::shared_ptr<hva::hvaBuf_t<int, std::pair<unsigned, unsigned>>> pBuf;
        Surface* next;
    };

    struct Config{
        int surfaceType;
        unsigned width;
        unsigned height;
        unsigned surfaceNum;
    };

    // SurfacePool(VADisplay* dpy, VaapiSurfaceAllocator* allocator);
    SurfacePool(VADisplay* dpy, JpegEncPicture* picPool);
    ~SurfacePool();
    bool init(Config& config);
    bool getFreeSurface(Surface** surface, int fd, std::shared_ptr<hva::hvaBuf_t<int, std::pair<unsigned, unsigned>>> pBuf);
    bool tryGetFreeSurface(Surface** surface, int fd, std::shared_ptr<hva::hvaBuf_t<int, std::pair<unsigned, unsigned>>> pBuf);
    bool moveToUsed(Surface** surface);
    bool getUsedSurface(Surface** surface);
    bool moveToFree(Surface** surface);
private:
    bool getFreeSurfaceUnsafe(Surface** surface, int fd, std::shared_ptr<hva::hvaBuf_t<int, std::pair<unsigned, unsigned>>> pBuf);
    bool moveToUsedUnsafe(Surface** surface);
    bool getUsedSurfaceUnsafe(Surface** surface);
    bool moveToFreeUnsafe(Surface** surface);

    Surface* m_freeSurfaces;
    Surface* m_usedSurfaces;
    VADisplay* m_dpy;
    VAContextID* m_ctx;
    // VaapiSurfaceAllocator* m_allocator;
    std::mutex m_mutex;
    std::condition_variable m_cv;
    std::mutex m_usedListMutex;
    JpegEncPicture* m_picPool;
    Config m_config;
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
#ifdef HANTRO_JPEGENC_ROI_API
    VABufferID ROIDataBufId;
#endif //#ifdef HANTRO_JPEGENC_ROI_API

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

    return ((s + 64 - 1) & (~63));
};


#define NUM_QUANT_ELEMENTS 64
#define NUM_MAX_HUFFTABLE 2
#define NUM_AC_RUN_SIZE_BITS 16
#define NUM_AC_CODE_WORDS_HUFFVAL 162
#define NUM_DC_RUN_SIZE_BITS 16
#define NUM_DC_CODE_WORDS_HUFFVAL 12

#define BITSTREAM_ALLOCATE_STEPPING     4096
#define MAX_JPEG_COMPONENTS 3 


struct __bitstream {
    unsigned int *buffer;
    int bit_offset;
    int max_size_in_dword;
};

typedef struct __bitstream bitstream;

static unsigned int 
swap32(unsigned int val)
{
    unsigned char *pval = (unsigned char *)&val;

    return ((pval[0] << 24)     |
            (pval[1] << 16)     |
            (pval[2] << 8)      |
            (pval[3] << 0));
};

static void
bitstream_start(bitstream *bs)
{
    bs->max_size_in_dword = BITSTREAM_ALLOCATE_STEPPING;
    bs->buffer = (unsigned int*) calloc(bs->max_size_in_dword * sizeof(int), 1);
    assert(bs->buffer);
    bs->bit_offset = 0;
};

static void
bitstream_end(bitstream *bs)
{
    int pos = (bs->bit_offset >> 5);
    int bit_offset = (bs->bit_offset & 0x1f);
    int bit_left = 32 - bit_offset;

    if (bit_offset) {
        bs->buffer[pos] = swap32((bs->buffer[pos] << bit_left));
    }
};
 
static void
bitstream_put_ui(bitstream *bs, unsigned int val, int size_in_bits)
{
    int pos = (bs->bit_offset >> 5);
    int bit_offset = (bs->bit_offset & 0x1f);
    int bit_left = 32 - bit_offset;

    if (!size_in_bits)
        return;

    if (size_in_bits < 32)
        val &= ((1 << size_in_bits) - 1);

    bs->bit_offset += size_in_bits;

    if (bit_left > size_in_bits) {
        bs->buffer[pos] = (bs->buffer[pos] << size_in_bits | val);
    } else {
        size_in_bits -= bit_left;
        bs->buffer[pos] = (bs->buffer[pos] << bit_left) | (val >> size_in_bits);
        bs->buffer[pos] = swap32(bs->buffer[pos]);

        if (pos + 1 == bs->max_size_in_dword) {
            bs->max_size_in_dword += BITSTREAM_ALLOCATE_STEPPING;
            bs->buffer = (unsigned int*) realloc(bs->buffer, bs->max_size_in_dword * sizeof(unsigned int));
            assert(bs->buffer);
        }

        bs->buffer[pos + 1] = val;
    }
};

//As per Jpeg Spec ISO/IEC 10918-1, below values are assigned
enum jpeg_markers {

 //Define JPEG markers as 0xFFXX if you are adding the value directly to the buffer
 //Else define marker as 0xXXFF if you are assigning the marker to a structure variable.
 //This is needed because of the little-endedness of the IA 

 SOI  = 0xFFD8, //Start of Image 
 EOI  = 0xFFD9, //End of Image
 SOS  = 0xFFDA, //Start of Scan
 DQT  = 0xFFDB, //Define Quantization Table
 DRI  = 0xFFDD, //Define restart interval
 RST0 = 0xFFD0, //Restart interval termination
 DHT  = 0xFFC4, //Huffman table
 SOF0 = 0xFFC0, //Baseline DCT   
 APP0 = 0xFFE0, //Application Segment
 COM  = 0xFFFE  //Comment segment    
};

typedef struct _JPEGQuantSection {
    
    uint16_t DQT;    //Quantization table marker
    uint16_t Lq;     //Length of Quantization table definition
    uint8_t  Tq:4;   //Quantization table destination identifier
    uint8_t  Pq:4;   //Quatization table precision. Should be 0 for 8-bit samples
    uint8_t  Qk[NUM_QUANT_ELEMENTS]; //Quantization table elements    
    
} JPEGQuantSection;

typedef struct _JPEGHuffSection {
    
        uint16_t DHT;                            //Huffman table marker
        uint16_t Lh;                             //Huffman table definition length
        uint8_t  Tc:4;                           //Table class- 0:DC, 1:AC
        uint8_t  Th:4;                           //Huffman table destination identifier
        uint8_t  Li[NUM_AC_RUN_SIZE_BITS];       //Number of Huffman codes of length i
        uint8_t  Vij[NUM_AC_CODE_WORDS_HUFFVAL]; //Value associated with each Huffman code
    
} JPEGHuffSection;


typedef struct _JPEGRestartSection {
    
    uint16_t DRI;  //Restart interval marker
    uint16_t Lr;   //Legth of restart interval segment
    uint16_t Ri;   //Restart interval
    
} JPEGRestartSection;

typedef struct _JPEGFrameHeader {
    
    uint16_t SOF;    //Start of Frame Header
    uint16_t Lf;     //Length of Frame Header
    uint8_t  P;      //Sample precision
    uint16_t Y;      //Number of lines
    uint16_t X;      //Number of samples per line
    uint8_t  Nf;     //Number of image components in frame
    
    struct _JPEGComponent {        
        uint8_t Ci;    //Component identifier
        uint8_t Hi:4;  //Horizontal sampling factor
        uint8_t Vi:4;  //Vertical sampling factor
        uint8_t Tqi;   //Quantization table destination selector        
    } JPEGComponent[MAX_JPEG_COMPONENTS];
    
} JPEGFrameHeader;


typedef struct _JPEGScanHeader {
    
    uint16_t SOS;  //Start of Scan
    uint16_t Ls;   //Length of Scan
    uint8_t  Ns;   //Number of image components in the scan
        
    struct _ScanComponent {
        uint8_t Csj;   //Scan component selector
        uint8_t Tdj:4; //DC Entropy coding table destination selector(Tdj:4 bits) 
        uint8_t Taj:4; //AC Entropy coding table destination selector(Taj:4 bits)       
    } ScanComponent[MAX_JPEG_COMPONENTS];
    
    uint8_t Ss;    //Start of spectral or predictor selection, 0 for Baseline
    uint8_t Se;    //End of spectral or predictor selection, 63 for Baseline
    uint8_t Ah:4;  //Successive approximation bit position high, 0 for Baseline
    uint8_t Al:4;  //Successive approximation bit position low, 0 for Baseline
    
} JPEGScanHeader;

class Defaults{
public:
    Defaults(const Defaults&) = delete;
    Defaults& operator=(const Defaults&) = delete;
    
    static const Defaults& getDefaults(){
        static Defaults inst;
        return inst;
    };

    ~Defaults();
    VAQMatrixBufferJPEG* qMatrix() const { return m_qMatrixParam; };
    VAHuffmanTableBufferJPEGBaseline* huffmanTbl() const { return m_huffmanTbl; };
    VAEncSliceParameterBufferJPEG* sliceParam() const { return m_sliceParam; };

    void populate_quantdata(JPEGQuantSection *quantVal, int type) const;
    void populate_frame_header(JPEGFrameHeader *frameHdr, int picture_width, int picture_height) const;
    void populate_huff_section_header(JPEGHuffSection *huffSectionHdr, int th, int tc) const;
    void populate_scan_header(JPEGScanHeader *scanHdr) const;
private:
    Defaults();

    // for quantization matrix
    void jpegenc_qmatrix_init();
    const uint8_t* jpeg_luma_quant;
    const uint8_t* jpeg_zigzag;
    const uint8_t* jpeg_chroma_quant;
    VAQMatrixBufferJPEG* m_qMatrixParam;

    // for huffman table
    void jpegenc_hufftable_init();
    VAHuffmanTableBufferJPEGBaseline* m_huffmanTbl;
    const uint8_t* jpeg_hufftable_luma_dc;
    const uint8_t* jpeg_hufftable_luma_ac;
    const uint8_t* jpeg_hufftable_chroma_dc;
    const uint8_t* jpeg_hufftable_chroma_ac;

    // for slice parameter
    void jpegenc_slice_param_init();
    VAEncSliceParameterBufferJPEG* m_sliceParam;
};

class JpegEncNode : public hva::hvaNode_t{
    friend class JpegEncNodeWorker;
public:
    JpegEncNode(std::size_t inPortNum, std::size_t outPortNum, std::size_t totalThreadNum, uint64_t WID);

    ~JpegEncNode();

    virtual std::shared_ptr<hva::hvaNodeWorker_t> createNodeWorker() const override;

    bool initVaapi();

    // bool initVaJpegCtx();

private:
    VADisplay m_vaDpy;
    int m_vaMajorVer;
    int m_vaMinorVer;
    VAConfigID m_jpegConfigId;
    // VAContextID m_jpegCtxId;
    //VaapiSurfaceAllocator* m_allocator;
    SurfacePool* m_pool;
    bool m_surfaceAndContextReady;
    bool m_vaDisplayReady;
    unsigned m_picWidth;
    unsigned m_picHeight;
    JpegEncPicture* m_picPool;
    uint64_t m_WID;

#ifdef NEW_HDDL_WCTX
    HddlUnite::WorkloadContext::Ptr m_hddlWCtx;
    WorkloadID m_WID;
#endif
};

class JpegEncNodeWorker : public hva::hvaNodeWorker_t{
public:

    JpegEncNodeWorker(hva::hvaNode_t* parentNode);

    virtual void process(std::size_t batchIdx) override;

    virtual void processByFirstRun(std::size_t batchIdx) override;

    virtual void init() override;

    //virtual void deinit() override;

private:
    bool prepareSurface(VASurfaceID surface, const unsigned char* img);
    void jpegenc_pic_param_init(VAEncPictureParameterBufferJPEG *pic_param,int width,int height,int quality);
    bool saveToFile(SurfacePool::Surface* surface);

    int build_packed_jpeg_header_buffer(unsigned char **header_buffer, 
            int picture_width, int picture_height, uint16_t restart_interval, int quality);
    std::atomic<int> m_jpegCtr;

};

#endif //#ifndef JPEG_ENC_NODE_HPP
