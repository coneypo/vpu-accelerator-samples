/* *
 * Copyright (C) 2019 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <gst/gst.h>

#include <gst/allocators/gstfdmemory.h>
#include <gst/allocators/gstdmabuf.h>

#include "gstvpusmm.h"
#include "vpusmm.h"

#define ALIGN_UP(i, n)    (((i) + (n) - 1) & ~((n) - 1))

GST_DEBUG_CATEGORY_STATIC (vpusmm_debug);
#define GST_CAT_DEFAULT vpusmm_debug

G_DEFINE_TYPE (GstVPUSMMAllocator, gst_vpusmm_allocator, GST_TYPE_DMABUF_ALLOCATOR);


static GstMemory *
gst_vpusmm_allocator_alloc (GstAllocator * base_allocator, gsize size, GstAllocationParams * params)
{
    GstMemory * gmem = NULL;
    GstVPUSMMAllocator *const alloc = GST_VPUSMM_ALLOCATOR_CAST (base_allocator);
    int dmabuf_fd = -1;
    gsize maxsize = 0;
    gsize alloc_size = 0;
    g_return_val_if_fail (alloc != NULL, NULL);

    GST_TRACE("params: prefix=%lu, padding=%lu, align=%lu", params->prefix, params->padding, params->align);

    maxsize = size + params->prefix + params->padding;
    alloc_size = ALIGN_UP(maxsize, alloc->align_size);
    if(alloc_size == 0)
        alloc_size = alloc->align_size;    // a special test requires 0-sized memory

    dmabuf_fd = vpusmm_alloc_dmabuf(alloc_size, alloc->mem_type);
    if(dmabuf_fd < 0){
        GST_TRACE("vpusmm_alloc_dmabuf(%"G_GSIZE_FORMAT", %d) failed with errno=%d (%s) \n",
                   alloc_size, alloc->mem_type, errno, strerror(errno));
        goto failed;
    }

    // create dmabuf proxy for the fd
    gmem = gst_dmabuf_allocator_alloc (base_allocator, dmabuf_fd, alloc_size);
    if(gmem == NULL){
        GST_TRACE("gst_dmabuf_allocator_alloc(%d,%"G_GSIZE_FORMAT") failed with errno=%d (%s) \n",
                   dmabuf_fd, alloc_size, errno, strerror(errno));
        goto failed;
    }

    // gst_dmabuf_allocator_alloc() didn't set size and align correctly, fix it here
    gst_memory_init (GST_MEMORY_CAST (gmem),
                     GST_MINI_OBJECT_FLAGS (gmem) | params->flags,
                      base_allocator,
                      NULL,
                      alloc_size,            // don't
                      alloc->align_size - 1, // gstreamer's align is all ones binary mask
                      params->prefix,
                      size);

    return gmem;

failed:
    if(gmem) {
        g_object_unref(gmem);
    }else if(dmabuf_fd >= 0) {
        close(dmabuf_fd);
    }

    return NULL;
}

// mem1 is presumed to be lower than mem2
static gboolean
gst_vpusmm_is_span(GstMemory * mem1, GstMemory * mem2, gsize * offset)
{
    if(gst_dmabuf_memory_get_fd(mem1) != gst_dmabuf_memory_get_fd(mem2))
        return FALSE;

    if (offset) {
        *offset = mem1->offset - mem1->parent->offset;
    }

    return mem1->offset + mem1->size == mem2->offset;
}

static GstMemory *
gst_vpusmm_mem_share (GstMemory * mem, gssize offset, gssize size)
{
    GstMemory *sub = NULL;
    GstMemory *parent;
    sub = gst_dmabuf_allocator_alloc (mem->allocator, gst_dmabuf_memory_get_fd(mem), mem->maxsize);
    if(sub){
        parent = mem->parent ? mem->parent: mem;
        gst_memory_init (GST_MEMORY_CAST (sub),
                         GST_MINI_OBJECT_FLAGS (parent),
                          mem->allocator,
                          parent, // there is always a parent for submem, so gst_memory_init() will lock
                                  // the parent with GST_LOCK_FLAG_EXCLUSIVE
                          mem->maxsize,
                          mem->align,
                          mem->offset + offset,
                          size < 0?(mem->size - offset): size);
    }
    return sub;
}

static void
gst_vpusmm_allocator_finalize (GObject * object)
{
    GstVPUSMMAllocator *const alloc = GST_VPUSMM_ALLOCATOR_CAST (object);
    (void)alloc;

    G_OBJECT_CLASS (gst_vpusmm_allocator_parent_class)->finalize (object);
}


static void
gst_vpusmm_allocator_class_init (GstVPUSMMAllocatorClass * klass)
{
    GObjectClass *const object_class = G_OBJECT_CLASS (klass);
    GstAllocatorClass *const allocator_class = GST_ALLOCATOR_CLASS (klass);

    object_class->finalize = gst_vpusmm_allocator_finalize;
    allocator_class->alloc = gst_vpusmm_allocator_alloc;
}

static void
gst_vpusmm_allocator_init (GstVPUSMMAllocator * allocator)
{
    GstAllocator *alloc = GST_ALLOCATOR_CAST (allocator);

    alloc->mem_is_span = gst_vpusmm_is_span;
    alloc->mem_share = gst_vpusmm_mem_share;
}

/**
 * gst_vpusmm_allocator_new:
 *
 * Return a new vpusmm allocator.
 *
 * Returns: a new vpusmm allocator, or NULL if the allocator
 *    isn't available. Use gst_object_unref() to release the allocator after
 *    usage
 *
 */
GstAllocator *
gst_vpusmm_allocator_new (int mem_type)
{
    GstVPUSMMAllocator *alloc = NULL;

    GST_DEBUG_CATEGORY_INIT (vpusmm_debug, "vpusmm",
                             2, "vpusmm allocator");

    alloc = g_object_new (GST_TYPE_VPUSMM_ALLOCATOR, NULL);

    gst_object_ref_sink (alloc);

    alloc->align_size = getpagesize();
    alloc->mem_type = mem_type;

    return GST_ALLOCATOR_CAST(alloc);
}

