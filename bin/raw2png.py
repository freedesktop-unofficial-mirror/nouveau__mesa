#!/usr/bin/env python
##########################################################################
# 
# Copyright 2008 Tungsten Graphics, Inc., Cedar Park, Texas.
# All Rights Reserved.
# 
# Permission is hereby granted, free of charge, to any person obtaining a
# copy of this software and associated documentation files (the
# "Software"), to deal in the Software without restriction, including
# without limitation the rights to use, copy, modify, merge, publish,
# distribute, sub license, and/or sell copies of the Software, and to
# permit persons to whom the Software is furnished to do so, subject to
# the following conditions:
# 
# The above copyright notice and this permission notice (including the
# next paragraph) shall be included in all copies or substantial portions
# of the Software.
# 
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
# OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
# MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.
# IN NO EVENT SHALL TUNGSTEN GRAPHICS AND/OR ITS SUPPLIERS BE LIABLE FOR
# ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
# TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
# SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
# 
##########################################################################


import os.path
import sys
import struct
import Image # http://www.pythonware.com/products/pil/

PIPE_FORMAT_LAYOUT_RGBAZS   = 0
PIPE_FORMAT_LAYOUT_YCBCR    = 1
PIPE_FORMAT_LAYOUT_DXT      = 2

PIPE_FORMAT_COMP_R    = 0
PIPE_FORMAT_COMP_G    = 1
PIPE_FORMAT_COMP_B    = 2
PIPE_FORMAT_COMP_A    = 3
PIPE_FORMAT_COMP_0    = 4
PIPE_FORMAT_COMP_1    = 5
PIPE_FORMAT_COMP_Z    = 6
PIPE_FORMAT_COMP_S    = 7

PIPE_FORMAT_TYPE_UNKNOWN = 0
PIPE_FORMAT_TYPE_FLOAT   = 1
PIPE_FORMAT_TYPE_UNORM   = 2
PIPE_FORMAT_TYPE_SNORM   = 3
PIPE_FORMAT_TYPE_USCALED = 4
PIPE_FORMAT_TYPE_SSCALED = 5
PIPE_FORMAT_TYPE_SRGB    = 6

def _PIPE_FORMAT_RGBAZS( SWZ, SIZEX, SIZEY, SIZEZ, SIZEW, EXP8, TYPE ):
   return ((PIPE_FORMAT_LAYOUT_RGBAZS << 0) |\
   ((SWZ) << 2) |\
   ((SIZEX) << 14) |\
   ((SIZEY) << 17) |\
   ((SIZEZ) << 20) |\
   ((SIZEW) << 23) |\
   ((EXP8) << 26) |\
   ((TYPE) << 28) )

def _PIPE_FORMAT_SWZ( SWZX, SWZY, SWZZ, SWZW ):
	return (((SWZX) << 0) | ((SWZY) << 3) | ((SWZZ) << 6) | ((SWZW) << 9))

def _PIPE_FORMAT_RGBAZS_1( SWZ, SIZEX, SIZEY, SIZEZ, SIZEW, TYPE ):
	return _PIPE_FORMAT_RGBAZS( SWZ, SIZEX, SIZEY, SIZEZ, SIZEW, 0, TYPE )

def _PIPE_FORMAT_RGBAZS_8( SWZ, SIZEX, SIZEY, SIZEZ, SIZEW, TYPE ):
	return _PIPE_FORMAT_RGBAZS( SWZ, SIZEX, SIZEY, SIZEZ, SIZEW, 1, TYPE )

def _PIPE_FORMAT_RGBAZS_64( SWZ, SIZEX, SIZEY, SIZEZ, SIZEW, TYPE ):
	return _PIPE_FORMAT_RGBAZS( SWZ, SIZEX, SIZEY, SIZEZ, SIZEW, 2, TYPE )

_PIPE_FORMAT_R001 = _PIPE_FORMAT_SWZ( PIPE_FORMAT_COMP_R, PIPE_FORMAT_COMP_0, PIPE_FORMAT_COMP_0, PIPE_FORMAT_COMP_1 )
_PIPE_FORMAT_RG01 = _PIPE_FORMAT_SWZ( PIPE_FORMAT_COMP_R, PIPE_FORMAT_COMP_G, PIPE_FORMAT_COMP_0, PIPE_FORMAT_COMP_1 )
_PIPE_FORMAT_RGB1 = _PIPE_FORMAT_SWZ( PIPE_FORMAT_COMP_R, PIPE_FORMAT_COMP_G, PIPE_FORMAT_COMP_B, PIPE_FORMAT_COMP_1 )
_PIPE_FORMAT_RGBA = _PIPE_FORMAT_SWZ( PIPE_FORMAT_COMP_R, PIPE_FORMAT_COMP_G, PIPE_FORMAT_COMP_B, PIPE_FORMAT_COMP_A )
_PIPE_FORMAT_ARGB = _PIPE_FORMAT_SWZ( PIPE_FORMAT_COMP_A, PIPE_FORMAT_COMP_R, PIPE_FORMAT_COMP_G, PIPE_FORMAT_COMP_B )
_PIPE_FORMAT_BGRA = _PIPE_FORMAT_SWZ( PIPE_FORMAT_COMP_B, PIPE_FORMAT_COMP_G, PIPE_FORMAT_COMP_R, PIPE_FORMAT_COMP_A )
_PIPE_FORMAT_1RGB = _PIPE_FORMAT_SWZ( PIPE_FORMAT_COMP_1, PIPE_FORMAT_COMP_R, PIPE_FORMAT_COMP_G, PIPE_FORMAT_COMP_B )
_PIPE_FORMAT_BGR1 = _PIPE_FORMAT_SWZ( PIPE_FORMAT_COMP_B, PIPE_FORMAT_COMP_G, PIPE_FORMAT_COMP_R, PIPE_FORMAT_COMP_1 )
_PIPE_FORMAT_0000 = _PIPE_FORMAT_SWZ( PIPE_FORMAT_COMP_0, PIPE_FORMAT_COMP_0, PIPE_FORMAT_COMP_0, PIPE_FORMAT_COMP_0 )
_PIPE_FORMAT_000R = _PIPE_FORMAT_SWZ( PIPE_FORMAT_COMP_0, PIPE_FORMAT_COMP_0, PIPE_FORMAT_COMP_0, PIPE_FORMAT_COMP_R )
_PIPE_FORMAT_RRR1 = _PIPE_FORMAT_SWZ( PIPE_FORMAT_COMP_R, PIPE_FORMAT_COMP_R, PIPE_FORMAT_COMP_R, PIPE_FORMAT_COMP_1 )
_PIPE_FORMAT_RRRR = _PIPE_FORMAT_SWZ( PIPE_FORMAT_COMP_R, PIPE_FORMAT_COMP_R, PIPE_FORMAT_COMP_R, PIPE_FORMAT_COMP_R )
_PIPE_FORMAT_RRRG = _PIPE_FORMAT_SWZ( PIPE_FORMAT_COMP_R, PIPE_FORMAT_COMP_R, PIPE_FORMAT_COMP_R, PIPE_FORMAT_COMP_G )
_PIPE_FORMAT_Z000 = _PIPE_FORMAT_SWZ( PIPE_FORMAT_COMP_Z, PIPE_FORMAT_COMP_0, PIPE_FORMAT_COMP_0, PIPE_FORMAT_COMP_0 )
_PIPE_FORMAT_0Z00 = _PIPE_FORMAT_SWZ( PIPE_FORMAT_COMP_0, PIPE_FORMAT_COMP_Z, PIPE_FORMAT_COMP_0, PIPE_FORMAT_COMP_0 )
_PIPE_FORMAT_SZ00 = _PIPE_FORMAT_SWZ( PIPE_FORMAT_COMP_S, PIPE_FORMAT_COMP_Z, PIPE_FORMAT_COMP_0, PIPE_FORMAT_COMP_0 )
_PIPE_FORMAT_ZS00 = _PIPE_FORMAT_SWZ( PIPE_FORMAT_COMP_Z, PIPE_FORMAT_COMP_S, PIPE_FORMAT_COMP_0, PIPE_FORMAT_COMP_0 )
_PIPE_FORMAT_S000 = _PIPE_FORMAT_SWZ( PIPE_FORMAT_COMP_S, PIPE_FORMAT_COMP_0, PIPE_FORMAT_COMP_0, PIPE_FORMAT_COMP_0 )

def _PIPE_FORMAT_YCBCR( REV ):
   return ((PIPE_FORMAT_LAYOUT_YCBCR << 0) |\
   ((REV) << 2) )

def _PIPE_FORMAT_DXT( LEVEL, RSIZE, GSIZE, BSIZE, ASIZE ):
   return ((PIPE_FORMAT_LAYOUT_DXT << 0) | \
    ((LEVEL) << 2) | \
    ((RSIZE) << 5) | \
    ((GSIZE) << 8) | \
    ((BSIZE) << 11) | \
    ((ASIZE) << 14) )

PIPE_FORMAT_NONE                  = _PIPE_FORMAT_RGBAZS_1 ( _PIPE_FORMAT_0000, 0, 0, 0, 0, PIPE_FORMAT_TYPE_UNKNOWN )
PIPE_FORMAT_A8R8G8B8_UNORM        = _PIPE_FORMAT_RGBAZS_8 ( _PIPE_FORMAT_ARGB, 1, 1, 1, 1, PIPE_FORMAT_TYPE_UNORM )
PIPE_FORMAT_X8R8G8B8_UNORM        = _PIPE_FORMAT_RGBAZS_8 ( _PIPE_FORMAT_1RGB, 1, 1, 1, 1, PIPE_FORMAT_TYPE_UNORM )
PIPE_FORMAT_B8G8R8A8_UNORM        = _PIPE_FORMAT_RGBAZS_8 ( _PIPE_FORMAT_BGRA, 1, 1, 1, 1, PIPE_FORMAT_TYPE_UNORM )
PIPE_FORMAT_B8G8R8X8_UNORM        = _PIPE_FORMAT_RGBAZS_8 ( _PIPE_FORMAT_BGR1, 1, 1, 1, 1, PIPE_FORMAT_TYPE_UNORM )
PIPE_FORMAT_A1R5G5B5_UNORM        = _PIPE_FORMAT_RGBAZS_1 ( _PIPE_FORMAT_ARGB, 1, 5, 5, 5, PIPE_FORMAT_TYPE_UNORM )
PIPE_FORMAT_A4R4G4B4_UNORM        = _PIPE_FORMAT_RGBAZS_1 ( _PIPE_FORMAT_ARGB, 4, 4, 4, 4, PIPE_FORMAT_TYPE_UNORM )
PIPE_FORMAT_R5G6B5_UNORM          = _PIPE_FORMAT_RGBAZS_1 ( _PIPE_FORMAT_RGB1, 5, 6, 5, 0, PIPE_FORMAT_TYPE_UNORM )
PIPE_FORMAT_L8_UNORM              = _PIPE_FORMAT_RGBAZS_8 ( _PIPE_FORMAT_RRR1, 1, 1, 1, 0, PIPE_FORMAT_TYPE_UNORM )
PIPE_FORMAT_A8_UNORM              = _PIPE_FORMAT_RGBAZS_8 ( _PIPE_FORMAT_000R, 0, 0, 0, 1, PIPE_FORMAT_TYPE_UNORM )
PIPE_FORMAT_I8_UNORM              = _PIPE_FORMAT_RGBAZS_8 ( _PIPE_FORMAT_RRRR, 1, 1, 1, 1, PIPE_FORMAT_TYPE_UNORM )
PIPE_FORMAT_A8L8_UNORM            = _PIPE_FORMAT_RGBAZS_8 ( _PIPE_FORMAT_RRRG, 1, 1, 1, 1, PIPE_FORMAT_TYPE_UNORM )
PIPE_FORMAT_YCBCR                 = _PIPE_FORMAT_YCBCR( 0 )
PIPE_FORMAT_YCBCR_REV             = _PIPE_FORMAT_YCBCR( 1 )
PIPE_FORMAT_Z16_UNORM             = _PIPE_FORMAT_RGBAZS_8 ( _PIPE_FORMAT_Z000, 2, 0, 0, 0, PIPE_FORMAT_TYPE_UNORM )
PIPE_FORMAT_Z32_UNORM             = _PIPE_FORMAT_RGBAZS_8 ( _PIPE_FORMAT_Z000, 4, 0, 0, 0, PIPE_FORMAT_TYPE_UNORM )
PIPE_FORMAT_Z32_FLOAT             = _PIPE_FORMAT_RGBAZS_8 ( _PIPE_FORMAT_Z000, 4, 0, 0, 0, PIPE_FORMAT_TYPE_FLOAT )
PIPE_FORMAT_S8Z24_UNORM           = _PIPE_FORMAT_RGBAZS_8 ( _PIPE_FORMAT_SZ00, 1, 3, 0, 0, PIPE_FORMAT_TYPE_UNORM )
PIPE_FORMAT_Z24S8_UNORM           = _PIPE_FORMAT_RGBAZS_8 ( _PIPE_FORMAT_ZS00, 3, 1, 0, 0, PIPE_FORMAT_TYPE_UNORM )
PIPE_FORMAT_X8Z24_UNORM           = _PIPE_FORMAT_RGBAZS_8 ( _PIPE_FORMAT_0Z00, 1, 3, 0, 0, PIPE_FORMAT_TYPE_UNORM )
PIPE_FORMAT_Z24X8_UNORM           = _PIPE_FORMAT_RGBAZS_8 ( _PIPE_FORMAT_Z000, 3, 1, 0, 0, PIPE_FORMAT_TYPE_UNORM )
PIPE_FORMAT_S8_UNORM              = _PIPE_FORMAT_RGBAZS_8 ( _PIPE_FORMAT_S000, 1, 0, 0, 0, PIPE_FORMAT_TYPE_UNORM )
PIPE_FORMAT_R64_FLOAT             = _PIPE_FORMAT_RGBAZS_64( _PIPE_FORMAT_R001, 1, 0, 0, 0, PIPE_FORMAT_TYPE_FLOAT )
PIPE_FORMAT_R64G64_FLOAT          = _PIPE_FORMAT_RGBAZS_64( _PIPE_FORMAT_RG01, 1, 1, 0, 0, PIPE_FORMAT_TYPE_FLOAT )
PIPE_FORMAT_R64G64B64_FLOAT       = _PIPE_FORMAT_RGBAZS_64( _PIPE_FORMAT_RGB1, 1, 1, 1, 0, PIPE_FORMAT_TYPE_FLOAT )
PIPE_FORMAT_R64G64B64A64_FLOAT    = _PIPE_FORMAT_RGBAZS_64( _PIPE_FORMAT_RGBA, 1, 1, 1, 1, PIPE_FORMAT_TYPE_FLOAT )
PIPE_FORMAT_R32_FLOAT             = _PIPE_FORMAT_RGBAZS_8 ( _PIPE_FORMAT_R001, 4, 0, 0, 0, PIPE_FORMAT_TYPE_FLOAT )
PIPE_FORMAT_R32G32_FLOAT          = _PIPE_FORMAT_RGBAZS_8 ( _PIPE_FORMAT_RG01, 4, 4, 0, 0, PIPE_FORMAT_TYPE_FLOAT )
PIPE_FORMAT_R32G32B32_FLOAT       = _PIPE_FORMAT_RGBAZS_8 ( _PIPE_FORMAT_RGB1, 4, 4, 4, 0, PIPE_FORMAT_TYPE_FLOAT )
PIPE_FORMAT_R32G32B32A32_FLOAT    = _PIPE_FORMAT_RGBAZS_8 ( _PIPE_FORMAT_RGBA, 4, 4, 4, 4, PIPE_FORMAT_TYPE_FLOAT )
PIPE_FORMAT_R32_UNORM             = _PIPE_FORMAT_RGBAZS_8 ( _PIPE_FORMAT_R001, 4, 0, 0, 0, PIPE_FORMAT_TYPE_UNORM )
PIPE_FORMAT_R32G32_UNORM          = _PIPE_FORMAT_RGBAZS_8 ( _PIPE_FORMAT_RG01, 4, 4, 0, 0, PIPE_FORMAT_TYPE_UNORM )
PIPE_FORMAT_R32G32B32_UNORM       = _PIPE_FORMAT_RGBAZS_8 ( _PIPE_FORMAT_RGB1, 4, 4, 4, 0, PIPE_FORMAT_TYPE_UNORM )
PIPE_FORMAT_R32G32B32A32_UNORM    = _PIPE_FORMAT_RGBAZS_8 ( _PIPE_FORMAT_RGBA, 4, 4, 4, 4, PIPE_FORMAT_TYPE_UNORM )
PIPE_FORMAT_R32_USCALED           = _PIPE_FORMAT_RGBAZS_8 ( _PIPE_FORMAT_R001, 4, 0, 0, 0, PIPE_FORMAT_TYPE_USCALED )
PIPE_FORMAT_R32G32_USCALED        = _PIPE_FORMAT_RGBAZS_8 ( _PIPE_FORMAT_RG01, 4, 4, 0, 0, PIPE_FORMAT_TYPE_USCALED )
PIPE_FORMAT_R32G32B32_USCALED     = _PIPE_FORMAT_RGBAZS_8 ( _PIPE_FORMAT_RGB1, 4, 4, 4, 0, PIPE_FORMAT_TYPE_USCALED )
PIPE_FORMAT_R32G32B32A32_USCALED  = _PIPE_FORMAT_RGBAZS_8 ( _PIPE_FORMAT_RGBA, 4, 4, 4, 4, PIPE_FORMAT_TYPE_USCALED )
PIPE_FORMAT_R32_SNORM             = _PIPE_FORMAT_RGBAZS_8 ( _PIPE_FORMAT_R001, 4, 0, 0, 0, PIPE_FORMAT_TYPE_SNORM )
PIPE_FORMAT_R32G32_SNORM          = _PIPE_FORMAT_RGBAZS_8 ( _PIPE_FORMAT_RG01, 4, 4, 0, 0, PIPE_FORMAT_TYPE_SNORM )
PIPE_FORMAT_R32G32B32_SNORM       = _PIPE_FORMAT_RGBAZS_8 ( _PIPE_FORMAT_RGB1, 4, 4, 4, 0, PIPE_FORMAT_TYPE_SNORM )
PIPE_FORMAT_R32G32B32A32_SNORM    = _PIPE_FORMAT_RGBAZS_8 ( _PIPE_FORMAT_RGBA, 4, 4, 4, 4, PIPE_FORMAT_TYPE_SNORM )
PIPE_FORMAT_R32_SSCALED           = _PIPE_FORMAT_RGBAZS_8 ( _PIPE_FORMAT_R001, 4, 0, 0, 0, PIPE_FORMAT_TYPE_SSCALED )
PIPE_FORMAT_R32G32_SSCALED        = _PIPE_FORMAT_RGBAZS_8 ( _PIPE_FORMAT_RG01, 4, 4, 0, 0, PIPE_FORMAT_TYPE_SSCALED )
PIPE_FORMAT_R32G32B32_SSCALED     = _PIPE_FORMAT_RGBAZS_8 ( _PIPE_FORMAT_RGB1, 4, 4, 4, 0, PIPE_FORMAT_TYPE_SSCALED )
PIPE_FORMAT_R32G32B32A32_SSCALED  = _PIPE_FORMAT_RGBAZS_8 ( _PIPE_FORMAT_RGBA, 4, 4, 4, 4, PIPE_FORMAT_TYPE_SSCALED )
PIPE_FORMAT_R16_UNORM             = _PIPE_FORMAT_RGBAZS_8 ( _PIPE_FORMAT_R001, 2, 0, 0, 0, PIPE_FORMAT_TYPE_UNORM )
PIPE_FORMAT_R16G16_UNORM          = _PIPE_FORMAT_RGBAZS_8 ( _PIPE_FORMAT_RG01, 2, 2, 0, 0, PIPE_FORMAT_TYPE_UNORM )
PIPE_FORMAT_R16G16B16_UNORM       = _PIPE_FORMAT_RGBAZS_8 ( _PIPE_FORMAT_RGB1, 2, 2, 2, 0, PIPE_FORMAT_TYPE_UNORM )
PIPE_FORMAT_R16G16B16A16_UNORM    = _PIPE_FORMAT_RGBAZS_8 ( _PIPE_FORMAT_RGBA, 2, 2, 2, 2, PIPE_FORMAT_TYPE_UNORM )
PIPE_FORMAT_R16_USCALED           = _PIPE_FORMAT_RGBAZS_8 ( _PIPE_FORMAT_R001, 2, 0, 0, 0, PIPE_FORMAT_TYPE_USCALED )
PIPE_FORMAT_R16G16_USCALED        = _PIPE_FORMAT_RGBAZS_8 ( _PIPE_FORMAT_RG01, 2, 2, 0, 0, PIPE_FORMAT_TYPE_USCALED )
PIPE_FORMAT_R16G16B16_USCALED     = _PIPE_FORMAT_RGBAZS_8 ( _PIPE_FORMAT_RGB1, 2, 2, 2, 0, PIPE_FORMAT_TYPE_USCALED )
PIPE_FORMAT_R16G16B16A16_USCALED  = _PIPE_FORMAT_RGBAZS_8 ( _PIPE_FORMAT_RGBA, 2, 2, 2, 2, PIPE_FORMAT_TYPE_USCALED )
PIPE_FORMAT_R16_SNORM             = _PIPE_FORMAT_RGBAZS_8 ( _PIPE_FORMAT_R001, 2, 0, 0, 0, PIPE_FORMAT_TYPE_SNORM )
PIPE_FORMAT_R16G16_SNORM          = _PIPE_FORMAT_RGBAZS_8 ( _PIPE_FORMAT_RG01, 2, 2, 0, 0, PIPE_FORMAT_TYPE_SNORM )
PIPE_FORMAT_R16G16B16_SNORM       = _PIPE_FORMAT_RGBAZS_8 ( _PIPE_FORMAT_RGB1, 2, 2, 2, 0, PIPE_FORMAT_TYPE_SNORM )
PIPE_FORMAT_R16G16B16A16_SNORM    = _PIPE_FORMAT_RGBAZS_8 ( _PIPE_FORMAT_RGBA, 2, 2, 2, 2, PIPE_FORMAT_TYPE_SNORM )
PIPE_FORMAT_R16_SSCALED           = _PIPE_FORMAT_RGBAZS_8 ( _PIPE_FORMAT_R001, 2, 0, 0, 0, PIPE_FORMAT_TYPE_SSCALED )
PIPE_FORMAT_R16G16_SSCALED        = _PIPE_FORMAT_RGBAZS_8 ( _PIPE_FORMAT_RG01, 2, 2, 0, 0, PIPE_FORMAT_TYPE_SSCALED )
PIPE_FORMAT_R16G16B16_SSCALED     = _PIPE_FORMAT_RGBAZS_8 ( _PIPE_FORMAT_RGB1, 2, 2, 2, 0, PIPE_FORMAT_TYPE_SSCALED )
PIPE_FORMAT_R16G16B16A16_SSCALED  = _PIPE_FORMAT_RGBAZS_8 ( _PIPE_FORMAT_RGBA, 2, 2, 2, 2, PIPE_FORMAT_TYPE_SSCALED )
PIPE_FORMAT_R8_UNORM              = _PIPE_FORMAT_RGBAZS_8 ( _PIPE_FORMAT_R001, 1, 0, 0, 0, PIPE_FORMAT_TYPE_UNORM )
PIPE_FORMAT_R8G8_UNORM            = _PIPE_FORMAT_RGBAZS_8 ( _PIPE_FORMAT_RG01, 1, 1, 0, 0, PIPE_FORMAT_TYPE_UNORM )
PIPE_FORMAT_R8G8B8_UNORM          = _PIPE_FORMAT_RGBAZS_8 ( _PIPE_FORMAT_RGB1, 1, 1, 1, 0, PIPE_FORMAT_TYPE_UNORM )
PIPE_FORMAT_R8G8B8A8_UNORM        = _PIPE_FORMAT_RGBAZS_8 ( _PIPE_FORMAT_RGBA, 1, 1, 1, 1, PIPE_FORMAT_TYPE_UNORM )
PIPE_FORMAT_R8G8B8X8_UNORM        = _PIPE_FORMAT_RGBAZS_8 ( _PIPE_FORMAT_RGB1, 1, 1, 1, 1, PIPE_FORMAT_TYPE_UNORM )
PIPE_FORMAT_R8_USCALED            = _PIPE_FORMAT_RGBAZS_8 ( _PIPE_FORMAT_R001, 1, 0, 0, 0, PIPE_FORMAT_TYPE_USCALED )
PIPE_FORMAT_R8G8_USCALED          = _PIPE_FORMAT_RGBAZS_8 ( _PIPE_FORMAT_RG01, 1, 1, 0, 0, PIPE_FORMAT_TYPE_USCALED )
PIPE_FORMAT_R8G8B8_USCALED        = _PIPE_FORMAT_RGBAZS_8 ( _PIPE_FORMAT_RGB1, 1, 1, 1, 0, PIPE_FORMAT_TYPE_USCALED )
PIPE_FORMAT_R8G8B8A8_USCALED      = _PIPE_FORMAT_RGBAZS_8 ( _PIPE_FORMAT_RGBA, 1, 1, 1, 1, PIPE_FORMAT_TYPE_USCALED )
PIPE_FORMAT_R8G8B8X8_USCALED      = _PIPE_FORMAT_RGBAZS_8 ( _PIPE_FORMAT_RGB1, 1, 1, 1, 1, PIPE_FORMAT_TYPE_USCALED )
PIPE_FORMAT_R8_SNORM              = _PIPE_FORMAT_RGBAZS_8 ( _PIPE_FORMAT_R001, 1, 0, 0, 0, PIPE_FORMAT_TYPE_SNORM )
PIPE_FORMAT_R8G8_SNORM            = _PIPE_FORMAT_RGBAZS_8 ( _PIPE_FORMAT_RG01, 1, 1, 0, 0, PIPE_FORMAT_TYPE_SNORM )
PIPE_FORMAT_R8G8B8_SNORM          = _PIPE_FORMAT_RGBAZS_8 ( _PIPE_FORMAT_RGB1, 1, 1, 1, 0, PIPE_FORMAT_TYPE_SNORM )
PIPE_FORMAT_R8G8B8A8_SNORM        = _PIPE_FORMAT_RGBAZS_8 ( _PIPE_FORMAT_RGBA, 1, 1, 1, 1, PIPE_FORMAT_TYPE_SNORM )
PIPE_FORMAT_R8G8B8X8_SNORM        = _PIPE_FORMAT_RGBAZS_8 ( _PIPE_FORMAT_RGB1, 1, 1, 1, 1, PIPE_FORMAT_TYPE_SNORM )
PIPE_FORMAT_R8_SSCALED            = _PIPE_FORMAT_RGBAZS_8 ( _PIPE_FORMAT_R001, 1, 0, 0, 0, PIPE_FORMAT_TYPE_SSCALED )
PIPE_FORMAT_R8G8_SSCALED          = _PIPE_FORMAT_RGBAZS_8 ( _PIPE_FORMAT_RG01, 1, 1, 0, 0, PIPE_FORMAT_TYPE_SSCALED )
PIPE_FORMAT_R8G8B8_SSCALED        = _PIPE_FORMAT_RGBAZS_8 ( _PIPE_FORMAT_RGB1, 1, 1, 1, 0, PIPE_FORMAT_TYPE_SSCALED )
PIPE_FORMAT_R8G8B8A8_SSCALED      = _PIPE_FORMAT_RGBAZS_8 ( _PIPE_FORMAT_RGBA, 1, 1, 1, 1, PIPE_FORMAT_TYPE_SSCALED )
PIPE_FORMAT_R8G8B8X8_SSCALED      = _PIPE_FORMAT_RGBAZS_8 ( _PIPE_FORMAT_RGB1, 1, 1, 1, 1, PIPE_FORMAT_TYPE_SSCALED )
PIPE_FORMAT_L8_SRGB               = _PIPE_FORMAT_RGBAZS_8 ( _PIPE_FORMAT_RRR1, 1, 1, 1, 0, PIPE_FORMAT_TYPE_SRGB )
PIPE_FORMAT_A8_L8_SRGB            = _PIPE_FORMAT_RGBAZS_8 ( _PIPE_FORMAT_RRRG, 1, 1, 1, 1, PIPE_FORMAT_TYPE_SRGB )
PIPE_FORMAT_R8G8B8_SRGB           = _PIPE_FORMAT_RGBAZS_8 ( _PIPE_FORMAT_RGB1, 1, 1, 1, 0, PIPE_FORMAT_TYPE_SRGB )
PIPE_FORMAT_R8G8B8A8_SRGB         = _PIPE_FORMAT_RGBAZS_8 ( _PIPE_FORMAT_RGBA, 1, 1, 1, 1, PIPE_FORMAT_TYPE_SRGB )
PIPE_FORMAT_R8G8B8X8_SRGB         = _PIPE_FORMAT_RGBAZS_8 ( _PIPE_FORMAT_RGB1, 1, 1, 1, 1, PIPE_FORMAT_TYPE_SRGB )
PIPE_FORMAT_DXT1_RGB              = _PIPE_FORMAT_DXT( 1, 8, 8, 8, 0 )
PIPE_FORMAT_DXT1_RGBA             = _PIPE_FORMAT_DXT( 1, 8, 8, 8, 8 )
PIPE_FORMAT_DXT3_RGBA             = _PIPE_FORMAT_DXT( 3, 8, 8, 8, 8 )
PIPE_FORMAT_DXT5_RGBA             = _PIPE_FORMAT_DXT( 5, 8, 8, 8, 8 )

formats = {
	PIPE_FORMAT_NONE: "PIPE_FORMAT_NONE",
	PIPE_FORMAT_A8R8G8B8_UNORM: "PIPE_FORMAT_A8R8G8B8_UNORM",
	PIPE_FORMAT_X8R8G8B8_UNORM: "PIPE_FORMAT_X8R8G8B8_UNORM",
	PIPE_FORMAT_B8G8R8A8_UNORM: "PIPE_FORMAT_B8G8R8A8_UNORM",
	PIPE_FORMAT_B8G8R8X8_UNORM: "PIPE_FORMAT_B8G8R8X8_UNORM",
	PIPE_FORMAT_A1R5G5B5_UNORM: "PIPE_FORMAT_A1R5G5B5_UNORM",
	PIPE_FORMAT_A4R4G4B4_UNORM: "PIPE_FORMAT_A4R4G4B4_UNORM",
	PIPE_FORMAT_R5G6B5_UNORM: "PIPE_FORMAT_R5G6B5_UNORM",
	PIPE_FORMAT_L8_UNORM: "PIPE_FORMAT_L8_UNORM",
	PIPE_FORMAT_A8_UNORM: "PIPE_FORMAT_A8_UNORM",
	PIPE_FORMAT_I8_UNORM: "PIPE_FORMAT_I8_UNORM",
	PIPE_FORMAT_A8L8_UNORM: "PIPE_FORMAT_A8L8_UNORM",
	PIPE_FORMAT_YCBCR: "PIPE_FORMAT_YCBCR",
	PIPE_FORMAT_YCBCR_REV: "PIPE_FORMAT_YCBCR_REV",
	PIPE_FORMAT_Z16_UNORM: "PIPE_FORMAT_Z16_UNORM",
	PIPE_FORMAT_Z32_UNORM: "PIPE_FORMAT_Z32_UNORM",
	PIPE_FORMAT_Z32_FLOAT: "PIPE_FORMAT_Z32_FLOAT",
	PIPE_FORMAT_S8Z24_UNORM: "PIPE_FORMAT_S8Z24_UNORM",
	PIPE_FORMAT_Z24S8_UNORM: "PIPE_FORMAT_Z24S8_UNORM",
	PIPE_FORMAT_X8Z24_UNORM: "PIPE_FORMAT_X8Z24_UNORM",
	PIPE_FORMAT_Z24X8_UNORM: "PIPE_FORMAT_Z24X8_UNORM",
	PIPE_FORMAT_S8_UNORM: "PIPE_FORMAT_S8_UNORM",
	PIPE_FORMAT_R64_FLOAT: "PIPE_FORMAT_R64_FLOAT",
	PIPE_FORMAT_R64G64_FLOAT: "PIPE_FORMAT_R64G64_FLOAT",
	PIPE_FORMAT_R64G64B64_FLOAT: "PIPE_FORMAT_R64G64B64_FLOAT",
	PIPE_FORMAT_R64G64B64A64_FLOAT: "PIPE_FORMAT_R64G64B64A64_FLOAT",
	PIPE_FORMAT_R32_FLOAT: "PIPE_FORMAT_R32_FLOAT",
	PIPE_FORMAT_R32G32_FLOAT: "PIPE_FORMAT_R32G32_FLOAT",
	PIPE_FORMAT_R32G32B32_FLOAT: "PIPE_FORMAT_R32G32B32_FLOAT",
	PIPE_FORMAT_R32G32B32A32_FLOAT: "PIPE_FORMAT_R32G32B32A32_FLOAT",
	PIPE_FORMAT_R32_UNORM: "PIPE_FORMAT_R32_UNORM",
	PIPE_FORMAT_R32G32_UNORM: "PIPE_FORMAT_R32G32_UNORM",
	PIPE_FORMAT_R32G32B32_UNORM: "PIPE_FORMAT_R32G32B32_UNORM",
	PIPE_FORMAT_R32G32B32A32_UNORM: "PIPE_FORMAT_R32G32B32A32_UNORM",
	PIPE_FORMAT_R32_USCALED: "PIPE_FORMAT_R32_USCALED",
	PIPE_FORMAT_R32G32_USCALED: "PIPE_FORMAT_R32G32_USCALED",
	PIPE_FORMAT_R32G32B32_USCALED: "PIPE_FORMAT_R32G32B32_USCALED",
	PIPE_FORMAT_R32G32B32A32_USCALED: "PIPE_FORMAT_R32G32B32A32_USCALED",
	PIPE_FORMAT_R32_SNORM: "PIPE_FORMAT_R32_SNORM",
	PIPE_FORMAT_R32G32_SNORM: "PIPE_FORMAT_R32G32_SNORM",
	PIPE_FORMAT_R32G32B32_SNORM: "PIPE_FORMAT_R32G32B32_SNORM",
	PIPE_FORMAT_R32G32B32A32_SNORM: "PIPE_FORMAT_R32G32B32A32_SNORM",
	PIPE_FORMAT_R32_SSCALED: "PIPE_FORMAT_R32_SSCALED",
	PIPE_FORMAT_R32G32_SSCALED: "PIPE_FORMAT_R32G32_SSCALED",
	PIPE_FORMAT_R32G32B32_SSCALED: "PIPE_FORMAT_R32G32B32_SSCALED",
	PIPE_FORMAT_R32G32B32A32_SSCALED: "PIPE_FORMAT_R32G32B32A32_SSCALED",
	PIPE_FORMAT_R16_UNORM: "PIPE_FORMAT_R16_UNORM",
	PIPE_FORMAT_R16G16_UNORM: "PIPE_FORMAT_R16G16_UNORM",
	PIPE_FORMAT_R16G16B16_UNORM: "PIPE_FORMAT_R16G16B16_UNORM",
	PIPE_FORMAT_R16G16B16A16_UNORM: "PIPE_FORMAT_R16G16B16A16_UNORM",
	PIPE_FORMAT_R16_USCALED: "PIPE_FORMAT_R16_USCALED",
	PIPE_FORMAT_R16G16_USCALED: "PIPE_FORMAT_R16G16_USCALED",
	PIPE_FORMAT_R16G16B16_USCALED: "PIPE_FORMAT_R16G16B16_USCALED",
	PIPE_FORMAT_R16G16B16A16_USCALED: "PIPE_FORMAT_R16G16B16A16_USCALED",
	PIPE_FORMAT_R16_SNORM: "PIPE_FORMAT_R16_SNORM",
	PIPE_FORMAT_R16G16_SNORM: "PIPE_FORMAT_R16G16_SNORM",
	PIPE_FORMAT_R16G16B16_SNORM: "PIPE_FORMAT_R16G16B16_SNORM",
	PIPE_FORMAT_R16G16B16A16_SNORM: "PIPE_FORMAT_R16G16B16A16_SNORM",
	PIPE_FORMAT_R16_SSCALED: "PIPE_FORMAT_R16_SSCALED",
	PIPE_FORMAT_R16G16_SSCALED: "PIPE_FORMAT_R16G16_SSCALED",
	PIPE_FORMAT_R16G16B16_SSCALED: "PIPE_FORMAT_R16G16B16_SSCALED",
	PIPE_FORMAT_R16G16B16A16_SSCALED: "PIPE_FORMAT_R16G16B16A16_SSCALED",
	PIPE_FORMAT_R8_UNORM: "PIPE_FORMAT_R8_UNORM",
	PIPE_FORMAT_R8G8_UNORM: "PIPE_FORMAT_R8G8_UNORM",
	PIPE_FORMAT_R8G8B8_UNORM: "PIPE_FORMAT_R8G8B8_UNORM",
	PIPE_FORMAT_R8G8B8A8_UNORM: "PIPE_FORMAT_R8G8B8A8_UNORM",
	PIPE_FORMAT_R8G8B8X8_UNORM: "PIPE_FORMAT_R8G8B8X8_UNORM",
	PIPE_FORMAT_R8_USCALED: "PIPE_FORMAT_R8_USCALED",
	PIPE_FORMAT_R8G8_USCALED: "PIPE_FORMAT_R8G8_USCALED",
	PIPE_FORMAT_R8G8B8_USCALED: "PIPE_FORMAT_R8G8B8_USCALED",
	PIPE_FORMAT_R8G8B8A8_USCALED: "PIPE_FORMAT_R8G8B8A8_USCALED",
	PIPE_FORMAT_R8G8B8X8_USCALED: "PIPE_FORMAT_R8G8B8X8_USCALED",
	PIPE_FORMAT_R8_SNORM: "PIPE_FORMAT_R8_SNORM",
	PIPE_FORMAT_R8G8_SNORM: "PIPE_FORMAT_R8G8_SNORM",
	PIPE_FORMAT_R8G8B8_SNORM: "PIPE_FORMAT_R8G8B8_SNORM",
	PIPE_FORMAT_R8G8B8A8_SNORM: "PIPE_FORMAT_R8G8B8A8_SNORM",
	PIPE_FORMAT_R8G8B8X8_SNORM: "PIPE_FORMAT_R8G8B8X8_SNORM",
	PIPE_FORMAT_R8_SSCALED: "PIPE_FORMAT_R8_SSCALED",
	PIPE_FORMAT_R8G8_SSCALED: "PIPE_FORMAT_R8G8_SSCALED",
	PIPE_FORMAT_R8G8B8_SSCALED: "PIPE_FORMAT_R8G8B8_SSCALED",
	PIPE_FORMAT_R8G8B8A8_SSCALED: "PIPE_FORMAT_R8G8B8A8_SSCALED",
	PIPE_FORMAT_R8G8B8X8_SSCALED: "PIPE_FORMAT_R8G8B8X8_SSCALED",
	PIPE_FORMAT_L8_SRGB: "PIPE_FORMAT_L8_SRGB",
	PIPE_FORMAT_A8_L8_SRGB: "PIPE_FORMAT_A8_L8_SRGB",
	PIPE_FORMAT_R8G8B8_SRGB: "PIPE_FORMAT_R8G8B8_SRGB",
	PIPE_FORMAT_R8G8B8A8_SRGB: "PIPE_FORMAT_R8G8B8A8_SRGB",
	PIPE_FORMAT_R8G8B8X8_SRGB: "PIPE_FORMAT_R8G8B8X8_SRGB",
	PIPE_FORMAT_DXT1_RGB: "PIPE_FORMAT_DXT1_RGB",
	PIPE_FORMAT_DXT1_RGBA: "PIPE_FORMAT_DXT1_RGBA",
	PIPE_FORMAT_DXT3_RGBA: "PIPE_FORMAT_DXT3_RGBA",
	PIPE_FORMAT_DXT5_RGBA: "PIPE_FORMAT_DXT5_RGBA",
}


def read_header(infile):
	header_fmt = "IIII"
	header = infile.read(struct.calcsize(header_fmt))
	return struct.unpack_from(header_fmt, header)

def read_pixel(infile, fmt, cpp):
	assert cpp == 4
	r, g, b, a = map(ord, infile.read(4))
	return r, g, b, a


def process(infilename, outfilename):
	infile = open(infilename, "rb")
	format, cpp, width, height = read_header(infile)
	sys.stderr.write("format = %s, cpp = %u, width = %u, height = %u\n" % (formats[format], cpp, width, height))
	outimage = Image.new(
    mode='RGB',
    size=(width, height),
    color=(0,0,0))
	outpixels = outimage.load()
	for y in range(height):
		for x in range(width):
			r, g, b, a = read_pixel(infile, format, cpp)
			outpixels[x, y] = r, g, b
	outimage.save(outfilename, "PNG")


def main():
	if sys.platform == 'win32':
		# wildcard expansion
		from glob import glob
		args = []
		for arg in sys.argv[1:]:
			args.extend(glob(arg))
	else:
		args = sys.argv[1:]
	for infilename in args:
		root, ext = os.path.splitext(infilename)
		outfilename = root + ".png"
		process(infilename, outfilename)


if __name__ == '__main__':
	main()