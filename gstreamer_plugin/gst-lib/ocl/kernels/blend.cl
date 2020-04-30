/*
 *Copyright (C) 2018 Intel Corporation
 *
 *SPDX-License-Identifier: LGPL-2.1-only
 *
 *This library is free software; you can redistribute it and/or modify it under the terms
 * of the GNU Lesser General Public License as published by the Free Software Foundation;
 * version 2.1.
 *
 *This library is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License along with this library;
 * if not, write to the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */


/*
 * BGRA + BGRA
 * The same image size
 */


static const __constant float CV_8U_MAX         = 255.0f;
static const __constant float CV_8U_HALF        = 128.0f;
static const __constant float d2                = CV_8U_HALF / CV_8U_MAX;
static const __constant float4 yblue		= 65.f/CV_8U_MAX;


typedef struct BoundingBox{
  int          x;
  int          y;
  int          width;
  int          height;
  char*        label;
  unsigned long pts;
  double       probability;
}BoundingBox;

__kernel void blend(read_only image2d_t src_y,
                     read_only image2d_t src_uv,
                     __global unsigned char* src_osd,
                     write_only image2d_t dst_y,
                     write_only image2d_t dst_uv,
		     int dst_w, int dst_h){

    int id_x = get_global_id(0);
    int id_y = get_global_id(1);
    int id_z =  id_x * 2;
    int id_w =  id_y * 2;


    if (id_z >= dst_w || id_w >= dst_h)
        return;

    float4 Y0, Y1, Y2, Y3, UV;
    sampler_t sampler = CLK_NORMALIZED_COORDS_FALSE | CLK_ADDRESS_CLAMP_TO_EDGE | CLK_FILTER_NEAREST;
    Y0 = read_imagef(src_y, sampler, (int2)(id_z    , id_w));
    Y1 = read_imagef(src_y, sampler, (int2)(id_z + 1, id_w));
    Y2 = read_imagef(src_y, sampler, (int2)(id_z    , id_w + 1));
    Y3 = read_imagef(src_y, sampler, (int2)(id_z + 1, id_w + 1));
    UV = read_imagef(src_uv,sampler,(int2)(id_z/2  , id_w / 2));

    write_imagef(dst_y, (int2)(id_z, id_w),Y0);
    write_imagef(dst_y, (int2)(id_z+1, id_w),Y1);
    write_imagef(dst_y, (int2)(id_z, id_w+1),Y2);
    write_imagef(dst_y, (int2)(id_z+1, id_w+1),Y3);
    write_imagef(dst_uv, (int2)(id_z/2, id_w/2),UV);

    __global uchar* pOsdRow1 = src_osd + (id_w * dst_w + id_z) ;
    __global uchar* pOsdRow2 = pOsdRow1 + dst_w ;
    if((pOsdRow1[0] | pOsdRow1[1] | pOsdRow2[0] | pOsdRow2[1])){
	  write_imagef(dst_y, (int2)(id_z, id_w),yblue);
	  write_imagef(dst_y, (int2)(id_z, id_w+1),yblue);
	  write_imagef(dst_y, (int2)(id_z+1, id_w),yblue);
	  write_imagef(dst_y, (int2)(id_z+1, id_w+1),yblue);
      write_imagef(dst_uv, (int2)(id_z/2, id_w/2),(float4)0.f);
    }
}
