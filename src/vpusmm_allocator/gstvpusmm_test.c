/* *
 * Copyright (C) 2019 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

#include <stdio.h>
#include <gst/check/gstcheck.h>

// libtool --mode=link gcc `pkg-config --cflags --libs gstreamer-1.0-uninstalled gstreamer-plugins-base-1.0-uninstalled libdrm_intel` -lgstallocators-1.0 -lgstcheck-1.0 -g -o gstmem_check gstmem_check.c gstdrmbomemory.c
// libtool --mode=link gcc `pkg-config --cflags --libs gstreamer-1.0 gstreamer-plugins-base-1.0 libdrm_hantro` -lgstallocators-1.0 -lgstcheck-1.0 -g -o gstmem_check gstmem_check.c gstdrmbomemory.c


#define DEBUG_SHORTCUT 0

#if DEBUG_SHORTCUT == 1
#define GST_START_TEST(__name) static void __name()
#define GST_END_TEST
#define fail_if(a, ...)     if(a)   {printf("===%s:%d  fail_if(%s, ... \n", __FUNCTION__, __LINE__, #a); exit(0);}
#define fail_unless(a, ...) if(!(a)){printf("===%s:%d  fail_unless(%s, ...\n", __FUNCTION__, __LINE__, #a); exit(0);}
#define ASSERT_CRITICAL(a) a
#endif


#define TRACE() fprintf(stderr, "TRACE  %s:%d\n", __FILE__, __LINE__);

GST_START_TEST (test_submemory)
{
  GstMemory *memory, *sub;
  GstMapInfo info, sinfo;

  memory = gst_allocator_alloc (NULL, 4, NULL);

  /* check sizes, memory starts out empty */
  fail_unless (gst_memory_map (memory, &info, GST_MAP_WRITE));
  fail_unless (info.size == 4, "memory has wrong size");
  fail_unless (info.maxsize >= 4, "memory has wrong size");
  memset (info.data, 0, 4);
  gst_memory_unmap (memory, &info);

  fail_unless (gst_memory_map (memory, &info, GST_MAP_READ));

  sub = gst_memory_share (memory, 1, 2);
  fail_if (sub == NULL, "share of memory returned NULL");

  fail_unless (gst_memory_map (sub, &sinfo, GST_MAP_READ));
  fail_unless (sinfo.size == 2, "submemory has wrong size");
  fail_unless (memcmp (info.data + 1, sinfo.data, 2) == 0,
      "submemory contains the wrong data");
  ASSERT_MINI_OBJECT_REFCOUNT (sub, "submemory", 1);
  gst_memory_unmap (sub, &sinfo);
  gst_memory_unref (sub);

  /* create a submemory of size 0 */
  sub = gst_memory_share (memory, 1, 0);
  fail_if (sub == NULL, "share memory returned NULL");
  fail_unless (gst_memory_map (sub, &sinfo, GST_MAP_READ));
  fail_unless (sinfo.size == 0, "submemory has wrong size");
  fail_unless (memcmp (info.data + 1, sinfo.data, 0) == 0,
      "submemory contains the wrong data");
  ASSERT_MINI_OBJECT_REFCOUNT (sub, "submemory", 1);
  gst_memory_unmap (sub, &sinfo);
  gst_memory_unref (sub);

  /* test if metadata is copied, not a complete memory copy so only the
   * timestamp and offset fields are copied. */
  sub = gst_memory_share (memory, 0, 1);
  fail_if (sub == NULL, "share of memory returned NULL");
  fail_unless (gst_memory_get_sizes (sub, NULL, NULL) == 1,
      "submemory has wrong size");
  gst_memory_unref (sub);

  /* test if metadata is copied, a complete memory is copied so all the timing
   * fields should be copied. */
  sub = gst_memory_share (memory, 0, 4);
  fail_if (sub == NULL, "share of memory returned NULL");
  fail_unless (gst_memory_get_sizes (sub, NULL, NULL) == 4,
      "submemory has wrong size");

  /* clean up */
  gst_memory_unref (sub);

  gst_memory_unmap (memory, &info);

  /* test write map + share failure */
  fail_unless (gst_memory_map (memory, &info, GST_MAP_WRITE));
  sub = gst_memory_share (memory, 0, 4);
  fail_unless (sub == NULL, "share with a write map succeeded");

  gst_memory_unmap (memory, &info);
  gst_memory_unref (memory);
}

GST_END_TEST;

GST_START_TEST (test_is_span)
{
  GstMemory *memory, *sub1, *sub2;

  memory = gst_allocator_alloc (NULL, 4, NULL);

  sub1 = gst_memory_share (memory, 0, 2);
  fail_if (sub1 == NULL, "share of memory returned NULL");

  sub2 = gst_memory_share (memory, 2, 2);
  fail_if (sub2 == NULL, "share of memory returned NULL");

  fail_if (gst_memory_is_span (memory, sub2, NULL) == TRUE,
      "a parent memory can't be span");

  fail_if (gst_memory_is_span (sub1, memory, NULL) == TRUE,
      "a parent memory can't be span");

  fail_if (gst_memory_is_span (sub1, sub2, NULL) == FALSE,
      "two submemorys next to each other should be span");

  /* clean up */
  gst_memory_unref (sub1);
  gst_memory_unref (sub2);
  gst_memory_unref (memory);
}

GST_END_TEST;



static const char ro_memory[] = "abcdefghijklmnopqrstuvwxyz";

static GstMemory *
create_read_only_memory (void)
{
  GstMemory *mem;

  /* assign some read-only data to the new memory */
  mem = gst_memory_new_wrapped (GST_MEMORY_FLAG_READONLY,
      (gpointer) ro_memory, sizeof (ro_memory), 0, sizeof (ro_memory), NULL,
      NULL);
  fail_unless (GST_MEMORY_IS_READONLY (mem));

  return mem;
}

GST_START_TEST (test_writable)
{
  GstMemory *mem, *mem2;
  GstMapInfo info;

  /* create read-only memory and try to write */
  mem = create_read_only_memory ();

  fail_if (gst_memory_map (mem, &info, GST_MAP_WRITE));

  /* Make sure mapping anxd unmapping it doesn't change it's locking state */
  fail_unless (gst_memory_map (mem, &info, GST_MAP_READ));
  gst_memory_unmap (mem, &info);

  fail_if (gst_memory_map (mem, &info, GST_MAP_WRITE));

  mem2 = gst_memory_copy (mem, 0, -1);
  fail_unless (GST_MEMORY_IS_READONLY (mem));
  fail_if (GST_MEMORY_IS_READONLY (mem2));

  fail_unless (gst_memory_map (mem2, &info, GST_MAP_WRITE));
  info.data[4] = 'a';
  gst_memory_unmap (mem2, &info);

  gst_memory_ref (mem2);
  fail_if (gst_memory_map (mem, &info, GST_MAP_WRITE));
  gst_memory_unref (mem2);

  fail_unless (gst_memory_map (mem2, &info, GST_MAP_WRITE));
  info.data[4] = 'a';
  gst_memory_unmap (mem2, &info);
  gst_memory_unref (mem2);

  gst_memory_unref (mem);
}

GST_END_TEST;

GST_START_TEST (test_submemory_writable)
{
  GstMemory *mem, *sub_mem;
  GstMapInfo info;

  /* create sub-memory of read-only memory and try to write */
  mem = create_read_only_memory ();

  sub_mem = gst_memory_share (mem, 0, 8);
  fail_unless (GST_MEMORY_IS_READONLY (sub_mem));

  fail_if (gst_memory_map (mem, &info, GST_MAP_WRITE));
  fail_if (gst_memory_map (sub_mem, &info, GST_MAP_WRITE));

  gst_memory_unref (sub_mem);
  gst_memory_unref (mem);
}

GST_END_TEST;

GST_START_TEST (test_copy)
{
  GstMemory *memory, *copy;
  GstMapInfo info, sinfo;

  memory = gst_allocator_alloc (NULL, 4, NULL);
  ASSERT_MINI_OBJECT_REFCOUNT (memory, "memory", 1);

  copy = gst_memory_copy (memory, 0, -1);
  ASSERT_MINI_OBJECT_REFCOUNT (memory, "memory", 1);
  ASSERT_MINI_OBJECT_REFCOUNT (copy, "copy", 1);
  /* memory is copied and must point to different memory */
  fail_if (memory == copy);

  fail_unless (gst_memory_map (memory, &info, GST_MAP_READ));
  fail_unless (gst_memory_map (copy, &sinfo, GST_MAP_READ));

  /* NOTE that data is refcounted */
  fail_unless (info.size == sinfo.size);

  gst_memory_unmap (copy, &sinfo);
  gst_memory_unmap (memory, &info);

  gst_memory_unref (copy);
  gst_memory_unref (memory);

  memory = gst_allocator_alloc (NULL, 0, NULL);
  fail_unless (gst_memory_map (memory, &info, GST_MAP_READ));
  fail_unless (info.size == 0);
  gst_memory_unmap (memory, &info);

  /* copying a 0-sized memory should not crash */
  copy = gst_memory_copy (memory, 0, -1);
  fail_unless (gst_memory_map (copy, &info, GST_MAP_READ));
  fail_unless (info.size == 0);
  gst_memory_unmap (copy, &info);

  gst_memory_unref (copy);
  gst_memory_unref (memory);
}

GST_END_TEST;

GST_START_TEST (test_try_new_and_alloc)
{
  GstMemory *mem;
  GstMapInfo info;
  gsize size;

  mem = gst_allocator_alloc (NULL, 0, NULL);
  fail_unless (mem != NULL);
  fail_unless (gst_memory_map (mem, &info, GST_MAP_READ));
  fail_unless (info.size == 0);
  gst_memory_unmap (mem, &info);
  gst_memory_unref (mem);

  /* normal alloc should still work */
  size = 640 * 480 * 4;
  mem = gst_allocator_alloc (NULL, size, NULL);
  fail_unless (mem != NULL);
  fail_unless (gst_memory_map (mem, &info, GST_MAP_WRITE));
  fail_unless (info.data != NULL);
  fail_unless (info.size == (640 * 480 * 4));
  info.data[640 * 479 * 4 + 479] = 0xff;
  gst_memory_unmap (mem, &info);

  gst_memory_unref (mem);
}

GST_END_TEST;

GST_START_TEST (test_resize)
{
  GstMemory *mem;
  gsize maxalloc;
  gsize size, maxsize, offset;

  /* one memory block */
  mem = gst_allocator_alloc (NULL, 100, NULL);

  size = gst_memory_get_sizes (mem, &offset, &maxalloc);
  fail_unless (size == 100);
  fail_unless (offset == 0);
  fail_unless (maxalloc >= 100);
#if 0
  // these tests are not good for dmabuf type
  ASSERT_CRITICAL (gst_memory_resize (mem, 200, 50));
  ASSERT_CRITICAL (gst_memory_resize (mem, 0, 150));
  ASSERT_CRITICAL (gst_memory_resize (mem, 1, maxalloc));
  ASSERT_CRITICAL (gst_memory_resize (mem, maxalloc, 1));
#endif
  /* this does nothing */
  gst_memory_resize (mem, 0, 100);

  /* nothing should have changed */
  size = gst_memory_get_sizes (mem, &offset, &maxsize);
  fail_unless (size == 100);
  fail_unless (offset == 0);
  fail_unless (maxsize == maxalloc);

  gst_memory_resize (mem, 0, 50);
  size = gst_memory_get_sizes (mem, &offset, &maxsize);
  fail_unless (size == 50);
  fail_unless (offset == 0);
  fail_unless (maxsize == maxalloc);

  gst_memory_resize (mem, 0, 100);
  size = gst_memory_get_sizes (mem, &offset, &maxsize);
  fail_unless (size == 100);
  fail_unless (offset == 0);
  fail_unless (maxsize == maxalloc);

  gst_memory_resize (mem, 1, 99);
  size = gst_memory_get_sizes (mem, &offset, &maxsize);
  fail_unless (size == 99);
  fail_unless (offset == 1);
  fail_unless (maxsize == maxalloc);

  ASSERT_CRITICAL (gst_memory_resize (mem, 1, maxalloc - 1));

  gst_memory_resize (mem, 0, 99);
  size = gst_memory_get_sizes (mem, &offset, &maxsize);
  fail_unless (size == 99);
  fail_unless (offset == 1);
  fail_unless (maxsize == maxalloc);

  gst_memory_resize (mem, -1, 100);
  size = gst_memory_get_sizes (mem, &offset, &maxsize);
  fail_unless (size == 100);
  fail_unless (offset == 0);
  fail_unless (maxsize == maxalloc);

  /* can't set offset below 0 */
  ASSERT_CRITICAL (gst_memory_resize (mem, -1, 100));

  gst_memory_resize (mem, 50, 40);
  size = gst_memory_get_sizes (mem, &offset, &maxsize);
  fail_unless (size == 40);
  fail_unless (offset == 50);
  fail_unless (maxsize == maxalloc);

  gst_memory_resize (mem, -50, 100);
  size = gst_memory_get_sizes (mem, &offset, &maxsize);
  fail_unless (size == 100);
  fail_unless (offset == 0);
  fail_unless (maxsize == maxalloc);

  gst_memory_resize (mem, 0, 0);
  size = gst_memory_get_sizes (mem, &offset, &maxsize);
  fail_unless (size == 0);
  fail_unless (offset == 0);
  fail_unless (maxsize == maxalloc);

  gst_memory_resize (mem, 0, 100);
  size = gst_memory_get_sizes (mem, &offset, &maxsize);
  fail_unless (size == 100);
  fail_unless (offset == 0);
  fail_unless (maxsize == maxalloc);

  gst_memory_resize (mem, 0, 100);
  size = gst_memory_get_sizes (mem, &offset, &maxsize);
  fail_unless (size == 100);
  fail_unless (offset == 0);
  fail_unless (maxsize == maxalloc);

  gst_memory_unref (mem);
}

GST_END_TEST;

GST_START_TEST (test_map)
{
  GstMemory *mem;
  GstMapInfo info;
  gsize maxalloc;
  gsize size, offset;

  /* one memory block */
  mem = gst_allocator_alloc (NULL, 100, NULL);

  size = gst_memory_get_sizes (mem, &offset, &maxalloc);
  fail_unless (size == 100);
  fail_unless (offset == 0);
  fail_unless (maxalloc >= 100);

  /* see if simply mapping works */
  fail_unless (gst_memory_map (mem, &info, GST_MAP_READ));
  fail_unless (info.data != NULL);
  fail_unless (info.size == 100);
  fail_unless (info.maxsize == maxalloc);

  gst_memory_unmap (mem, &info);
  gst_memory_unref (mem);
}

GST_END_TEST;

GST_START_TEST (test_map_nested)
{
  GstMemory *mem;
  GstMapInfo info1, info2;

  mem = gst_allocator_alloc (NULL, 100, NULL);

  /* nested mapping */
  fail_unless (gst_memory_map (mem, &info1, GST_MAP_READ));
  fail_unless (info1.data != NULL);
  fail_unless (info1.size == 100);

  fail_unless (gst_memory_map (mem, &info2, GST_MAP_READ));
  fail_unless (info2.data == info1.data);
  fail_unless (info2.size == 100);

  /* unmap */
  gst_memory_unmap (mem, &info2);
  gst_memory_unmap (mem, &info1);

  fail_unless (gst_memory_map (mem, &info1, GST_MAP_READ));
  /* not allowed */
  fail_if (gst_memory_map (mem, &info2, GST_MAP_WRITE));
  fail_if (gst_memory_map (mem, &info2, GST_MAP_READWRITE));
  fail_unless (gst_memory_map (mem, &info2, GST_MAP_READ));
  gst_memory_unmap (mem, &info2);
  gst_memory_unmap (mem, &info1);

  fail_unless (gst_memory_map (mem, &info1, GST_MAP_WRITE));
  /* not allowed */
  fail_if (gst_memory_map (mem, &info2, GST_MAP_READ));
  fail_if (gst_memory_map (mem, &info2, GST_MAP_READWRITE));
  fail_unless (gst_memory_map (mem, &info2, GST_MAP_WRITE));
  gst_memory_unmap (mem, &info1);
  gst_memory_unmap (mem, &info2);
  /* nothing was mapped */
  ASSERT_CRITICAL (gst_memory_unmap (mem, &info2));

  fail_unless (gst_memory_map (mem, &info1, GST_MAP_READWRITE));
  fail_unless (gst_memory_map (mem, &info2, GST_MAP_READ));
  gst_memory_unmap (mem, &info2);
  fail_unless (gst_memory_map (mem, &info2, GST_MAP_WRITE));
  gst_memory_unmap (mem, &info2);
  gst_memory_unmap (mem, &info1);
  /* nothing was mapped */
  ASSERT_CRITICAL (gst_memory_unmap (mem, &info1));

  gst_memory_unref (mem);
}

GST_END_TEST;

GST_START_TEST (test_map_resize)
{
  GstMemory *mem;
  GstMapInfo info;
  gsize size, maxalloc, offset;

  mem = gst_allocator_alloc (NULL, 100, NULL);

  /* do mapping */
  fail_unless (gst_memory_map (mem, &info, GST_MAP_READ));
  fail_unless (info.data != NULL);
  fail_unless (info.size == 100);

  /* resize the buffer */
  gst_memory_resize (mem, 1, info.size - 1);
  size = gst_memory_get_sizes (mem, &offset, &maxalloc);
  fail_unless (size == 99);
  fail_unless (offset == 1);
  fail_unless (maxalloc >= 100);
  gst_memory_unmap (mem, &info);

  size = gst_memory_get_sizes (mem, &offset, &maxalloc);
  fail_unless (size == 99);
  fail_unless (offset == 1);
  fail_unless (maxalloc >= 100);

  fail_unless (gst_memory_map (mem, &info, GST_MAP_READ));
  fail_unless (info.data != NULL);
  fail_unless (info.size == 99);
  fail_unless (info.maxsize >= 100);
  gst_memory_unmap (mem, &info);

  /* and larger */
  fail_unless (gst_memory_map (mem, &info, GST_MAP_READ));
  gst_memory_resize (mem, -1, 100);
  gst_memory_unmap (mem, &info);

  size = gst_memory_get_sizes (mem, &offset, &maxalloc);
  fail_unless (size == 100);
  fail_unless (offset == 0);
  fail_unless (maxalloc >= 100);

  fail_unless (gst_memory_map (mem, &info, GST_MAP_READ));
  gst_memory_unmap (mem, &info);
  gst_memory_unref (mem);
}

GST_END_TEST;

GST_START_TEST (test_alloc_params)
{
  GstMemory *mem;
  GstMapInfo info;
  gsize size, offset, maxalloc;
  GstAllocationParams params;
  guint8 arr[10];

  memset (arr, 0, 10);

  gst_allocation_params_init (&params);
  params.padding = 10;
  params.prefix = 10;
  params.flags = GST_MEMORY_FLAG_ZERO_PREFIXED | GST_MEMORY_FLAG_ZERO_PADDED;
  mem = gst_allocator_alloc (NULL, 100, &params);

  /*Checking size and offset */
  size = gst_memory_get_sizes (mem, &offset, &maxalloc);
  fail_unless (size == 100);
  fail_unless (offset == 10);
  fail_unless (maxalloc >= 120);

  fail_unless (GST_MEMORY_FLAG_IS_SET (mem, GST_MEMORY_FLAG_ZERO_PREFIXED));
  fail_unless (GST_MEMORY_FLAG_IS_SET (mem, GST_MEMORY_FLAG_ZERO_PADDED));

  fail_unless (gst_memory_map (mem, &info, GST_MAP_READ));
  fail_unless (info.data != NULL);
  fail_unless (info.size == 100);

  /*Checking prefix */
  fail_unless (memcmp (info.data - 10, arr, 10) == 0);

  /*Checking padding */
  fail_unless (memcmp (info.data + 100, arr, 10) == 0);


  gst_memory_unmap (mem, &info);
  gst_memory_unref (mem);
}

GST_END_TEST;

GST_START_TEST (test_lock)
{
  GstMemory *mem;

  mem = gst_allocator_alloc (NULL, 10, NULL);
  fail_unless (mem != NULL);

  /* test exclusivity */
  fail_unless (gst_memory_lock (mem,
          GST_LOCK_FLAG_WRITE | GST_LOCK_FLAG_EXCLUSIVE));
  fail_if (gst_memory_lock (mem, GST_LOCK_FLAG_EXCLUSIVE));
  fail_unless (gst_memory_lock (mem, GST_LOCK_FLAG_WRITE));
  gst_memory_unlock (mem, GST_LOCK_FLAG_WRITE | GST_LOCK_FLAG_EXCLUSIVE);
  gst_memory_unlock (mem, GST_LOCK_FLAG_WRITE);

  /* no lock here */

  fail_unless (gst_memory_lock (mem,
          GST_LOCK_FLAG_READ | GST_LOCK_FLAG_EXCLUSIVE));
  fail_unless (gst_memory_lock (mem,
          GST_LOCK_FLAG_READ | GST_LOCK_FLAG_EXCLUSIVE));
  gst_memory_unlock (mem, GST_LOCK_FLAG_READ | GST_LOCK_FLAG_EXCLUSIVE);
  gst_memory_unlock (mem, GST_LOCK_FLAG_READ | GST_LOCK_FLAG_EXCLUSIVE);

  /* no lock here */

  fail_unless (gst_memory_lock (mem,
          GST_LOCK_FLAG_READWRITE | GST_LOCK_FLAG_EXCLUSIVE));
  fail_unless (gst_memory_lock (mem, GST_LOCK_FLAG_READ));
  fail_if (gst_memory_lock (mem, GST_LOCK_FLAG_READ | GST_LOCK_FLAG_EXCLUSIVE));
  fail_if (gst_memory_lock (mem, GST_LOCK_FLAG_EXCLUSIVE));
  fail_unless (gst_memory_lock (mem, GST_LOCK_FLAG_WRITE));
  gst_memory_unlock (mem, GST_LOCK_FLAG_WRITE);
  gst_memory_unlock (mem, GST_LOCK_FLAG_READ);
  gst_memory_unlock (mem, GST_LOCK_FLAG_READWRITE | GST_LOCK_FLAG_EXCLUSIVE);

  gst_memory_unref (mem);
}

GST_END_TEST;

static Suite *
gst_memory_suite (void)
{
  Suite *s = suite_create ("GstMemory");
  TCase *tc_chain = tcase_create ("general");

  suite_add_tcase (s, tc_chain);

  tcase_add_test (tc_chain, test_submemory);
  tcase_add_test (tc_chain, test_submemory_writable);
  tcase_add_test (tc_chain, test_writable);
  tcase_add_test (tc_chain, test_is_span);
  tcase_add_test (tc_chain, test_copy);
  tcase_add_test (tc_chain, test_try_new_and_alloc);
  tcase_add_test (tc_chain, test_resize);
  tcase_add_test (tc_chain, test_map);
  tcase_add_test (tc_chain, test_map_nested);
  tcase_add_test (tc_chain, test_map_resize);
  tcase_add_test (tc_chain, test_alloc_params);
  tcase_add_test (tc_chain, test_lock);
  return s;
}


//
//=============================================================================================================



//=============================================================================================================

#include <gst/gstallocator.h>
#include <gst/allocators/allocators.h>
#include "gstvpusmm.h"


void simple_test(gsize size, int n)
{
  GstMemory *memory[1024];
  GstMapInfo info;
  int i;

  if(size < 1) size = 1;
  if(n < 1) n = 1;
  if(n > sizeof(memory)/sizeof(memory[0]))
    n = sizeof(memory)/sizeof(memory[0]);

  for(i=0;i<n;i++){
    memory[i] = gst_allocator_alloc (NULL, size, NULL);
    if(memory[i] == NULL){
      g_print("gst_allocator_alloc( size=%lu ) %dth failed\n", size, i);
      return;
    }else{
      g_print("gst_allocator_alloc( size=%lu ) %dth succeeded\n", size, i);
    }
  }

  g_print("Press any key to do map/unmap test\n");
  getchar();

  for(i=0;i<n;i++){
    if(gst_memory_map (memory[i], &info, GST_MAP_WRITE)){
      info.data[0] = 0;
      info.data[1] = 1;
      info.data[2] = 2;
      info.data[3] = 3;
      g_print("gst_memory_map %dth succeeded: %p      %x,%x,%x,%x\n", i, memory,
              info.data[0], info.data[1],info.data[2], info.data[3]);
      gst_memory_unmap (memory[i], &info);
    }else{
      g_print("gst_memory_map %dth failed: %p\n", i, memory);
    }
  }

  for(i=0;i<n;i++){
    gst_memory_unref (memory[i]);
  }
}

//GST_CHECK_MAIN (gst_memory);
int main (int argc, char **argv)
{
  Suite *s;
  GstAllocator * allocator;
  int ret;
  gst_check_init (&argc, &argv);

  allocator = gst_vpusmm_allocator_new(0);
  if(allocator == NULL) {
    g_print("gst_vpusmm_allocator_new() failed!");
    goto exit_now;
  }

  //Set the default allocator. This function takes ownership of allocator
  gst_allocator_set_default(allocator);

  if(argc > 1){
      int sz = atoi(argv[1]);
      int cnt = 1;
      if(argc > 2)
        cnt = atoi(argv[2]);
      simple_test(sz, cnt);
      return 0;
  }


#if DEBUG_SHORTCUT == 1
  test_copy();
  test_resize();
  test_is_span();
return;
#endif

  s = gst_memory_suite ();
  ret = gst_check_run_suite (s, "gst_memory", __FILE__);



exit_now:
  return ret;
}