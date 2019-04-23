/* *
 * Copyright (C) 2019 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

/*
 This allocator is used for allocating CMA memory suitable for on-chip HW (and CPU) access
 Although underlying memory manager is DRM, but the memory is not specific for display.
 So we call APIs from drm(hantro version) instead of VAAPI.

 for this to be compatible to gstreamer-vaapi's memory format, we need derived from GstFdMemory
 and this is not a typical GstFdMemory:
  1. we re-implement the mmap/unmap using drm API instead of mmap syscall, because the prime fd
     is read-only, write is only possible when we import it as a drm bo.

  2. it's still a GstFdMemory so fd is accessible through gst_fd_memory_get_fd(). the prime fd
     is created since the beginning and closed in free.

*/

#ifndef GST_HANTRO_BO_MEMORY_H
#define GST_HANTRO_BO_MEMORY_H

#include <gst/gstallocator.h>
#include <gst/allocators/allocators.h>


G_BEGIN_DECLS


typedef struct _GstHantroBoAllocator GstHantroBoAllocator;
typedef struct _GstHantroBoAllocatorClass GstHantroBoAllocatorClass;



#define GST_TYPE_HANTROBO_ALLOCATOR              (gst_hantrobo_allocator_get_type())
#define GST_IS_HANTROBO_ALLOCATOR(obj)           (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GST_TYPE_HANTROBO_ALLOCATOR))

#define GST_HANTROBO_ALLOCATOR_CAST(allocator)   ((GstHantroBoAllocator *) (allocator))

struct _GstHantroBoAllocator
{
  GstDmaBufAllocator parent;

  /*< private >*/
  int fd;       // drm device fd
  void *bufmgr; // drm buff manager
};


GType          gst_hantrobo_allocator_get_type (void);
GstAllocator * gst_hantrobo_allocator_new (void);


/**
 * GstHantroBoAllocatorClass:
 *
 *
 */
struct _GstHantroBoAllocatorClass
{
  GstDmaBufAllocatorClass parent_class;
};

G_END_DECLS


#endif /* GST_VAAPI_VIDEO_MEMORY_H */
