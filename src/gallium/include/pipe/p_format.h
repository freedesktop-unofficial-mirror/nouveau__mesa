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

#ifndef PIPE_FORMAT_H
#define PIPE_FORMAT_H

#include <stdio.h> // for sprintf

#include "p_compiler.h"
#include "p_debug.h"

/**
 * The PIPE_FORMAT is a 32-bit wide bitfield that encodes all the information
 * needed to uniquely describe a pixel format.
 */

/**
 * Possible format layouts -- occupy first 2 bits. The interpretation of
 * the remaining 30 bits depends on a particual format layout.
 */
#define PIPE_FORMAT_LAYOUT_RGBAZS   0
#define PIPE_FORMAT_LAYOUT_YCBCR    1

static INLINE uint pf_layout(uint f)  /**< PIPE_FORMAT_LAYOUT_ */
{
   return f & 0x3;
}

/**
 * RGBAZS Format Layout.
 */

/**
 * Format component selectors for RGBAZS layout.
 */
#define PIPE_FORMAT_COMP_R    0
#define PIPE_FORMAT_COMP_G    1
#define PIPE_FORMAT_COMP_B    2
#define PIPE_FORMAT_COMP_A    3
#define PIPE_FORMAT_COMP_0    4
#define PIPE_FORMAT_COMP_1    5
#define PIPE_FORMAT_COMP_Z    6
#define PIPE_FORMAT_COMP_S    7

/**
 * Format types for RGBAZS layout.
 */
#define PIPE_FORMAT_TYPE_UNKNOWN 0
#define PIPE_FORMAT_TYPE_FLOAT   1
#define PIPE_FORMAT_TYPE_UNORM   2
#define PIPE_FORMAT_TYPE_SNORM   3
#define PIPE_FORMAT_TYPE_USCALED 4
#define PIPE_FORMAT_TYPE_SSCALED 5

/**
 * Because the destination vector is assumed to be RGBA FLOAT, we
 * need to know how to swizzle and expand components from the source
 * vector.
 * Let's take U_A1_R5_G5_B5 as an example. X swizzle is A, X size
 * is 1 bit and type is UNORM. So we take the most significant bit
 * from source vector, convert 0 to 0.0 and 1 to 1.0 and save it
 * in the last component of the destination RGBA component.
 * Next, Y swizzle is R, Y size is 5 and type is UNORM. We normalize
 * those 5 bits into [0.0; 1.0] range and put it into second
 * component of the destination vector. Rinse and repeat for
 * components Z and W.
 * If any of size fields is zero, it means the source format contains
 * less than four components.
 * If any swizzle is 0 or 1, the corresponding destination component
 * should be filled with 0.0 and 1.0, respectively.
 */
typedef uint pipe_format_rgbazs_t;

static INLINE uint pf_get(pipe_format_rgbazs_t f, uint shift, uint mask)
{
   return (f >> shift) & mask;
}

/* XXX: The bit layout needs to be revised, can't currently encode 10-bit components. */

#define pf_swizzle_x(f)       pf_get(f, 2, 0x7)  /**< PIPE_FORMAT_COMP_ */
#define pf_swizzle_y(f)       pf_get(f, 5, 0x7)  /**< PIPE_FORMAT_COMP_ */
#define pf_swizzle_z(f)       pf_get(f, 8, 0x7)  /**< PIPE_FORMAT_COMP_ */
#define pf_swizzle_w(f)       pf_get(f, 11, 0x7) /**< PIPE_FORMAT_COMP_ */
#define pf_swizzle_xyzw(f,i)  pf_get(f, 2+((i)*3), 0x7)
#define pf_size_x(f)          pf_get(f, 14, 0x7) /**< Size of X */
#define pf_size_y(f)          pf_get(f, 17, 0x7) /**< Size of Y */
#define pf_size_z(f)          pf_get(f, 20, 0x7) /**< Size of Z */
#define pf_size_w(f)          pf_get(f, 23, 0x7) /**< Size of W */
#define pf_size_xyzw(f,i)     pf_get(f, 14+((i)*3), 0x7)
#define pf_exp8(f)            pf_get(f, 26, 0x3) /**< Scale size by 8 ^ exp8 */
#define pf_type(f)            pf_get(f, 28, 0xf) /**< PIPE_FORMAT_TYPE_ */

/**
 * Helper macro to encode the above structure into a 32-bit value.
 */
#define _PIPE_FORMAT_RGBAZS( SWZ, SIZEX, SIZEY, SIZEZ, SIZEW, EXP8, TYPE ) (\
   (PIPE_FORMAT_LAYOUT_RGBAZS << 0) |\
   ((SWZ) << 2) |\
   ((SIZEX) << 14) |\
   ((SIZEY) << 17) |\
   ((SIZEZ) << 20) |\
   ((SIZEW) << 23) |\
   ((EXP8) << 26) |\
   ((TYPE) << 28) )

/**
 * Helper macro to encode the swizzle part of the structure above.
 */
#define _PIPE_FORMAT_SWZ( SWZX, SWZY, SWZZ, SWZW ) (((SWZX) << 0) | ((SWZY) << 3) | ((SWZZ) << 6) | ((SWZW) << 9))

/**
 * Shorthand macro for RGBAZS layout with component sizes in 1-bit units.
 */
#define _PIPE_FORMAT_RGBAZS_1( SWZ, SIZEX, SIZEY, SIZEZ, SIZEW, TYPE )\
   _PIPE_FORMAT_RGBAZS( SWZ, SIZEX, SIZEY, SIZEZ, SIZEW, 0, TYPE )

/**
 * Shorthand macro for RGBAZS layout with component sizes in 8-bit units.
 */
#define _PIPE_FORMAT_RGBAZS_8( SWZ, SIZEX, SIZEY, SIZEZ, SIZEW, TYPE )\
   _PIPE_FORMAT_RGBAZS( SWZ, SIZEX, SIZEY, SIZEZ, SIZEW, 1, TYPE )

/**
 * Shorthand macro for RGBAZS layout with component sizes in 64-bit units.
 */
#define _PIPE_FORMAT_RGBAZS_64( SWZ, SIZEX, SIZEY, SIZEZ, SIZEW, TYPE )\
   _PIPE_FORMAT_RGBAZS( SWZ, SIZEX, SIZEY, SIZEZ, SIZEW, 2, TYPE )

/**
 * Shorthand macro for common format swizzles.
 */
#define _PIPE_FORMAT_R000 _PIPE_FORMAT_SWZ( PIPE_FORMAT_COMP_R, PIPE_FORMAT_COMP_0, PIPE_FORMAT_COMP_0, PIPE_FORMAT_COMP_0 )
#define _PIPE_FORMAT_RG00 _PIPE_FORMAT_SWZ( PIPE_FORMAT_COMP_R, PIPE_FORMAT_COMP_G, PIPE_FORMAT_COMP_0, PIPE_FORMAT_COMP_0 )
#define _PIPE_FORMAT_RGB0 _PIPE_FORMAT_SWZ( PIPE_FORMAT_COMP_R, PIPE_FORMAT_COMP_G, PIPE_FORMAT_COMP_B, PIPE_FORMAT_COMP_0 )
#define _PIPE_FORMAT_RGBA _PIPE_FORMAT_SWZ( PIPE_FORMAT_COMP_R, PIPE_FORMAT_COMP_G, PIPE_FORMAT_COMP_B, PIPE_FORMAT_COMP_A )
#define _PIPE_FORMAT_ARGB _PIPE_FORMAT_SWZ( PIPE_FORMAT_COMP_A, PIPE_FORMAT_COMP_R, PIPE_FORMAT_COMP_G, PIPE_FORMAT_COMP_B )
#define _PIPE_FORMAT_BGRA _PIPE_FORMAT_SWZ( PIPE_FORMAT_COMP_B, PIPE_FORMAT_COMP_G, PIPE_FORMAT_COMP_R, PIPE_FORMAT_COMP_A )
#define _PIPE_FORMAT_0000 _PIPE_FORMAT_SWZ( PIPE_FORMAT_COMP_0, PIPE_FORMAT_COMP_0, PIPE_FORMAT_COMP_0, PIPE_FORMAT_COMP_0 )
#define _PIPE_FORMAT_000R _PIPE_FORMAT_SWZ( PIPE_FORMAT_COMP_0, PIPE_FORMAT_COMP_0, PIPE_FORMAT_COMP_0, PIPE_FORMAT_COMP_R )
#define _PIPE_FORMAT_RRR1 _PIPE_FORMAT_SWZ( PIPE_FORMAT_COMP_R, PIPE_FORMAT_COMP_R, PIPE_FORMAT_COMP_R, PIPE_FORMAT_COMP_1 )
#define _PIPE_FORMAT_RRRR _PIPE_FORMAT_SWZ( PIPE_FORMAT_COMP_R, PIPE_FORMAT_COMP_R, PIPE_FORMAT_COMP_R, PIPE_FORMAT_COMP_R )
#define _PIPE_FORMAT_RRRG _PIPE_FORMAT_SWZ( PIPE_FORMAT_COMP_R, PIPE_FORMAT_COMP_R, PIPE_FORMAT_COMP_R, PIPE_FORMAT_COMP_G )
#define _PIPE_FORMAT_Z000 _PIPE_FORMAT_SWZ( PIPE_FORMAT_COMP_Z, PIPE_FORMAT_COMP_0, PIPE_FORMAT_COMP_0, PIPE_FORMAT_COMP_0 )
#define _PIPE_FORMAT_SZ00 _PIPE_FORMAT_SWZ( PIPE_FORMAT_COMP_S, PIPE_FORMAT_COMP_Z, PIPE_FORMAT_COMP_0, PIPE_FORMAT_COMP_0 )
#define _PIPE_FORMAT_ZS00 _PIPE_FORMAT_SWZ( PIPE_FORMAT_COMP_Z, PIPE_FORMAT_COMP_S, PIPE_FORMAT_COMP_0, PIPE_FORMAT_COMP_0 )
#define _PIPE_FORMAT_S000 _PIPE_FORMAT_SWZ( PIPE_FORMAT_COMP_S, PIPE_FORMAT_COMP_0, PIPE_FORMAT_COMP_0, PIPE_FORMAT_COMP_0 )

/**
 * YCBCR Format Layout.
 */

/**
 * This only contains a flag that indicates whether the format is reversed or
 * not.
 */
typedef uint pipe_format_ycbcr_t;

/**
 * Helper macro to encode the above structure into a 32-bit value.
 */
#define _PIPE_FORMAT_YCBCR( REV ) (\
   (PIPE_FORMAT_LAYOUT_YCBCR << 0) |\
   ((REV) << 2) )

static INLINE uint pf_rev(pipe_format_ycbcr_t f)
{
   return (f >> 2) & 0x1;
}

/**
 * Texture/surface image formats (preliminary)
 */

/* KW: Added lots of surface formats to support vertex element layout
 * definitions, and eventually render-to-vertex-buffer.  Could
 * consider making float/int/uint/scaled/normalized a separate
 * parameter, but on the other hand there are special cases like
 * z24s8, compressed textures, ycbcr, etc that won't fit that model.
 */

enum pipe_format {
   PIPE_FORMAT_NONE                  = _PIPE_FORMAT_RGBAZS_1 ( _PIPE_FORMAT_0000, 0, 0, 0, 0, PIPE_FORMAT_TYPE_UNKNOWN ),
   PIPE_FORMAT_A8R8G8B8_UNORM        = _PIPE_FORMAT_RGBAZS_8 ( _PIPE_FORMAT_ARGB, 1, 1, 1, 1, PIPE_FORMAT_TYPE_UNORM ),
   PIPE_FORMAT_B8G8R8A8_UNORM        = _PIPE_FORMAT_RGBAZS_8 ( _PIPE_FORMAT_BGRA, 1, 1, 1, 1, PIPE_FORMAT_TYPE_UNORM ),
   PIPE_FORMAT_A1R5G5B5_UNORM        = _PIPE_FORMAT_RGBAZS_1 ( _PIPE_FORMAT_ARGB, 1, 5, 5, 5, PIPE_FORMAT_TYPE_UNORM ),
   PIPE_FORMAT_A4R4G4B4_UNORM        = _PIPE_FORMAT_RGBAZS_1 ( _PIPE_FORMAT_ARGB, 4, 4, 4, 4, PIPE_FORMAT_TYPE_UNORM ),
   PIPE_FORMAT_R5G6B5_UNORM          = _PIPE_FORMAT_RGBAZS_1 ( _PIPE_FORMAT_RGB0, 5, 6, 5, 0, PIPE_FORMAT_TYPE_UNORM ),
   PIPE_FORMAT_U_L8                  = _PIPE_FORMAT_RGBAZS_8 ( _PIPE_FORMAT_RRR1, 1, 1, 1, 1, PIPE_FORMAT_TYPE_UNORM ), /**< ubyte luminance */
   PIPE_FORMAT_U_A8                  = _PIPE_FORMAT_RGBAZS_8 ( _PIPE_FORMAT_000R, 1, 1, 1, 1, PIPE_FORMAT_TYPE_UNORM ), /**< ubyte alpha */
   PIPE_FORMAT_U_I8                  = _PIPE_FORMAT_RGBAZS_8 ( _PIPE_FORMAT_RRRR, 1, 1, 1, 1, PIPE_FORMAT_TYPE_UNORM ), /**< ubyte intensity */
   PIPE_FORMAT_U_A8_L8               = _PIPE_FORMAT_RGBAZS_8 ( _PIPE_FORMAT_RRRG, 1, 1, 1, 1, PIPE_FORMAT_TYPE_UNORM ), /**< ubyte alpha, luminance */
   PIPE_FORMAT_YCBCR                 = _PIPE_FORMAT_YCBCR( 0 ),
   PIPE_FORMAT_YCBCR_REV             = _PIPE_FORMAT_YCBCR( 1 ),
   PIPE_FORMAT_Z16_UNORM             = _PIPE_FORMAT_RGBAZS_8 ( _PIPE_FORMAT_Z000, 2, 0, 0, 0, PIPE_FORMAT_TYPE_UNORM ),
   PIPE_FORMAT_Z32_UNORM             = _PIPE_FORMAT_RGBAZS_8 ( _PIPE_FORMAT_Z000, 4, 0, 0, 0, PIPE_FORMAT_TYPE_UNORM ),
   PIPE_FORMAT_Z32_FLOAT             = _PIPE_FORMAT_RGBAZS_8 ( _PIPE_FORMAT_Z000, 4, 0, 0, 0, PIPE_FORMAT_TYPE_FLOAT ),
   PIPE_FORMAT_S8Z24_UNORM           = _PIPE_FORMAT_RGBAZS_8 ( _PIPE_FORMAT_SZ00, 1, 3, 0, 0, PIPE_FORMAT_TYPE_UNORM ),
   PIPE_FORMAT_Z24S8_UNORM           = _PIPE_FORMAT_RGBAZS_8 ( _PIPE_FORMAT_ZS00, 3, 1, 0, 0, PIPE_FORMAT_TYPE_UNORM ),
   PIPE_FORMAT_S8_UNORM              = _PIPE_FORMAT_RGBAZS_8 ( _PIPE_FORMAT_S000, 1, 0, 0, 0, PIPE_FORMAT_TYPE_UNORM ), /**< ubyte stencil */
   PIPE_FORMAT_R64_FLOAT             = _PIPE_FORMAT_RGBAZS_64( _PIPE_FORMAT_R000, 1, 0, 0, 0, PIPE_FORMAT_TYPE_FLOAT ),
   PIPE_FORMAT_R64G64_FLOAT          = _PIPE_FORMAT_RGBAZS_64( _PIPE_FORMAT_RG00, 1, 1, 0, 0, PIPE_FORMAT_TYPE_FLOAT ),
   PIPE_FORMAT_R64G64B64_FLOAT       = _PIPE_FORMAT_RGBAZS_64( _PIPE_FORMAT_RGB0, 1, 1, 1, 0, PIPE_FORMAT_TYPE_FLOAT ),
   PIPE_FORMAT_R64G64B64A64_FLOAT    = _PIPE_FORMAT_RGBAZS_64( _PIPE_FORMAT_RGBA, 1, 1, 1, 1, PIPE_FORMAT_TYPE_FLOAT ),
   PIPE_FORMAT_R32_FLOAT             = _PIPE_FORMAT_RGBAZS_8 ( _PIPE_FORMAT_R000, 4, 0, 0, 0, PIPE_FORMAT_TYPE_FLOAT ),
   PIPE_FORMAT_R32G32_FLOAT          = _PIPE_FORMAT_RGBAZS_8 ( _PIPE_FORMAT_RG00, 4, 4, 0, 0, PIPE_FORMAT_TYPE_FLOAT ),
   PIPE_FORMAT_R32G32B32_FLOAT       = _PIPE_FORMAT_RGBAZS_8 ( _PIPE_FORMAT_RGB0, 4, 4, 4, 0, PIPE_FORMAT_TYPE_FLOAT ),
   PIPE_FORMAT_R32G32B32A32_FLOAT    = _PIPE_FORMAT_RGBAZS_8 ( _PIPE_FORMAT_RGBA, 4, 4, 4, 4, PIPE_FORMAT_TYPE_FLOAT ),
   PIPE_FORMAT_R32_UNORM             = _PIPE_FORMAT_RGBAZS_8 ( _PIPE_FORMAT_R000, 4, 0, 0, 0, PIPE_FORMAT_TYPE_UNORM ),
   PIPE_FORMAT_R32G32_UNORM          = _PIPE_FORMAT_RGBAZS_8 ( _PIPE_FORMAT_RG00, 4, 4, 0, 0, PIPE_FORMAT_TYPE_UNORM ),
   PIPE_FORMAT_R32G32B32_UNORM       = _PIPE_FORMAT_RGBAZS_8 ( _PIPE_FORMAT_RGB0, 4, 4, 4, 0, PIPE_FORMAT_TYPE_UNORM ),
   PIPE_FORMAT_R32G32B32A32_UNORM    = _PIPE_FORMAT_RGBAZS_8 ( _PIPE_FORMAT_RGBA, 4, 4, 4, 4, PIPE_FORMAT_TYPE_UNORM ),
   PIPE_FORMAT_R32_USCALED           = _PIPE_FORMAT_RGBAZS_8 ( _PIPE_FORMAT_R000, 4, 0, 0, 0, PIPE_FORMAT_TYPE_USCALED ),
   PIPE_FORMAT_R32G32_USCALED        = _PIPE_FORMAT_RGBAZS_8 ( _PIPE_FORMAT_RG00, 4, 4, 0, 0, PIPE_FORMAT_TYPE_USCALED ),
   PIPE_FORMAT_R32G32B32_USCALED     = _PIPE_FORMAT_RGBAZS_8 ( _PIPE_FORMAT_RGB0, 4, 4, 4, 0, PIPE_FORMAT_TYPE_USCALED ),
   PIPE_FORMAT_R32G32B32A32_USCALED  = _PIPE_FORMAT_RGBAZS_8 ( _PIPE_FORMAT_RGBA, 4, 4, 4, 4, PIPE_FORMAT_TYPE_USCALED ),
   PIPE_FORMAT_R32_SNORM             = _PIPE_FORMAT_RGBAZS_8 ( _PIPE_FORMAT_R000, 4, 0, 0, 0, PIPE_FORMAT_TYPE_SNORM ),
   PIPE_FORMAT_R32G32_SNORM          = _PIPE_FORMAT_RGBAZS_8 ( _PIPE_FORMAT_RG00, 4, 4, 0, 0, PIPE_FORMAT_TYPE_SNORM ),
   PIPE_FORMAT_R32G32B32_SNORM       = _PIPE_FORMAT_RGBAZS_8 ( _PIPE_FORMAT_RGB0, 4, 4, 4, 0, PIPE_FORMAT_TYPE_SNORM ),
   PIPE_FORMAT_R32G32B32A32_SNORM    = _PIPE_FORMAT_RGBAZS_8 ( _PIPE_FORMAT_RGBA, 4, 4, 4, 4, PIPE_FORMAT_TYPE_SNORM ),
   PIPE_FORMAT_R32_SSCALED           = _PIPE_FORMAT_RGBAZS_8 ( _PIPE_FORMAT_R000, 4, 0, 0, 0, PIPE_FORMAT_TYPE_SSCALED ),
   PIPE_FORMAT_R32G32_SSCALED        = _PIPE_FORMAT_RGBAZS_8 ( _PIPE_FORMAT_RG00, 4, 4, 0, 0, PIPE_FORMAT_TYPE_SSCALED ),
   PIPE_FORMAT_R32G32B32_SSCALED     = _PIPE_FORMAT_RGBAZS_8 ( _PIPE_FORMAT_RGB0, 4, 4, 4, 0, PIPE_FORMAT_TYPE_SSCALED ),
   PIPE_FORMAT_R32G32B32A32_SSCALED  = _PIPE_FORMAT_RGBAZS_8 ( _PIPE_FORMAT_RGBA, 4, 4, 4, 4, PIPE_FORMAT_TYPE_SSCALED ),
   PIPE_FORMAT_R16_UNORM             = _PIPE_FORMAT_RGBAZS_8 ( _PIPE_FORMAT_R000, 2, 0, 0, 0, PIPE_FORMAT_TYPE_UNORM ),
   PIPE_FORMAT_R16G16_UNORM          = _PIPE_FORMAT_RGBAZS_8 ( _PIPE_FORMAT_RG00, 2, 2, 0, 0, PIPE_FORMAT_TYPE_UNORM ),
   PIPE_FORMAT_R16G16B16_UNORM       = _PIPE_FORMAT_RGBAZS_8 ( _PIPE_FORMAT_RGB0, 2, 2, 2, 0, PIPE_FORMAT_TYPE_UNORM ),
   PIPE_FORMAT_R16G16B16A16_UNORM    = _PIPE_FORMAT_RGBAZS_8 ( _PIPE_FORMAT_RGBA, 2, 2, 2, 2, PIPE_FORMAT_TYPE_UNORM ),
   PIPE_FORMAT_R16_USCALED           = _PIPE_FORMAT_RGBAZS_8 ( _PIPE_FORMAT_R000, 2, 0, 0, 0, PIPE_FORMAT_TYPE_USCALED ),
   PIPE_FORMAT_R16G16_USCALED        = _PIPE_FORMAT_RGBAZS_8 ( _PIPE_FORMAT_RG00, 2, 2, 0, 0, PIPE_FORMAT_TYPE_USCALED ),
   PIPE_FORMAT_R16G16B16_USCALED     = _PIPE_FORMAT_RGBAZS_8 ( _PIPE_FORMAT_RGB0, 2, 2, 2, 0, PIPE_FORMAT_TYPE_USCALED ),
   PIPE_FORMAT_R16G16B16A16_USCALED  = _PIPE_FORMAT_RGBAZS_8 ( _PIPE_FORMAT_RGBA, 2, 2, 2, 2, PIPE_FORMAT_TYPE_USCALED ),
   PIPE_FORMAT_R16_SNORM             = _PIPE_FORMAT_RGBAZS_8 ( _PIPE_FORMAT_R000, 2, 0, 0, 0, PIPE_FORMAT_TYPE_SNORM ),
   PIPE_FORMAT_R16G16_SNORM          = _PIPE_FORMAT_RGBAZS_8 ( _PIPE_FORMAT_RG00, 2, 2, 0, 0, PIPE_FORMAT_TYPE_SNORM ),
   PIPE_FORMAT_R16G16B16_SNORM       = _PIPE_FORMAT_RGBAZS_8 ( _PIPE_FORMAT_RGB0, 2, 2, 2, 0, PIPE_FORMAT_TYPE_SNORM ),
   PIPE_FORMAT_R16G16B16A16_SNORM    = _PIPE_FORMAT_RGBAZS_8 ( _PIPE_FORMAT_RGBA, 2, 2, 2, 2, PIPE_FORMAT_TYPE_SNORM ),
   PIPE_FORMAT_R16_SSCALED           = _PIPE_FORMAT_RGBAZS_8 ( _PIPE_FORMAT_R000, 2, 0, 0, 0, PIPE_FORMAT_TYPE_SSCALED ),
   PIPE_FORMAT_R16G16_SSCALED        = _PIPE_FORMAT_RGBAZS_8 ( _PIPE_FORMAT_RG00, 2, 2, 0, 0, PIPE_FORMAT_TYPE_SSCALED ),
   PIPE_FORMAT_R16G16B16_SSCALED     = _PIPE_FORMAT_RGBAZS_8 ( _PIPE_FORMAT_RGB0, 2, 2, 2, 0, PIPE_FORMAT_TYPE_SSCALED ),
   PIPE_FORMAT_R16G16B16A16_SSCALED  = _PIPE_FORMAT_RGBAZS_8 ( _PIPE_FORMAT_RGBA, 2, 2, 2, 2, PIPE_FORMAT_TYPE_SSCALED ),
   PIPE_FORMAT_R8_UNORM              = _PIPE_FORMAT_RGBAZS_8 ( _PIPE_FORMAT_R000, 1, 0, 0, 0, PIPE_FORMAT_TYPE_UNORM ),
   PIPE_FORMAT_R8G8_UNORM            = _PIPE_FORMAT_RGBAZS_8 ( _PIPE_FORMAT_RG00, 1, 1, 0, 0, PIPE_FORMAT_TYPE_UNORM ),
   PIPE_FORMAT_R8G8B8_UNORM          = _PIPE_FORMAT_RGBAZS_8 ( _PIPE_FORMAT_RGB0, 1, 1, 1, 0, PIPE_FORMAT_TYPE_UNORM ),
   PIPE_FORMAT_R8G8B8A8_UNORM        = _PIPE_FORMAT_RGBAZS_8 ( _PIPE_FORMAT_RGBA, 1, 1, 1, 1, PIPE_FORMAT_TYPE_UNORM ),
   PIPE_FORMAT_R8_USCALED            = _PIPE_FORMAT_RGBAZS_8 ( _PIPE_FORMAT_R000, 1, 0, 0, 0, PIPE_FORMAT_TYPE_USCALED ),
   PIPE_FORMAT_R8G8_USCALED          = _PIPE_FORMAT_RGBAZS_8 ( _PIPE_FORMAT_RG00, 1, 1, 0, 0, PIPE_FORMAT_TYPE_USCALED ),
   PIPE_FORMAT_R8G8B8_USCALED        = _PIPE_FORMAT_RGBAZS_8 ( _PIPE_FORMAT_RGB0, 1, 1, 1, 0, PIPE_FORMAT_TYPE_USCALED ),
   PIPE_FORMAT_R8G8B8A8_USCALED      = _PIPE_FORMAT_RGBAZS_8 ( _PIPE_FORMAT_RGBA, 1, 1, 1, 1, PIPE_FORMAT_TYPE_USCALED ),
   PIPE_FORMAT_R8_SNORM              = _PIPE_FORMAT_RGBAZS_8 ( _PIPE_FORMAT_R000, 1, 0, 0, 0, PIPE_FORMAT_TYPE_SNORM ),
   PIPE_FORMAT_R8G8_SNORM            = _PIPE_FORMAT_RGBAZS_8 ( _PIPE_FORMAT_RG00, 1, 1, 0, 0, PIPE_FORMAT_TYPE_SNORM ),
   PIPE_FORMAT_R8G8B8_SNORM          = _PIPE_FORMAT_RGBAZS_8 ( _PIPE_FORMAT_RGB0, 1, 1, 1, 0, PIPE_FORMAT_TYPE_SNORM ),
   PIPE_FORMAT_R8G8B8A8_SNORM        = _PIPE_FORMAT_RGBAZS_8 ( _PIPE_FORMAT_RGBA, 1, 1, 1, 1, PIPE_FORMAT_TYPE_SNORM ),
   PIPE_FORMAT_R8_SSCALED            = _PIPE_FORMAT_RGBAZS_8 ( _PIPE_FORMAT_R000, 1, 0, 0, 0, PIPE_FORMAT_TYPE_SSCALED ),
   PIPE_FORMAT_R8G8_SSCALED          = _PIPE_FORMAT_RGBAZS_8 ( _PIPE_FORMAT_RG00, 1, 1, 0, 0, PIPE_FORMAT_TYPE_SSCALED ),
   PIPE_FORMAT_R8G8B8_SSCALED        = _PIPE_FORMAT_RGBAZS_8 ( _PIPE_FORMAT_RGB0, 1, 1, 1, 0, PIPE_FORMAT_TYPE_SSCALED ),
   PIPE_FORMAT_R8G8B8A8_SSCALED      = _PIPE_FORMAT_RGBAZS_8 ( _PIPE_FORMAT_RGBA, 1, 1, 1, 1, PIPE_FORMAT_TYPE_SSCALED )
};


/**
 * XXX should remove this, but S8_UNORM is a poor name
 */
#define PIPE_FORMAT_U_S8                  PIPE_FORMAT_S8_UNORM


/**
 * Builds pipe format name from format token.
 */
static INLINE char *pf_sprint_name( char *str, uint format )
{
   strcpy( str, "PIPE_FORMAT_" );
   switch (pf_layout( format )) {
   case PIPE_FORMAT_LAYOUT_RGBAZS: {
         pipe_format_rgbazs_t rgbazs = (pipe_format_rgbazs_t) format;
         uint                 i;
         uint                 scale = 1 << (pf_exp8( rgbazs ) * 3);

         for (i = 0; i < 4; i++) {
            uint  size = pf_size_xyzw( rgbazs, i );

            if (size == 0) {
               break;
            }
            switch (pf_swizzle_xyzw( rgbazs, i )) {
            case PIPE_FORMAT_COMP_R:
               strcat( str, "R" );
               break;
            case PIPE_FORMAT_COMP_G:
               strcat( str, "G" );
               break;
            case PIPE_FORMAT_COMP_B:
               strcat( str, "B" );
               break;
            case PIPE_FORMAT_COMP_A:
               strcat( str, "A" );
               break;
            case PIPE_FORMAT_COMP_0:
               strcat( str, "0" );
               break;
            case PIPE_FORMAT_COMP_1:
               strcat( str, "1" );
               break;
            case PIPE_FORMAT_COMP_Z:
               strcat( str, "Z" );
               break;
            case PIPE_FORMAT_COMP_S:
               strcat( str, "S" );
               break;
            }
            sprintf( &str[strlen( str )], "%u", size * scale );
         }
         if (i != 0) {
            strcat( str, "_" );
         }
         switch (pf_type( rgbazs )) {
         case PIPE_FORMAT_TYPE_UNKNOWN:
            strcat( str, "NONE" );
            break;
         case PIPE_FORMAT_TYPE_FLOAT:
            strcat( str, "FLOAT" );
            break;
         case PIPE_FORMAT_TYPE_UNORM:
            strcat( str, "UNORM" );
            break;
         case PIPE_FORMAT_TYPE_SNORM:
            strcat( str, "SNORM" );
            break;
         case PIPE_FORMAT_TYPE_USCALED:
            strcat( str, "USCALED" );
            break;
         case PIPE_FORMAT_TYPE_SSCALED:
            strcat( str, "SSCALED" );
            break;
         }
      }
      break;
   case PIPE_FORMAT_LAYOUT_YCBCR: {
         pipe_format_ycbcr_t  ycbcr = (pipe_format_ycbcr_t) format;

         strcat( str, "YCBCR" );
         if (pf_rev( ycbcr )) {
            strcat( str, "_REV" );
         }
      }
      break;
   }
   return str;
}

static INLINE uint pf_get_component_bits( enum pipe_format format, uint comp )
{
   uint size;

   if (pf_swizzle_x(format) == comp) {
      size = pf_size_x(format);
   }
   else if (pf_swizzle_y(format) == comp) {
      size = pf_size_y(format);
   }
   else if (pf_swizzle_z(format) == comp) {
      size = pf_size_z(format);
   }
   else if (pf_swizzle_w(format) == comp) {
      size = pf_size_w(format);
   }
   else {
      size = 0;
   }
   return size << (pf_exp8(format) * 3);
}

static INLINE uint pf_get_bits( enum pipe_format format )
{
   if (pf_layout(format) == PIPE_FORMAT_LAYOUT_RGBAZS) {
      return
         pf_get_component_bits( format, PIPE_FORMAT_COMP_R ) +
         pf_get_component_bits( format, PIPE_FORMAT_COMP_G ) +
         pf_get_component_bits( format, PIPE_FORMAT_COMP_B ) +
         pf_get_component_bits( format, PIPE_FORMAT_COMP_A ) +
         pf_get_component_bits( format, PIPE_FORMAT_COMP_Z ) +
         pf_get_component_bits( format, PIPE_FORMAT_COMP_S );
   }
   else {
      assert( pf_layout(format) == PIPE_FORMAT_LAYOUT_YCBCR );

      /* TODO */
      assert( 0 );
      return 0;
   }
}

static INLINE uint pf_get_size( enum pipe_format format ) {
   assert(pf_get_bits(format) % 8 == 0);
   return pf_get_bits(format) / 8;
}

#endif