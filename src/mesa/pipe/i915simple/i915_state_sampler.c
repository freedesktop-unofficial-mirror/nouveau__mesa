/**************************************************************************
 * 
 * Copyright 2003 Tungsten Graphics, Inc., Cedar Park, Texas.
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

//#include "mtypes.h"
//#include "enums.h"
//#include "texformat.h"
//#include "macros.h"
//#include "dri_bufmgr.h"


#include "pipe/p_context.h"
#include "pipe/p_state.h"
#include "pipe/p_util.h"

#include "i915_state_inlines.h"
#include "i915_context.h"
#include "i915_reg.h"
#include "i915_state.h"
//#include "i915_cache.h"






/* The i915 (and related graphics cores) do not support GL_CLAMP.  The
 * Intel drivers for "other operating systems" implement GL_CLAMP as
 * GL_CLAMP_TO_EDGE, so the same is done here.
 */
static unsigned
translate_wrap_mode(unsigned wrap)
{
   switch (wrap) {
   case PIPE_TEX_WRAP_REPEAT:
      return TEXCOORDMODE_WRAP;
   case PIPE_TEX_WRAP_CLAMP:
      return TEXCOORDMODE_CLAMP_EDGE;   /* not quite correct */
   case PIPE_TEX_WRAP_CLAMP_TO_EDGE:
      return TEXCOORDMODE_CLAMP_EDGE;
   case PIPE_TEX_WRAP_CLAMP_TO_BORDER:
      return TEXCOORDMODE_CLAMP_BORDER;
//   case PIPE_TEX_WRAP_MIRRORED_REPEAT:
//      return TEXCOORDMODE_MIRROR;
   default:
      return TEXCOORDMODE_WRAP;
   }
}

static unsigned translate_img_filter( unsigned filter )
{
   switch (filter) {
   case PIPE_TEX_FILTER_NEAREST:
      return FILTER_NEAREST;
   case PIPE_TEX_FILTER_LINEAR:
      return FILTER_LINEAR;
   default:
      assert(0);
      return FILTER_NEAREST;
   }
}

static unsigned translate_mip_filter( unsigned filter )
{
   switch (filter) {
   case PIPE_TEX_MIPFILTER_NONE:
      return MIPFILTER_NONE;
   case PIPE_TEX_MIPFILTER_NEAREST:
      return MIPFILTER_NEAREST;
   case PIPE_TEX_FILTER_LINEAR:
      return MIPFILTER_LINEAR;
   default:
      assert(0);
      return MIPFILTER_NONE;
   }
}

static unsigned translate_compare_func(unsigned func)
{
   switch (func) {
   case PIPE_FUNC_NEVER:
   case PIPE_FUNC_LESS:
   case PIPE_FUNC_EQUAL:
   case PIPE_FUNC_LEQUAL:
   case PIPE_FUNC_GREATER:
   case PIPE_FUNC_NOTEQUAL:
   case PIPE_FUNC_GEQUAL:
   case PIPE_FUNC_ALWAYS:
      return 0;
   default:
      assert(0);
      return 0;
   }
}


static uint
bitcount(uint k)
{
   uint count = 0;
   while (k) {
      if (k & 1)
         count++;
      k = k >> 1;
   }
   return count;
}


static boolean
is_power_of_two_texture(const struct pipe_mipmap_tree *mt)
{
   if (bitcount(mt->width0) == 1 &&
       bitcount(mt->height0) == 1 &&
       bitcount(mt->depth0) == 1) {
      return 1;
   }
   else
      return 0;
}


/**
 * Compute i915 texture sampling state.
 *
 * Recalculate all state from scratch.  Perhaps not the most
 * efficient, but this has gotten complex enough that we need
 * something which is understandable and reliable.
 * \param state  returns the 3 words of compute state
 */
static void update_sampler(struct i915_context *i915,
                           uint unit,
			   const struct pipe_sampler_state *sampler,
			   const struct pipe_mipmap_tree *mt,
			   unsigned state[3] )
{
   const unsigned ws = sampler->wrap_s;
   const unsigned wt = sampler->wrap_t;
   const unsigned wr = sampler->wrap_r;

   /* Need to do this after updating the maps, which call the
    * intel_finalize_mipmap_tree and hence can update firstLevel:
    */
   unsigned minFilt, magFilt;
   unsigned mipFilt;

   state[0] = state[1] = state[2] = 0;

   mipFilt = translate_mip_filter( sampler->min_mip_filter );

   if (sampler->max_anisotropy > 1.0) {
      minFilt = FILTER_ANISOTROPIC;
      magFilt = FILTER_ANISOTROPIC;
   }
   else {
      minFilt = translate_img_filter( sampler->min_img_filter );
      magFilt = translate_img_filter( sampler->mag_img_filter );
   }

   {
      int b = sampler->lod_bias * 16.0;
      b = CLAMP(b, -256, 255);
      state[0] |= ((b << SS2_LOD_BIAS_SHIFT) & SS2_LOD_BIAS_MASK);
   }

   if (mt->format == PIPE_FORMAT_YCBCR ||
       mt->format == PIPE_FORMAT_YCBCR_REV)
      state[0] |= SS2_COLORSPACE_CONVERSION;


   /* Shadow:
    */
   if (sampler->compare_mode == PIPE_TEX_COMPARE_R_TO_TEXTURE) 
   {
      state[0] |= (SS2_SHADOW_ENABLE |
		   translate_compare_func(sampler->compare_func));

      minFilt = FILTER_4X4_FLAT;
      magFilt = FILTER_4X4_FLAT;
   }

   state[0] |= ((minFilt << SS2_MIN_FILTER_SHIFT) |
		(mipFilt << SS2_MIP_FILTER_SHIFT) |
		(magFilt << SS2_MAG_FILTER_SHIFT));

   
   /* 3D textures don't seem to respect the border color.
    * Fallback if there's ever a danger that they might refer to
    * it.  
    * 
    * Effectively this means fallback on 3D clamp or
    * clamp_to_border.
    *
    * XXX: Check if this is true on i945.  
    * XXX: Check if this bug got fixed in release silicon.
    */
   if (mt->target == PIPE_TEXTURE_3D &&
       (sampler->min_img_filter != PIPE_TEX_FILTER_NEAREST ||
        sampler->mag_img_filter != PIPE_TEX_FILTER_NEAREST) &&
       (ws == PIPE_TEX_WRAP_CLAMP ||
        wt == PIPE_TEX_WRAP_CLAMP ||
        wr == PIPE_TEX_WRAP_CLAMP ||
        ws == PIPE_TEX_WRAP_CLAMP_TO_BORDER ||
        wt == PIPE_TEX_WRAP_CLAMP_TO_BORDER || 
        wr == PIPE_TEX_WRAP_CLAMP_TO_BORDER)) {
#if 0
      if (i915->strict_conformance) {
         assert(0);
         /* 	    sampler->fallback = true; */
         /* TODO */
      }
#endif
   }

   state[1] =
      ((translate_wrap_mode(ws) << SS3_TCX_ADDR_MODE_SHIFT) |
       (translate_wrap_mode(wt) << SS3_TCY_ADDR_MODE_SHIFT) |
       (translate_wrap_mode(wr) << SS3_TCZ_ADDR_MODE_SHIFT) |
       (unit << SS3_TEXTUREMAP_INDEX_SHIFT));

   if (is_power_of_two_texture(mt)) {
      state[1] |= SS3_NORMALIZED_COORDS;
   }

   {
      ubyte r = float_to_ubyte(sampler->border_color[0]);
      ubyte g = float_to_ubyte(sampler->border_color[1]);
      ubyte b = float_to_ubyte(sampler->border_color[2]);
      ubyte a = float_to_ubyte(sampler->border_color[3]);
      state[2] = I915PACKCOLOR8888(r, g, b, a);
   }
}



void i915_update_samplers( struct i915_context *i915 )
{
   uint unit;

   i915->current.sampler_enable_nr = 0;
   i915->current.sampler_enable_flags = 0x0;

   for (unit = 0; unit < I915_TEX_UNITS; unit++) {
      /* determine unit enable/disable by looking for a bound mipmap tree */
      /* could also examine the fragment program? */
      if (i915->texture[unit]) {
	 update_sampler( i915,
                         unit,
                         i915->sampler + unit,       /* sampler state */
                         i915->texture[unit],        /* mipmap tree */
			 i915->current.sampler[unit] /* the result */
                         );

         i915->current.sampler_enable_nr++;
         i915->current.sampler_enable_flags |= (1 << unit);
      }
   }

   i915->hardware_dirty |= I915_HW_SAMPLER;
}




static uint
translate_texture_format(uint pipeFormat)
{
   switch (pipeFormat) {
   case PIPE_FORMAT_U_L8:
      return MAPSURF_8BIT | MT_8BIT_L8;
   case PIPE_FORMAT_U_I8:
      return MAPSURF_8BIT | MT_8BIT_I8;
   case PIPE_FORMAT_U_A8:
      return MAPSURF_8BIT | MT_8BIT_A8;
   case PIPE_FORMAT_U_A8_L8:
      return MAPSURF_16BIT | MT_16BIT_AY88;
   case PIPE_FORMAT_U_R5_G6_B5:
      return MAPSURF_16BIT | MT_16BIT_RGB565;
   case PIPE_FORMAT_U_A1_R5_G5_B5:
      return MAPSURF_16BIT | MT_16BIT_ARGB1555;
   case PIPE_FORMAT_U_A4_R4_G4_B4:
      return MAPSURF_16BIT | MT_16BIT_ARGB4444;
   case PIPE_FORMAT_U_A8_R8_G8_B8:
      return MAPSURF_32BIT | MT_32BIT_ARGB8888;
   case PIPE_FORMAT_YCBCR_REV:
      return (MAPSURF_422 | MT_422_YCRCB_NORMAL);
   case PIPE_FORMAT_YCBCR:
      return (MAPSURF_422 | MT_422_YCRCB_SWAPY);
#if 0
   case PIPE_FORMAT_RGB_FXT1:
   case PIPE_FORMAT_RGBA_FXT1:
      return (MAPSURF_COMPRESSED | MT_COMPRESS_FXT1);
#endif
   case PIPE_FORMAT_U_Z16:
      return (MAPSURF_16BIT | MT_16BIT_L16);
#if 0
   case PIPE_FORMAT_RGBA_DXT1:
   case PIPE_FORMAT_RGB_DXT1:
      return (MAPSURF_COMPRESSED | MT_COMPRESS_DXT1);
   case PIPE_FORMAT_RGBA_DXT3:
      return (MAPSURF_COMPRESSED | MT_COMPRESS_DXT2_3);
   case PIPE_FORMAT_RGBA_DXT5:
      return (MAPSURF_COMPRESSED | MT_COMPRESS_DXT4_5);
#endif
   case PIPE_FORMAT_S8_Z24:
      return (MAPSURF_32BIT | MT_32BIT_xL824);
   default:
      fprintf(stderr, "i915: translate_texture_format() bad image format %x\n",
              pipeFormat);
      assert(0);
      return 0;
   }
}


#define I915_TEXREG_MS3        1
#define I915_TEXREG_MS4        2


static void
i915_update_texture(struct i915_context *i915, uint unit,
                    uint state[6])
{
   const struct pipe_mipmap_tree *mt = i915->texture[unit];
   uint format, pitch;
   const uint width = mt->width0, height = mt->height0, depth = mt->depth0;
   const uint num_levels = mt->last_level - mt->first_level;

   assert(mt);
   assert(width);
   assert(height);
   assert(depth);

#if 0
   if (i915->state.tex_buffer[unit] != NULL) {
       driBOUnReference(i915->state.tex_buffer[unit]);
       i915->state.tex_buffer[unit] = NULL;
   }
#endif


   {
      struct pipe_buffer_handle *p = driBOReference(mt->region->buffer);
   }

#if 0
   i915->state.tex_buffer[unit] = driBOReference(intelObj->mt->region->
                                                 buffer);
   i915->state.tex_offset[unit] =  intel_miptree_image_offset(intelObj->mt,
                                                              0, intelObj->
                                                              firstLevel);
#endif

   format = translate_texture_format(mt->format);
   pitch = mt->pitch * mt->cpp;

   assert(format);
   assert(pitch);

   printf("texture format = 0x%x\n", format);

   /* MS3 state */
   state[0] =
      (((height - 1) << MS3_HEIGHT_SHIFT)
       | ((width - 1) << MS3_WIDTH_SHIFT)
       | format
       | MS3_USE_FENCE_REGS);

   /* MS4 state */
   state[1] =
      ((((pitch / 4) - 1) << MS4_PITCH_SHIFT)
       | MS4_CUBE_FACE_ENA_MASK
       | ((num_levels * 4) << MS4_MAX_LOD_SHIFT)
       | ((depth - 1) << MS4_VOLUME_DEPTH_SHIFT));
}



void
i915_update_textures(struct i915_context *i915)
{
   uint unit;

   for (unit = 0; unit < I915_TEX_UNITS; unit++) {
      /* determine unit enable/disable by looking for a bound mipmap tree */
      /* could also examine the fragment program? */
      if (i915->texture[unit]) {
         i915_update_texture(i915, unit, i915->current.texbuffer[unit]);
      }
   }

   i915->hardware_dirty |= I915_HW_MAP;
}