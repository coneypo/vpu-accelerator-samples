/* *
 * Copyright (C) 2019 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

/*
Design constraints:
for our allocator generate vaapi compatible memory, we need to follow what vaapi does:
    1.use gst_dmabuf_allocator_alloc() to generated a fd-memory, since GstFdMemory is private
      by design
    2.override all memory API, to allow general gst_memory_xxx API interface to work.

compile system needs to:
   * define DRM_TYPE as
          0 intel
          1 hantro
   * add dependencies on (pkg-config packages)
            gstreamer-1.0
            gstreamer-allocators-1.0 (needs version >= 1.14 for GstDmaBufAllocator to be defined)
            libdrm
            libdrm_intel/libdrm_hantro
*/
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>


#include <gst/allocators/gstfdmemory.h>
#include "mp_gstdrmbomemory.h"

#include "drm/drm.h"
#include "xf86drm.h"

#ifdef DRM_INTEL
#include "intel_bufmgr.h"
#define DRM_BO_MAP(bo, writable)                    drm_intel_bo_map(bo, writable)
#define DRM_BO_UNMAP(bo)                            drm_intel_bo_unmap(bo)
#define DRM_BO_IMPORT(bufmgr, primeFd, size)        drm_intel_bo_gem_create_from_prime(bufmgr, primeFd, size)
#define DRM_BO_EXPORT(bo, ptrPrimeFd)               drm_intel_bo_gem_export_to_prime(bo, ptrPrimeFd)
#define DRM_BO_ALLOC(bufmgr, name, size, align)     drm_intel_bo_alloc(bufmgr, "intel", size, align)
#define DRM_BO_UNREF(bo)                            drm_intel_bo_unreference(bo)
#define DRM_BO                                      drm_intel_bo
#define DRM_BUFMGR                                  drm_intel_bufmgr
#define DRM_BUFMGR_DESTROY(bufmgr)                  bufmgr
#endif


#ifdef DRM_HANTRO
#include "hantro_bufmgr.h"
#define DRM_BO_MAP(bo, writable)                    drm_hantro_bo_map(bo, writable)
#define DRM_BO_UNMAP(bo)                            drm_hantro_bo_unmap(bo)
#define DRM_BO_IMPORT(bufmgr, primeFd, size)        drm_hantro_bo_gem_create_from_prime(bufmgr, primeFd, size)
#define DRM_BO_EXPORT(bo, ptrPrimeFd)               drm_hantro_bo_gem_export_to_prime(bo, ptrPrimeFd)
#define DRM_BO_ALLOC(bufmgr, name, size, align)     drm_hantro_bo_gem_alloc(bufmgr, "hantro", size, 0x1000)
#define DRM_BO_UNREF(bo)                            drm_hantro_bo_unreference(bo)
#define DRM_BO                                      drm_hantro_bo
#define DRM_BUFMGR                                  drm_hantro_bufmgr
#define DRM_BUFMGR_DESTROY(bufmgr)                  drm_hantro_bufmgr_destroy(bufmgr)
#endif


GST_DEBUG_CATEGORY_STATIC (hantrobo_category);     // define category (statically)
#define GST_CAT_DEFAULT hantrobo_category          // set as default

G_DEFINE_TYPE (GstHantroBoAllocator, gst_hantrobo_allocator, GST_TYPE_DMABUF_ALLOCATOR);



/****************************************************************************
   HantroBoExt utils, qdata
****************************************************************************/
typedef struct _HantroBoExt
{
    void * bufmgr;
    int    fd;
    void * ibo;
    void * virtual;
    gint   map_count;
    GMutex lock;
}HantroBoExt;

static void HantroBoExt_destroy(HantroBoExt * boext)
{
    GST_TRACE("boext=%p", boext);
    if(boext){
        boext->virtual = NULL;
        if(boext->fd >= 0){
            close(boext->fd);
            boext->fd = -1;
        }
        if(boext->ibo) {
            DRM_BO_UNREF(boext->ibo);
            boext->ibo = NULL;
        }
        g_mutex_clear(&boext->lock);
        free(boext);
    }
}

static HantroBoExt * HantroBoExt_new(void * bufmgr, gsize size, gsize align, int fd)
{
    HantroBoExt * boext = NULL;
    DRM_BO * ibo_temp = NULL;
    if(bufmgr == NULL) goto new_failed;

    if((boext = (HantroBoExt *)malloc(sizeof(HantroBoExt))) == NULL)
        goto new_failed;
    boext->bufmgr = bufmgr;
    boext->virtual = NULL;
    boext->fd = fd;
    boext->ibo = NULL;
    g_atomic_int_set(&boext->map_count, 0);
    g_mutex_init (&boext->lock);

    // do we need create a new fd?
    if(boext->fd < 0)
    {
        ibo_temp = DRM_BO_ALLOC(boext->bufmgr, "intel", size, align);
        if(!ibo_temp) {
            GST_TRACE("drm bo alloc failed for size=%lu, align=%lu, errno=%d(%s)", size, align, errno, strerror(errno));
            goto new_failed;
        }

        if(DRM_BO_EXPORT(ibo_temp, &boext->fd)) {
            GST_TRACE("export primeFD failed");
            goto new_failed;
        }
        DRM_BO_UNREF(ibo_temp);
        ibo_temp = NULL;
    }
    GST_TRACE("boext=%p", boext);
    return boext;

new_failed:
    GST_TRACE("failed");
    if(ibo_temp){
        DRM_BO_UNREF(ibo_temp);
        ibo_temp = NULL;
    }
    if(boext)
        HantroBoExt_destroy(boext);
    return NULL;
}


static void * HantroBoExt_map(HantroBoExt * boext, int pfd, int size)
{
    GST_TRACE("boext=%p", boext);
    if(boext == NULL) return NULL;

    if(boext->ibo == NULL) {
        g_mutex_lock (&boext->lock);
        //request_write = flags & GST_MAP_WRITE ?1:0;
        if(boext->ibo == NULL){
            // we only map once for recursive request
            boext->ibo = DRM_BO_IMPORT(boext->bufmgr, pfd, size);
            // for simplicity: always writable
            if(!DRM_BO_MAP(boext->ibo, 1))
                boext->virtual = ((DRM_BO *)(boext->ibo))->virtual;
            else
                boext->virtual = NULL;
        }
        g_mutex_unlock (&boext->lock);
    }

    // map count
    g_atomic_int_add(&boext->map_count, 1);

    return boext->virtual;
}

static void HantroBoExt_unmap(HantroBoExt * boext)
{
    GST_TRACE("boext=%p", boext);
    if(boext == NULL) return;

    // make sure map_count >=0
    if(g_atomic_int_get(&boext->map_count) <= 0)
        return;

    if(g_atomic_int_add(&boext->map_count, -1) == 1) {
        DRM_BO_UNMAP(boext->ibo);
        boext->ibo = NULL;
        boext->virtual = NULL;
    }
}

/****************************************************************************
   mem APIs
****************************************************************************/

#define GST_HANTROBO_QUARK gst_hantrobo_quark_get ()
static GQuark
gst_hantrobo_quark_get (void)
{
  static gsize g_quark;

  if (g_once_init_enter (&g_quark)) {
    gsize quark = (gsize) g_quark_from_static_string ("GstHantroBo");
    g_once_init_leave (&g_quark, quark);
  }
  return g_quark;
}



static GstMemory *
gst_hantrobo_allocator_alloc (GstAllocator * base_allocator, gsize size, GstAllocationParams * params)
{
    GstMemory * gmem = NULL;
    GstHantroBoAllocator *const allocator =
        GST_HANTROBO_ALLOCATOR_CAST (base_allocator);

    g_return_val_if_fail (allocator != NULL, NULL);
    //g_return_val_if_fail (size != 0, NULL);
#if 0
    printf("base_allocator is GST_TYPE_HANTROBO_ALLOCATOR?   %d\n", (G_TYPE_CHECK_INSTANCE_TYPE((base_allocator), GST_TYPE_HANTROBO_ALLOCATOR)));
    printf("base_allocator is GST_TYPE_DMABUF_ALLOCATOR?     %d\n", (G_TYPE_CHECK_INSTANCE_TYPE((base_allocator), GST_TYPE_DMABUF_ALLOCATOR)));
    printf("base_allocator is GstAllocator?                     %d\n", (G_TYPE_CHECK_INSTANCE_TYPE((base_allocator), GST_TYPE_ALLOCATOR)));

    printf("allocator is GST_TYPE_HANTROBO_ALLOCATOR?   %d\n", (G_TYPE_CHECK_INSTANCE_TYPE((allocator), GST_TYPE_HANTROBO_ALLOCATOR)));
    printf("allocator is GST_TYPE_DMABUF_ALLOCATOR?     %d\n", (G_TYPE_CHECK_INSTANCE_TYPE((allocator), GST_TYPE_DMABUF_ALLOCATOR)));
    printf("allocator is GST_TYPE_ALLOCATOR?               %d\n", (G_TYPE_CHECK_INSTANCE_TYPE((allocator), GST_TYPE_ALLOCATOR)));
#endif

    // create drm BO
    // export to prime fd
    GST_TRACE("params: prefix=%lu, padding=%lu, align=%lu", params->prefix, params->padding, params->align);

    gsize maxsize = size + params->prefix + params->padding + 16; // to pass test only;

    HantroBoExt  *boext = HantroBoExt_new(allocator->bufmgr, maxsize, params->align, -1);
    if(boext == NULL) goto alloc_error_exit;

    // create dmabuf proxy for the fd
    gmem = gst_dmabuf_allocator_alloc (base_allocator, boext->fd, maxsize);
    if(gmem == NULL)
        goto alloc_error_exit;

    gst_memory_init (GST_MEMORY_CAST (gmem),
                     GST_MINI_OBJECT_FLAGS (gmem) | params->flags,
                      base_allocator,
                      NULL,
                      maxsize,
                      gmem->align,
                      params->prefix,
                      size);


    // success, need store ibo for unmap, the best place would be inside memory
    // but the GstFdMemory is not derivable. the only way is through a qdata
    gst_mini_object_set_qdata (GST_MINI_OBJECT_CAST (gmem),
                                GST_HANTROBO_QUARK, boext,
                                (GDestroyNotify) HantroBoExt_destroy);

    return gmem;

alloc_error_exit:
    if(gmem) {
        g_object_unref(gmem);
    }
    return NULL;
}


static void
gst_hantrobo_mem_free (GstAllocator * allocator, GstMemory * gmem)
{
    GST_TRACE("gmem=%p, itsparent=%p", gmem, gmem->parent);
}

static gpointer
gst_hantrobo_mem_map (GstMemory * gmem, gsize maxsize, GstMapFlags flags)
{
    GstHantroBoAllocator *const allocator = GST_HANTROBO_ALLOCATOR_CAST (gmem->allocator);
    HantroBoExt  *boext = NULL;
    int ret;
    int pfd = gst_fd_memory_get_fd(gmem);

    GST_TRACE("gmem=%p, itsparent=%p", gmem, gmem->parent);

    g_return_val_if_fail (pfd >= 0, NULL);

    boext = (HantroBoExt *)(gst_mini_object_get_qdata(GST_MINI_OBJECT_CAST (gmem),
                                                     GST_HANTROBO_QUARK));
    if(!boext) return NULL;

    if (gmem->parent){
        boext->virtual = gst_hantrobo_mem_map(gmem->parent, maxsize, flags);
        return boext->virtual;
    }

    // always use writeble
    return HantroBoExt_map(boext, pfd, maxsize);
}

static void
gst_hantrobo_mem_unmap (GstMemory * gmem)
{
    HantroBoExt  *boext = NULL;

    GST_TRACE("gmem=%p, itsparent=%p", gmem, gmem->parent);

    if (gmem->parent){
        gst_hantrobo_mem_unmap (gmem->parent);
        return;
    }

    boext = (HantroBoExt *)(gst_mini_object_get_qdata(GST_MINI_OBJECT_CAST (gmem),
                                                     GST_HANTROBO_QUARK));
    HantroBoExt_unmap(boext);
}

/*
  Return a shared copy of size bytes from mem starting from offset .
  No memory copy is performed and the memory region is simply shared.
  The result is guaranteed to be non-writable.
  size can be set to -1 to return a shared copy from offset to the end of the memory region.

  No aditional map is required too, because sub mem will call parent's map/unmap
  so sub mem will share the same fd and never create it's own bo (only root parent does).
*/
static GstMemory *
gst_hantrobo_mem_share (GstMemory * gmem, gssize offset, gssize size)
{
    int pfd = gst_fd_memory_get_fd(gmem);

    GST_TRACE("gmem=%p, itsparent=%p", gmem, gmem->parent);
    GstHantroBoAllocator *const allocator = GST_HANTROBO_ALLOCATOR_CAST (gmem->allocator);
    GstMemory * gmem2 = NULL;
    gmem2 = gst_dmabuf_allocator_alloc (gmem->allocator, pfd, gmem->maxsize);
    if(gmem2){
        GstMemory *parent = gmem->parent ? gmem->parent: gmem;

        gst_memory_init (GST_MEMORY_CAST (gmem2),
                         GST_MINI_OBJECT_FLAGS (parent) | GST_MINI_OBJECT_FLAG_LOCK_READONLY,
                          gmem->allocator,
                          parent,           // there is always a parent for submem, so gst_memory_init() will lock
                                             // the parent with GST_LOCK_FLAG_EXCLUSIVE
                          gmem->maxsize,
                          gmem->align,
                          gmem->offset + offset,
                          size < 0?(gmem->size - offset): size);

        // share same fd
        gst_mini_object_set_qdata (GST_MINI_OBJECT_CAST (gmem2),
                                    GST_HANTROBO_QUARK, HantroBoExt_new(allocator->bufmgr, gmem2->size, gmem2->align, pfd),
                                    (GDestroyNotify) NULL);
    }
    return gmem2;
}

/*
  Check if @mem1 and mem2 share the memory with a common parent memory object
  and that the memory is contiguous.

  If this is the case, the memory of @mem1 and @mem2 can be merged
  efficiently by performing gst_memory_share() on the parent object from
  the returned @offset.

  Returns: %TRUE if the memory is contiguous and of a common parent.
*/
static gboolean
gst_hantrobo_mem_is_span (GstMemory * gmem1, GstMemory * gmem2, gsize * offset)
{
    GST_TRACE("gmem1=%p, gmem2=%p", gmem1, gmem2);
    HantroBoExt * boext1 = NULL;
    HantroBoExt * boext2 = NULL;
    boext1 = (HantroBoExt *)(gst_mini_object_get_qdata(GST_MINI_OBJECT_CAST (gmem1),
                                                     GST_HANTROBO_QUARK));
    boext2 = (HantroBoExt *)(gst_mini_object_get_qdata(GST_MINI_OBJECT_CAST (gmem2),
                                                     GST_HANTROBO_QUARK));
    if(!boext1 || !boext2) return 0;

    if (offset) {
        GstMemory *parent;
        *offset = gmem1->offset - gmem1->parent->offset;
    }

  /* and memory is contiguous */
  return ((char*)(boext1->virtual)) + gmem1->offset + gmem1->size ==
         ((char*)(boext2->virtual)) + gmem2->offset;
}

static void
gst_hantrobo_allocator_finalize (GObject * object)
{
    GstHantroBoAllocator *const allocator =
      GST_HANTROBO_ALLOCATOR_CAST (object);

    if(allocator->bufmgr) {
        DRM_BUFMGR_DESTROY(allocator->bufmgr);
    }

    if(allocator->fd > 0) {
        close(allocator->fd);
        allocator->fd = 0;
    }

    G_OBJECT_CLASS (gst_hantrobo_allocator_parent_class)->finalize (object);
}


static void
gst_hantrobo_allocator_class_init (GstHantroBoAllocatorClass * klass)
{
    GObjectClass *const object_class = G_OBJECT_CLASS (klass);
    GstAllocatorClass *const allocator_class = GST_ALLOCATOR_CLASS (klass);

    object_class->finalize = gst_hantrobo_allocator_finalize;
    allocator_class->free = gst_hantrobo_mem_free;
    allocator_class->alloc = gst_hantrobo_allocator_alloc;

}

static void
gst_hantrobo_allocator_init (GstHantroBoAllocator * allocator)
{
  GstAllocator *alloc = GST_ALLOCATOR_CAST (allocator);
  alloc->mem_map = gst_hantrobo_mem_map;
  alloc->mem_unmap = gst_hantrobo_mem_unmap;
  alloc->mem_share = gst_hantrobo_mem_share;
  alloc->mem_is_span = gst_hantrobo_mem_is_span;

  // hide dmabuf's implementation
  alloc->mem_map_full = NULL;
  alloc->mem_unmap_full = NULL;
}


GstAllocator *
gst_hantrobo_allocator_new (void)
{

    GstHantroBoAllocator *allocator = NULL;
    DRM_BUFMGR * bufmgr = NULL;
    int fd = -1;

    GST_DEBUG_CATEGORY_INIT (hantrobo_category, "hantrobo",
                             2, "This is hantro bo based allocator");

#ifdef DRM_INTEL
    // open the DRM device fd
    fd = open("/dev/dri/card0", O_RDWR | O_SYNC);
    if(fd < 0) {
    	GST_TRACE(" fd open faild(need sudo?)");
    	goto failed;
    }
    bufmgr = drm_intel_bufmgr_gem_init(fd, 1024);
#endif

#ifdef DRM_HANTRO
    fd = drmOpen("hantro", NULL);
    if(fd < 0) {
    	GST_TRACE(" fd open faild");
    	goto failed;
    }
    bufmgr = drm_hantro_bufmgr_open(fd);
#endif

    if(bufmgr == NULL) goto failed;
    allocator = g_object_new (GST_TYPE_HANTROBO_ALLOCATOR, NULL);
    gst_object_ref_sink (allocator);
    if(allocator) {
        allocator->fd = fd;
        allocator->bufmgr = bufmgr;
    }
    GST_TRACE(" success");

    return GST_ALLOCATOR_CAST(allocator);

failed:
    GST_TRACE(" failed");
    if(fd >= 0)
        close(fd);
    if(bufmgr)
    {
        DRM_BUFMGR_DESTROY(bufmgr);
    }
    return NULL;
}







// The failed attempt which rely on GstFdMemory/GstDmaBufAllocator in gst-plugins-base for map/unmap

#if 0

static GstMemory *
gst_hantrobo_allocator_alloc (GstAllocator * base_allocator, gsize size, GstAllocationParams * params)
{
    GstMemory * gmem = NULL;
    GstHantroBoAllocator *const allocator =
        GST_HANTROBO_ALLOCATOR_CAST (base_allocator);
    DRM_BO * ibo_temp = NULL;
    int fd = -1;

    g_return_val_if_fail (allocator != NULL, NULL);
    //g_return_val_if_fail (size != 0, NULL);
#if 0
    printf("base_allocator is GST_TYPE_HANTROBO_ALLOCATOR?   %d\n", (G_TYPE_CHECK_INSTANCE_TYPE((base_allocator), GST_TYPE_HANTROBO_ALLOCATOR)));
    printf("base_allocator is GST_TYPE_DMABUF_ALLOCATOR?     %d\n", (G_TYPE_CHECK_INSTANCE_TYPE((base_allocator), GST_TYPE_DMABUF_ALLOCATOR)));
    printf("base_allocator is GstAllocator?                     %d\n", (G_TYPE_CHECK_INSTANCE_TYPE((base_allocator), GST_TYPE_ALLOCATOR)));

    printf("allocator is GST_TYPE_HANTROBO_ALLOCATOR?   %d\n", (G_TYPE_CHECK_INSTANCE_TYPE((allocator), GST_TYPE_HANTROBO_ALLOCATOR)));
    printf("allocator is GST_TYPE_DMABUF_ALLOCATOR?     %d\n", (G_TYPE_CHECK_INSTANCE_TYPE((allocator), GST_TYPE_DMABUF_ALLOCATOR)));
    printf("allocator is GST_TYPE_ALLOCATOR?               %d\n", (G_TYPE_CHECK_INSTANCE_TYPE((allocator), GST_TYPE_ALLOCATOR)));
#endif

    // create drm BO
    // export to prime fd
    GST_TRACE("params: prefix=%lu, padding=%lu, align=%lu", params->prefix, params->padding, params->align);

    gsize maxsize = size + params->prefix + params->padding + 16; // to pass test only;

    ibo_temp = DRM_BO_ALLOC(allocator->bufmgr, "intel", maxsize, params->align);
    if(!ibo_temp) {
        GST_TRACE("drm bo alloc failed for size=%lu, align=%lu, errno=%d(%s)", size, params->align, errno, strerror(errno));
        goto alloc_error_exit;
    }

    if(DRM_BO_EXPORT(ibo_temp, &fd)) {
        GST_TRACE("export primeFD failed");
        goto alloc_error_exit;
    }
    DRM_BO_UNREF(ibo_temp);
    ibo_temp = NULL;


    // create dmabuf proxy for the fd
    gmem = gst_dmabuf_allocator_alloc (base_allocator, fd, size);
    if(gmem == NULL)
        goto alloc_error_exit;

    return gmem;

alloc_error_exit:
    if(gmem) {
        g_object_unref(gmem);
    }
    if(ibo_temp) {
        DRM_BO_UNREF(ibo_temp);
        ibo_temp = NULL;
    }
    return NULL;
}

static void
gst_hantrobo_allocator_finalize (GObject * object)
{
    GstHantroBoAllocator *const allocator =
      GST_HANTROBO_ALLOCATOR_CAST (object);

    if(allocator->bufmgr) {
        DRM_BUFMGR_DESTROY(allocator->bufmgr);
    }

    if(allocator->fd > 0) {
        close(allocator->fd);
        allocator->fd = 0;
    }

    G_OBJECT_CLASS (gst_hantrobo_allocator_parent_class)->finalize (object);
}


static void
gst_hantrobo_allocator_class_init (GstHantroBoAllocatorClass * klass)
{
    GObjectClass *const object_class = G_OBJECT_CLASS (klass);
    GstAllocatorClass *const allocator_class = GST_ALLOCATOR_CLASS (klass);

    object_class->finalize = gst_hantrobo_allocator_finalize;
    allocator_class->alloc = gst_hantrobo_allocator_alloc;
}

static void
gst_hantrobo_allocator_init (GstHantroBoAllocator * allocator)
{
  GstAllocator *alloc = GST_ALLOCATOR_CAST (allocator);
}



GstAllocator *
gst_hantrobo_allocator_new (void)
{
    GstHantroBoAllocator *allocator = NULL;
    DRM_BUFMGR * bufmgr = NULL;
    int fd = -1;

    GST_DEBUG_CATEGORY_INIT (hantrobo_category, "hantrobo",
                             2, "This is hantro bo based allocator");

    {
        // this redundant operation is for GST_DEBUG_CATEGORY_INIT inside
        // gst_dmabuf_allocator_new to work correctly
        GstAllocator * allocator_not_care = gst_dmabuf_allocator_new ();
        gst_object_unref (allocator_not_care);
    }

#if DRM_TYPE == DRM_TYPE_INTEL
    // open the DRM device fd
    fd = open("/dev/dri/card0", O_RDWR | O_SYNC);
    if(fd < 0) {
        GST_TRACE(" fd open faild(need sudo?)");
        goto failed;
    }
    bufmgr = drm_intel_bufmgr_gem_init(fd, 1024);
#endif

#if DRM_TYPE == DRM_TYPE_HANTRO
    fd = drmOpen("hantro", NULL);
    if(fd < 0) {
        GST_TRACE(" fd open faild");
        goto failed;
    }
    bufmgr = drm_hantro_bufmgr_open(fd);
#endif

    if(bufmgr == NULL) goto failed;
    allocator = g_object_new (GST_TYPE_HANTROBO_ALLOCATOR, NULL);
    gst_object_ref_sink (allocator);
    if(allocator) {
        allocator->fd = fd;
        allocator->bufmgr = bufmgr;
    }
    GST_TRACE(" success");

    return GST_ALLOCATOR_CAST(allocator);

failed:
    GST_TRACE(" failed");
    if(fd >= 0)
        close(fd);
    if(bufmgr)
    {
        DRM_BUFMGR_DESTROY(bufmgr);
    }
    return NULL;
}
#endif