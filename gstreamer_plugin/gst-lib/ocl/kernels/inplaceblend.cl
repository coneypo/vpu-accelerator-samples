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




/*

__kernel void myblend(write_only image2d_t src_y,
                     write_only image2d_t src_uv,
		     __global BoundingBox *box,
		     int num,
		     int dst_w, int dst_h){
 
    int id_x = get_global_id(0);
    int id_y = get_global_id(1);
    int id_z =  id_x * 2;
    int id_w =  id_y * 2;

    if (id_z >= dst_w || id_w >= dst_h)
        return;
    
    
    //if(id_x ==0 && id_y == 0){
   // 	printf(" image y type: %x, width:%d, height:%d\n",get_image_channel_data_type(src_y),get_image_width(src_y), get_image_height(src_y));
   // 	printf(" image uv type: %x, width:%d, height:%d\n",get_image_channel_data_type(src_uv),get_image_width(src_uv), get_image_height(src_uv));
   // 	printf(" image channel order:%x, dim x:%d, dim y:%d \n",get_image_channel_order(src_y), get_image_dim(src_y).x, get_image_dim(src_y).y);
   // }

    for(int i=0; i<num; i++){
      if(id_z == box[i].x || id_z+1 == box[i].x){
	if(id_w >= box[i].y && id_w < box[i].y+box[i].height){
	  write_imagef(src_y, (int2)(id_z, id_w),65.f/CV_8U_MAX);
	  write_imagef(src_y, (int2)(id_z, id_w+1),65.f/CV_8U_MAX);
	  write_imagef(src_y, (int2)(id_z+1, id_w),65.f/CV_8U_MAX);
	  write_imagef(src_y, (int2)(id_z+1, id_w+1),65.f/CV_8U_MAX);

	  write_imagef(src_y, (int2)(id_z+box[i].width, id_w),65.f/CV_8U_MAX);
	  write_imagef(src_y, (int2)(id_z+box[i].width, id_w+1),65.f/CV_8U_MAX);
	  write_imagef(src_y, (int2)(id_z+1+box[i].width, id_w),65.f/CV_8U_MAX);
	  write_imagef(src_y, (int2)(id_z+1+box[i].width, id_w+1),65.f/CV_8U_MAX);

          write_imagef(src_uv, (int2)(id_z/2, id_w/2),0.f/224.f);
          write_imagef(src_uv, (int2)(id_z/2, id_w/2+1),0.f);

          write_imagef(src_uv, (int2)((id_z+box[i].width)/2, id_w/2),0.f);
          write_imagef(src_uv, (int2)((id_z+box[i].width)/2, id_w/2+1),0.f);
	}
      }
      if(id_w == box[i].y || id_w+1 == box[i].y){
	if(id_z >= box[i].x && id_z < box[i].x+box[i].width){
	  write_imagef(src_y, (int2)(id_z, id_w),65.f/CV_8U_MAX);
	  write_imagef(src_y, (int2)(id_z, id_w+1),65.f/CV_8U_MAX);
	  write_imagef(src_y, (int2)(id_z+1, id_w),65.f/CV_8U_MAX);
	  write_imagef(src_y, (int2)(id_z+1, id_w+1),65.f/CV_8U_MAX);

	  write_imagef(src_y, (int2)(id_z, id_w+box[i].height),65.f/CV_8U_MAX);
	  write_imagef(src_y, (int2)(id_z, id_w+1+box[i].height),65.f/CV_8U_MAX);
	  write_imagef(src_y, (int2)(id_z+1, id_w+box[i].height),65.f/CV_8U_MAX);
	  write_imagef(src_y, (int2)(id_z+1, id_w+1+box[i].height),65.f/CV_8U_MAX);


          write_imagef(src_uv, (int2)(id_z/2, id_w/2),0.f);
          write_imagef(src_uv, (int2)(id_z/2, id_w/2+1),0.f);

          write_imagef(src_uv, (int2)(id_z/2, (id_w+box[i].height)/2),0.f);
          write_imagef(src_uv, (int2)(id_z/2, (id_w+box[i].height)/2+1),0.f);
	}
      }



    }
}

*/


/*
__kernel void blend(write_only image2d_t src_y,
                     write_only image2d_t src_uv,
                     __global unsigned char* src_osd,
		     int dst_w, int dst_h){
 
    int id_x = get_global_id(0);
    int id_y = get_global_id(1);
    int id_z =  id_x * 2;
    int id_w =  id_y * 2;


    if (id_z >= dst_w || id_w >= dst_h)
        return;
    __global uchar* pOsdRow1 = src_osd + (id_w * dst_w + id_z) ;
    __global uchar* pOsdRow2 = pOsdRow1 + dst_w ;
    if((pOsdRow1[0] | pOsdRow1[1] | pOsdRow2[0] | pOsdRow2[1]) == 255 ){
	  write_imagef(src_y, (int2)(id_z, id_w),yblue);
	  write_imagef(src_y, (int2)(id_z, id_w+1),yblue);
	  write_imagef(src_y, (int2)(id_z+1, id_w),yblue);
	  write_imagef(src_y, (int2)(id_z+1, id_w+1),yblue);
          write_imagef(src_uv, (int2)(id_z/2, id_w/2),(float4)0.f);
    }
    
}

*/

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
    if((pOsdRow1[0] | pOsdRow1[1] | pOsdRow2[0] | pOsdRow2[1]) == 255 ){
	  write_imagef(dst_y, (int2)(id_z, id_w),yblue);
	  write_imagef(dst_y, (int2)(id_z, id_w+1),yblue);
	  write_imagef(dst_y, (int2)(id_z+1, id_w),yblue);
	  write_imagef(dst_y, (int2)(id_z+1, id_w+1),yblue);
          write_imagef(dst_uv, (int2)(id_z/2, id_w/2),(float4)0.f);
    }
 
    
}
