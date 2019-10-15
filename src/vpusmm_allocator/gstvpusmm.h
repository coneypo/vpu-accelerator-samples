/* *
 * Copyright (C) 2019 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

#ifndef GST_VPUSMM_H
#define GST_VPUSMM_H

#include <gst/gstallocator.h>
#include <gst/allocators/allocators.h>

G_BEGIN_DECLS

typedef struct _GstVPUSMMAllocator GstVPUSMMAllocator;
typedef struct _GstVPUSMMAllocatorClass GstVPUSMMAllocatorClass;


#define GST_TYPE_VPUSMM_ALLOCATOR              (gst_vpusmm_allocator_get_type())
#define GST_IS_VPUSMM_ALLOCATOR(obj)           (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GST_TYPE_VPUSMM_ALLOCATOR))
#define GST_VPUSMM_ALLOCATOR_CAST(allocator)   ((GstVPUSMMAllocator *) (allocator))

struct _GstVPUSMMAllocator
{
  GstDmaBufAllocator parent;

  /*< private >*/
  int align_size;
  int mem_type;
};

GType          gst_vpusmm_allocator_get_type (void);
GstAllocator * gst_vpusmm_allocator_new (int mem_type);

struct _GstVPUSMMAllocatorClass
{
  GstDmaBufAllocatorClass parent_class;

  /*< private >*/
  gpointer _gst_reserved[GST_PADDING];
};

G_END_DECLS

#endif /* GST_VPUSMM_H */
