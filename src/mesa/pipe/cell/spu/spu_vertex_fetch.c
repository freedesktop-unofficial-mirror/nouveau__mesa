/**************************************************************************
 * 
 * Copyright 2007 Tungsten Graphics, Inc., Cedar Park, Texas.
 * All Rights Reserved.
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sub license, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 * 
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.
 * IN NO EVENT SHALL TUNGSTEN GRAPHICS AND/OR ITS SUPPLIERS BE LIABLE FOR
 * ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 * 
 **************************************************************************/

 /*
  * Authors:
  *   Keith Whitwell <keith@tungstengraphics.com>
  */

#include <spu_mfcio.h>
#include "pipe/p_util.h"
#include "pipe/p_state.h"
#include "pipe/p_shader_tokens.h"
#include "spu_exec.h"
#include "spu_vertex_shader.h"
#include "spu_main.h"


#define DRAW_DBG 0


/**
 * Fetch a float[4] vertex attribute from memory, doing format/type
 * conversion as needed.
 *
 * This is probably needed/dupliocated elsewhere, eg format
 * conversion, texture sampling etc.
 */
#define FETCH_ATTRIB( NAME, SZ, CVT )			\
static void						\
fetch_##NAME(const void *ptr, float *attrib)		\
{							\
   static const float defaults[4] = { 0,0,0,1 };	\
   int i;						\
							\
   for (i = 0; i < SZ; i++) {				\
      attrib[i] = CVT;					\
   }							\
							\
   for (; i < 4; i++) {					\
      attrib[i] = defaults[i];				\
   }							\
}

#define CVT_64_FLOAT   (float) ((double *) ptr)[i]
#define CVT_32_FLOAT   ((float *) ptr)[i]

#define CVT_8_USCALED  (float) ((unsigned char *) ptr)[i]
#define CVT_16_USCALED (float) ((unsigned short *) ptr)[i]
#define CVT_32_USCALED (float) ((unsigned int *) ptr)[i]

#define CVT_8_SSCALED  (float) ((char *) ptr)[i]
#define CVT_16_SSCALED (float) ((short *) ptr)[i]
#define CVT_32_SSCALED (float) ((int *) ptr)[i]

#define CVT_8_UNORM    (float) ((unsigned char *) ptr)[i] / 255.0f
#define CVT_16_UNORM   (float) ((unsigned short *) ptr)[i] / 65535.0f
#define CVT_32_UNORM   (float) ((unsigned int *) ptr)[i] / 4294967295.0f

#define CVT_8_SNORM    (float) ((char *) ptr)[i] / 127.0f
#define CVT_16_SNORM   (float) ((short *) ptr)[i] / 32767.0f
#define CVT_32_SNORM   (float) ((int *) ptr)[i] / 2147483647.0f

FETCH_ATTRIB( R64G64B64A64_FLOAT,   4, CVT_64_FLOAT )
FETCH_ATTRIB( R64G64B64_FLOAT,      3, CVT_64_FLOAT )
FETCH_ATTRIB( R64G64_FLOAT,         2, CVT_64_FLOAT )
FETCH_ATTRIB( R64_FLOAT,            1, CVT_64_FLOAT )

FETCH_ATTRIB( R32G32B32A32_FLOAT,   4, CVT_32_FLOAT )
FETCH_ATTRIB( R32G32B32_FLOAT,      3, CVT_32_FLOAT )
FETCH_ATTRIB( R32G32_FLOAT,         2, CVT_32_FLOAT )
FETCH_ATTRIB( R32_FLOAT,            1, CVT_32_FLOAT )

FETCH_ATTRIB( R32G32B32A32_USCALED, 4, CVT_32_USCALED )
FETCH_ATTRIB( R32G32B32_USCALED,    3, CVT_32_USCALED )
FETCH_ATTRIB( R32G32_USCALED,       2, CVT_32_USCALED )
FETCH_ATTRIB( R32_USCALED,          1, CVT_32_USCALED )

FETCH_ATTRIB( R32G32B32A32_SSCALED, 4, CVT_32_SSCALED )
FETCH_ATTRIB( R32G32B32_SSCALED,    3, CVT_32_SSCALED )
FETCH_ATTRIB( R32G32_SSCALED,       2, CVT_32_SSCALED )
FETCH_ATTRIB( R32_SSCALED,          1, CVT_32_SSCALED )

FETCH_ATTRIB( R32G32B32A32_UNORM, 4, CVT_32_UNORM )
FETCH_ATTRIB( R32G32B32_UNORM,    3, CVT_32_UNORM )
FETCH_ATTRIB( R32G32_UNORM,       2, CVT_32_UNORM )
FETCH_ATTRIB( R32_UNORM,          1, CVT_32_UNORM )

FETCH_ATTRIB( R32G32B32A32_SNORM, 4, CVT_32_SNORM )
FETCH_ATTRIB( R32G32B32_SNORM,    3, CVT_32_SNORM )
FETCH_ATTRIB( R32G32_SNORM,       2, CVT_32_SNORM )
FETCH_ATTRIB( R32_SNORM,          1, CVT_32_SNORM )

FETCH_ATTRIB( R16G16B16A16_USCALED, 4, CVT_16_USCALED )
FETCH_ATTRIB( R16G16B16_USCALED,    3, CVT_16_USCALED )
FETCH_ATTRIB( R16G16_USCALED,       2, CVT_16_USCALED )
FETCH_ATTRIB( R16_USCALED,          1, CVT_16_USCALED )

FETCH_ATTRIB( R16G16B16A16_SSCALED, 4, CVT_16_SSCALED )
FETCH_ATTRIB( R16G16B16_SSCALED,    3, CVT_16_SSCALED )
FETCH_ATTRIB( R16G16_SSCALED,       2, CVT_16_SSCALED )
FETCH_ATTRIB( R16_SSCALED,          1, CVT_16_SSCALED )

FETCH_ATTRIB( R16G16B16A16_UNORM, 4, CVT_16_UNORM )
FETCH_ATTRIB( R16G16B16_UNORM,    3, CVT_16_UNORM )
FETCH_ATTRIB( R16G16_UNORM,       2, CVT_16_UNORM )
FETCH_ATTRIB( R16_UNORM,          1, CVT_16_UNORM )

FETCH_ATTRIB( R16G16B16A16_SNORM, 4, CVT_16_SNORM )
FETCH_ATTRIB( R16G16B16_SNORM,    3, CVT_16_SNORM )
FETCH_ATTRIB( R16G16_SNORM,       2, CVT_16_SNORM )
FETCH_ATTRIB( R16_SNORM,          1, CVT_16_SNORM )

FETCH_ATTRIB( R8G8B8A8_USCALED,   4, CVT_8_USCALED )
FETCH_ATTRIB( R8G8B8_USCALED,     3, CVT_8_USCALED )
FETCH_ATTRIB( R8G8_USCALED,       2, CVT_8_USCALED )
FETCH_ATTRIB( R8_USCALED,         1, CVT_8_USCALED )

FETCH_ATTRIB( R8G8B8A8_SSCALED,  4, CVT_8_SSCALED )
FETCH_ATTRIB( R8G8B8_SSCALED,    3, CVT_8_SSCALED )
FETCH_ATTRIB( R8G8_SSCALED,      2, CVT_8_SSCALED )
FETCH_ATTRIB( R8_SSCALED,        1, CVT_8_SSCALED )

FETCH_ATTRIB( R8G8B8A8_UNORM,  4, CVT_8_UNORM )
FETCH_ATTRIB( R8G8B8_UNORM,    3, CVT_8_UNORM )
FETCH_ATTRIB( R8G8_UNORM,      2, CVT_8_UNORM )
FETCH_ATTRIB( R8_UNORM,        1, CVT_8_UNORM )

FETCH_ATTRIB( R8G8B8A8_SNORM,  4, CVT_8_SNORM )
FETCH_ATTRIB( R8G8B8_SNORM,    3, CVT_8_SNORM )
FETCH_ATTRIB( R8G8_SNORM,      2, CVT_8_SNORM )
FETCH_ATTRIB( R8_SNORM,        1, CVT_8_SNORM )

FETCH_ATTRIB( A8R8G8B8_UNORM,       4, CVT_8_UNORM )
//FETCH_ATTRIB( R8G8B8A8_UNORM,       4, CVT_8_UNORM )



static spu_fetch_func get_fetch_func( enum pipe_format format )
{
#if 0
   {
      char tmp[80];
      pf_sprint_name(tmp, format);
      _mesa_printf("%s: %s\n", __FUNCTION__, tmp);
   }
#endif

   switch (format) {
   case PIPE_FORMAT_R64_FLOAT:
      return fetch_R64_FLOAT;
   case PIPE_FORMAT_R64G64_FLOAT:
      return fetch_R64G64_FLOAT;
   case PIPE_FORMAT_R64G64B64_FLOAT:
      return fetch_R64G64B64_FLOAT;
   case PIPE_FORMAT_R64G64B64A64_FLOAT:
      return fetch_R64G64B64A64_FLOAT;

   case PIPE_FORMAT_R32_FLOAT:
      return fetch_R32_FLOAT;
   case PIPE_FORMAT_R32G32_FLOAT:
      return fetch_R32G32_FLOAT;
   case PIPE_FORMAT_R32G32B32_FLOAT:
      return fetch_R32G32B32_FLOAT;
   case PIPE_FORMAT_R32G32B32A32_FLOAT:
      return fetch_R32G32B32A32_FLOAT;

   case PIPE_FORMAT_R32_UNORM:
      return fetch_R32_UNORM;
   case PIPE_FORMAT_R32G32_UNORM:
      return fetch_R32G32_UNORM;
   case PIPE_FORMAT_R32G32B32_UNORM:
      return fetch_R32G32B32_UNORM;
   case PIPE_FORMAT_R32G32B32A32_UNORM:
      return fetch_R32G32B32A32_UNORM;

   case PIPE_FORMAT_R32_USCALED:
      return fetch_R32_USCALED;
   case PIPE_FORMAT_R32G32_USCALED:
      return fetch_R32G32_USCALED;
   case PIPE_FORMAT_R32G32B32_USCALED:
      return fetch_R32G32B32_USCALED;
   case PIPE_FORMAT_R32G32B32A32_USCALED:
      return fetch_R32G32B32A32_USCALED;

   case PIPE_FORMAT_R32_SNORM:
      return fetch_R32_SNORM;
   case PIPE_FORMAT_R32G32_SNORM:
      return fetch_R32G32_SNORM;
   case PIPE_FORMAT_R32G32B32_SNORM:
      return fetch_R32G32B32_SNORM;
   case PIPE_FORMAT_R32G32B32A32_SNORM:
      return fetch_R32G32B32A32_SNORM;

   case PIPE_FORMAT_R32_SSCALED:
      return fetch_R32_SSCALED;
   case PIPE_FORMAT_R32G32_SSCALED:
      return fetch_R32G32_SSCALED;
   case PIPE_FORMAT_R32G32B32_SSCALED:
      return fetch_R32G32B32_SSCALED;
   case PIPE_FORMAT_R32G32B32A32_SSCALED:
      return fetch_R32G32B32A32_SSCALED;

   case PIPE_FORMAT_R16_UNORM:
      return fetch_R16_UNORM;
   case PIPE_FORMAT_R16G16_UNORM:
      return fetch_R16G16_UNORM;
   case PIPE_FORMAT_R16G16B16_UNORM:
      return fetch_R16G16B16_UNORM;
   case PIPE_FORMAT_R16G16B16A16_UNORM:
      return fetch_R16G16B16A16_UNORM;

   case PIPE_FORMAT_R16_USCALED:
      return fetch_R16_USCALED;
   case PIPE_FORMAT_R16G16_USCALED:
      return fetch_R16G16_USCALED;
   case PIPE_FORMAT_R16G16B16_USCALED:
      return fetch_R16G16B16_USCALED;
   case PIPE_FORMAT_R16G16B16A16_USCALED:
      return fetch_R16G16B16A16_USCALED;

   case PIPE_FORMAT_R16_SNORM:
      return fetch_R16_SNORM;
   case PIPE_FORMAT_R16G16_SNORM:
      return fetch_R16G16_SNORM;
   case PIPE_FORMAT_R16G16B16_SNORM:
      return fetch_R16G16B16_SNORM;
   case PIPE_FORMAT_R16G16B16A16_SNORM:
      return fetch_R16G16B16A16_SNORM;

   case PIPE_FORMAT_R16_SSCALED:
      return fetch_R16_SSCALED;
   case PIPE_FORMAT_R16G16_SSCALED:
      return fetch_R16G16_SSCALED;
   case PIPE_FORMAT_R16G16B16_SSCALED:
      return fetch_R16G16B16_SSCALED;
   case PIPE_FORMAT_R16G16B16A16_SSCALED:
      return fetch_R16G16B16A16_SSCALED;

   case PIPE_FORMAT_R8_UNORM:
      return fetch_R8_UNORM;
   case PIPE_FORMAT_R8G8_UNORM:
      return fetch_R8G8_UNORM;
   case PIPE_FORMAT_R8G8B8_UNORM:
      return fetch_R8G8B8_UNORM;
   case PIPE_FORMAT_R8G8B8A8_UNORM:
      return fetch_R8G8B8A8_UNORM;

   case PIPE_FORMAT_R8_USCALED:
      return fetch_R8_USCALED;
   case PIPE_FORMAT_R8G8_USCALED:
      return fetch_R8G8_USCALED;
   case PIPE_FORMAT_R8G8B8_USCALED:
      return fetch_R8G8B8_USCALED;
   case PIPE_FORMAT_R8G8B8A8_USCALED:
      return fetch_R8G8B8A8_USCALED;

   case PIPE_FORMAT_R8_SNORM:
      return fetch_R8_SNORM;
   case PIPE_FORMAT_R8G8_SNORM:
      return fetch_R8G8_SNORM;
   case PIPE_FORMAT_R8G8B8_SNORM:
      return fetch_R8G8B8_SNORM;
   case PIPE_FORMAT_R8G8B8A8_SNORM:
      return fetch_R8G8B8A8_SNORM;

   case PIPE_FORMAT_R8_SSCALED:
      return fetch_R8_SSCALED;
   case PIPE_FORMAT_R8G8_SSCALED:
      return fetch_R8G8_SSCALED;
   case PIPE_FORMAT_R8G8B8_SSCALED:
      return fetch_R8G8B8_SSCALED;
   case PIPE_FORMAT_R8G8B8A8_SSCALED:
      return fetch_R8G8B8A8_SSCALED;

   case PIPE_FORMAT_A8R8G8B8_UNORM:
      return fetch_A8R8G8B8_UNORM;

   case 0:
      return NULL;		/* not sure why this is needed */

   default:
      assert(0);
      return NULL;
   }
}


static void 
transpose_4x4( float *out, const float *in )
{
   /* This can be achieved in 12 sse instructions, plus the final
    * stores I guess.  This is probably a bit more than that - maybe
    * 32 or so?
    */
   out[0] = in[0];  out[1] = in[4];  out[2] = in[8];   out[3] = in[12];
   out[4] = in[1];  out[5] = in[5];  out[6] = in[9];   out[7] = in[13];
   out[8] = in[2];  out[9] = in[6];  out[10] = in[10]; out[11] = in[14];
   out[12] = in[3]; out[13] = in[7]; out[14] = in[11]; out[15] = in[15];
}



static void fetch_xyz_rgb( struct spu_vs_context *draw,
			   struct spu_exec_machine *machine,
			   const unsigned *elts,
			   unsigned count )
{
   assert(count <= 4);

//   _mesa_printf("%s\n", __FUNCTION__);

   /* loop over vertex attributes (vertex shader inputs)
    */

   const unsigned *pitch   = draw->vertex_fetch.pitch;
   const ubyte **src       = draw->vertex_fetch.src_ptr;
   int i;

   for (i = 0; i < 4; i++) {
      {
	 const float *in = (const float *)(src[0] + elts[i] * pitch[0]);
	 float *out = &machine->Inputs[0].xyzw[0].f[i];
	 out[0] = in[0];
	 out[4] = in[1];
	 out[8] = in[2];
 	 out[12] = 1.0f;
      }

      {
	 const float *in = (const float *)(src[1] + elts[i] * pitch[1]);
	 float *out = &machine->Inputs[1].xyzw[0].f[i];
	 out[0] = in[0];
	 out[4] = in[1];
	 out[8] = in[2];
 	 out[12] = 1.0f;
      }
   }
}




static void fetch_xyz_rgb_st( struct spu_vs_context *draw,
			      struct spu_exec_machine *machine,
			      const unsigned *elts,
			      unsigned count )
{
   assert(count <= 4);

   /* loop over vertex attributes (vertex shader inputs)
    */

   const unsigned *pitch   = draw->vertex_fetch.pitch;
   const ubyte **src       = draw->vertex_fetch.src_ptr;
   int i;

   for (i = 0; i < 4; i++) {
      {
	 const float *in = (const float *)(src[0] + elts[i] * pitch[0]);
	 float *out = &machine->Inputs[0].xyzw[0].f[i];
	 out[0] = in[0];
	 out[4] = in[1];
	 out[8] = in[2];
 	 out[12] = 1.0f;
      }

      {
	 const float *in = (const float *)(src[1] + elts[i] * pitch[1]);
	 float *out = &machine->Inputs[1].xyzw[0].f[i];
	 out[0] = in[0];
	 out[4] = in[1];
	 out[8] = in[2];
 	 out[12] = 1.0f;
      }

      {
	 const float *in = (const float *)(src[2] + elts[i] * pitch[2]);
	 float *out = &machine->Inputs[1].xyzw[0].f[i];
	 out[0] = in[0];
	 out[4] = in[1];
	 out[8] = 0.0f;
 	 out[12] = 1.0f;
      }
   }
}




/**
 * Fetch vertex attributes for 'count' vertices.
 */
static void generic_vertex_fetch(struct spu_vs_context *draw,
                                 struct spu_exec_machine *machine,
                                 const unsigned *elts,
                                 unsigned count)
{
   unsigned nr_attrs = draw->vertex_fetch.nr_attrs;
   unsigned attr;

   assert(count <= 4);

   wait_on_mask(1 << TAG_VERTEX_BUFFER);

//   _mesa_printf("%s %d\n", __FUNCTION__, count);

   /* loop over vertex attributes (vertex shader inputs)
    */
   for (attr = 0; attr < nr_attrs; attr++) {

      const unsigned pitch   = draw->vertex_fetch.pitch[attr];
      const ubyte *src = draw->vertex_fetch.src_ptr[attr];
      const spu_fetch_func fetch = draw->vertex_fetch.fetch[attr];
      unsigned i;
      float p[4][4];


      /* Fetch four attributes for four vertices.  
       * 
       * Could fetch directly into AOS format, but this is meant to be
       * a prototype for an sse implementation, which would have
       * difficulties doing that.
       */
      for (i = 0; i < count; i++) {
         uint8_t buffer[32] ALIGN16_ATTRIB;
         const unsigned long addr = src + (elts[i] * pitch);
         const unsigned size = ((addr & 0x0f) == 0) ? 16 : 32;

         mfc_get(buffer, addr & ~0x0f, size, TAG_VERTEX_BUFFER, 0, 0);
         wait_on_mask(1 << TAG_VERTEX_BUFFER);

         memmove(& buffer, buffer + (addr & 0x0f), 16);

         fetch(buffer, p[i]);
      }

      /* Be nice and zero out any missing vertices: 
       */
      for (/* empty */; i < 4; i++) 
          p[i][0] = p[i][1] = p[i][2] = p[i][3] = 0;
      
      /* Transpose/swizzle into sse-friendly format.  Currently
       * assuming that all vertex shader inputs are float[4], but this
       * isn't true -- if the vertex shader only wants tex0.xy, we
       * could optimize for that.
       *
       * To do so fully without codegen would probably require an
       * excessive number of fetch functions, but we could at least
       * minimize the transpose step:
       */
      transpose_4x4( (float *)&machine->Inputs[attr].xyzw[0].f[0], (float *)p );
   }
}


void spu_update_vertex_fetch( struct spu_vs_context *draw )
{
   unsigned i;

   
   for (i = 0; i < draw->vertex_fetch.nr_attrs; i++) {
      draw->vertex_fetch.fetch[i] =
          get_fetch_func(draw->vertex_fetch.format[i]);
   }

   draw->vertex_fetch.fetch_func = generic_vertex_fetch;

   /* Disable the fast path because they don't use mfc_get yet.
    */
#if 0
   switch (draw->vertex_fetch.nr_attrs) {
   case 2:
      if (draw->vertex_fetch.format[0] == PIPE_FORMAT_R32G32B32_FLOAT &&
          draw->vertex_fetch.format[1] == PIPE_FORMAT_R32G32B32_FLOAT)
          draw->vertex_fetch.fetch_func = fetch_xyz_rgb;
      break;
   case 3:
      if (draw->vertex_fetch.format[0] == PIPE_FORMAT_R32G32B32_FLOAT &&
          draw->vertex_fetch.format[1] == PIPE_FORMAT_R32G32B32_FLOAT &&
          draw->vertex_fetch.format[2] == PIPE_FORMAT_R32G32_FLOAT)
          draw->vertex_fetch.fetch_func = fetch_xyz_rgb_st;
      break;
   default:
      break;
   }
#endif
}