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

#include "pipe/p_context.h"
#include "pipe/p_state.h"
#include "pipe/p_util.h"

#include "i915_state_inlines.h"
#include "i915_context.h"
#include "i915_reg.h"
#include "i915_state.h"


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
			   const struct i915_sampler_state *sampler,
			   const struct i915_texture *tex,
			   unsigned state[3] )
{
   const struct pipe_texture *pt = &tex->base;

   /* Need to do this after updating the maps, which call the
    * intel_finalize_mipmap_tree and hence can update firstLevel:
    */
   state[0] = sampler->state[0];
   state[1] = sampler->state[1];
   state[2] = sampler->state[2];

   if (pt->format == PIPE_FORMAT_YCBCR ||
       pt->format == PIPE_FORMAT_YCBCR_REV)
      state[0] |= SS2_COLORSPACE_CONVERSION;

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
#if 0
   {
      const unsigned ws = sampler->templ->wrap_s;
      const unsigned wt = sampler->templ->wrap_t;
      const unsigned wr = sampler->templ->wrap_r;
      if (pt->target == PIPE_TEXTURE_3D &&
          (sampler->templ->min_img_filter != PIPE_TEX_FILTER_NEAREST ||
           sampler->templ->mag_img_filter != PIPE_TEX_FILTER_NEAREST) &&
          (ws == PIPE_TEX_WRAP_CLAMP ||
           wt == PIPE_TEX_WRAP_CLAMP ||
           wr == PIPE_TEX_WRAP_CLAMP ||
           ws == PIPE_TEX_WRAP_CLAMP_TO_BORDER ||
           wt == PIPE_TEX_WRAP_CLAMP_TO_BORDER || 
           wr == PIPE_TEX_WRAP_CLAMP_TO_BORDER)) {
         if (i915->strict_conformance) {
            assert(0);
            /* 	    sampler->fallback = true; */
            /* TODO */
         }
      }
   }
#endif

   state[1] |= (unit << SS3_TEXTUREMAP_INDEX_SHIFT);
}


void i915_update_samplers( struct i915_context *i915 )
{
   uint unit;

   i915->current.sampler_enable_nr = 0;
   i915->current.sampler_enable_flags = 0x0;

   for (unit = 0; unit < I915_TEX_UNITS; unit++) {
      /* determine unit enable/disable by looking for a bound texture */
      /* could also examine the fragment program? */
      if (i915->texture[unit]) {
	 update_sampler( i915,
                         unit,
                         i915->sampler[unit],       /* sampler state */
                         i915->texture[unit],        /* texture */
			 i915->current.sampler[unit] /* the result */
                         );

         i915->current.sampler_enable_nr++;
         i915->current.sampler_enable_flags |= (1 << unit);
      }
   }

   i915->hardware_dirty |= I915_HW_SAMPLER;
}


static uint
translate_texture_format(enum pipe_format pipeFormat)
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
   case PIPE_FORMAT_R5G6B5_UNORM:
      return MAPSURF_16BIT | MT_16BIT_RGB565;
   case PIPE_FORMAT_A1R5G5B5_UNORM:
      return MAPSURF_16BIT | MT_16BIT_ARGB1555;
   case PIPE_FORMAT_A4R4G4B4_UNORM:
      return MAPSURF_16BIT | MT_16BIT_ARGB4444;
   case PIPE_FORMAT_A8R8G8B8_UNORM:
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
   case PIPE_FORMAT_Z16_UNORM:
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
   case PIPE_FORMAT_S8Z24_UNORM:
      return (MAPSURF_32BIT | MT_32BIT_xL824);
   default:
      debug_printf("i915: translate_texture_format() bad image format %x\n",
              pipeFormat);
      assert(0);
      return 0;
   }
}


static void
i915_update_texture(struct i915_context *i915, uint unit,
                    uint state[6])
{
   const struct i915_texture *tex = i915->texture[unit];
   const struct pipe_texture *pt = &tex->base;
   uint format, pitch;
   const uint width = pt->width[0], height = pt->height[0], depth = pt->depth[0];
   const uint num_levels = pt->last_level;

   assert(tex);
   assert(width);
   assert(height);
   assert(depth);

   format = translate_texture_format(pt->format);
   pitch = tex->pitch * pt->cpp;

   assert(format);
   assert(pitch);

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
      /* determine unit enable/disable by looking for a bound texture */
      /* could also examine the fragment program? */
      if (i915->texture[unit]) {
         i915_update_texture(i915, unit, i915->current.texbuffer[unit]);
      }
   }

   i915->hardware_dirty |= I915_HW_MAP;
}