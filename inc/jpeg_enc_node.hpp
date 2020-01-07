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
    bool getFreeSurface(Surface** surface);
    bool tryGetFreeSurface(Surface** surface);
    bool moveToUsed(Surface** surface);
    bool getUsedSurface(Surface** surface);
private:
    bool getFreeSurfaceUnsafe(Surface** surface);
    bool moveToUsedUnsafe(Surface** surface);
    bool getUsedSurfaceUnsafe(Surface** surface);

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
    bs->buffer = calloc(bs->max_size_in_dword * sizeof(int), 1);
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
            bs->buffer = realloc(bs->buffer, bs->max_size_in_dword * sizeof(unsigned int));
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
    const int NUM_QUANT_ELEMENTS;
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

    int JpegEncNodeWorker::build_packed_jpeg_header_buffer(unsigned char **header_buffer, 
            int picture_width, int picture_height, uint16_t restart_interval, int quality);

};

#define NUM_QUANT_ELEMENTS 64
#define NUM_MAX_HUFFTABLE 2
#define NUM_AC_RUN_SIZE_BITS 16
#define NUM_AC_CODE_WORDS_HUFFVAL 162
#define NUM_DC_RUN_SIZE_BITS 16
#define NUM_DC_CODE_WORDS_HUFFVAL 12

void Defaults::populate_quantdata(JPEGQuantSection *quantVal, int type)
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

void Defaults::populate_frame_header(JPEGFrameHeader *frameHdr, int picture_width, int picture_height)
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

void Defaults::populate_huff_section_header(JPEGHuffSection *huffSectionHdr, int th, int tc)
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

void Defaults::populate_scan_header(JPEGScanHeader *scanHdr)
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

#endif //#ifndef JPEG_ENC_NODE_HPP