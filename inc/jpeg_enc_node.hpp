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

    VaapiSurfaceAllocator& operator=(const VaapiSurfaceAllocator&) = delete;
    VaapiSurfaceAllocator& operator=(VaapiSurfaceAllocator&&) = delete;

private:
    VADisplay* m_dpy;
    std::vector<VASurfaceID> m_surfaces;
};

class SurfacePool{
public:
    struct Surface;
    struct Surface{
        VASurfaceID surfaceId;
        Surface* next;
    };

    SurfacePool(VADisplay* dpy, VaapiSurfaceAllocator* allocator);
    ~SurfacePool();
    bool init(VaapiSurfaceAllocator::Config& config);
    bool getFreeSurface(Surface& surface);
    bool tryGetFreeSurface(Surface& surface);
    bool moveToUsed();
private:
    bool getFreeSurfaceUnsafe(Surface& surface);

    Surface* m_freeSurfaces;
    Surface* m_usedSurfaces;
    VADisplay* m_dpy;
    VaapiSurfaceAllocator* m_allocator;
    std::mutex m_mutex;
    std::condition_variable m_cv;
};

struct JpegEncPicture{
    VASurfaceID surfaceId;
    VABufferID codedBufId;
    VABufferID picParamBufId;
    VABufferID qMatrixBufId;
    VABufferID huffmanTblBufId;
    VABufferID sliceParamBufId;
    VABufferID headerParamBufId;
    VABufferID headerDataBufId;

    VAEncPictureParameterBufferJPEG picParam;
    VAQMatrixBufferJPEG qMatrix;
    VAQMatrixBufferJPEG huffmanTbl;
    VAEncSliceParameterBufferJPEG sliceParam;
    VAEncPackedHeaderParameterBuffer headerParam;
    unsigned char* headerData;

};

// class JpegEncPicturePool{
// public:
//     JpegEncPicturePool();
//     ~JpegEncPicturePool();
//     void init();
//     void getPicture();
//     bool tryGetPicture();

// };

class JpegEncNode : public hva::hvaNode_t{
public:
    JpegEncNode(std::size_t inPortNum, std::size_t outPortNum, std::size_t totalThreadNum);

    virtual std::shared_ptr<hva::hvaNodeWorker_t> createNodeWorker() const override;

    bool initVaapi();

private:
    bool initVaapiCallOnce();

    VADisplay m_vaDpy;
    int m_vaMajorVer;
    int m_vaMinorVer
    VAConfigID m_jpegConfigId;
};

class JpegEncNodeWorker : public hva::hvaNodeWorker_t{
public:
    JpegEncNodeWorker(hva::hvaNode_t* parentNode);

    virtual void process(std::size_t batchIdx) override;

    virtual void init() override;

private:

};

#endif //#ifndef JPEG_ENC_NODE_HPP