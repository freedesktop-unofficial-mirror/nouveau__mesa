/**************************************************************************
 * 
 * Copyright 2008 Tungsten Graphics, Inc., Cedar Park, Texas.
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


#include "pipe/p_util.h"
#include "pipe/p_winsys.h"
#include "pipe/p_defines.h"
#include "pipe/p_screen.h"

#include "sp_texture.h"
#include "sp_winsys.h"


static const char *
softpipe_get_vendor(struct pipe_screen *screen)
{
   return "Tungsten Graphics, Inc.";
}


static const char *
softpipe_get_name(struct pipe_screen *screen)
{
   return "softpipe";
}


static int
softpipe_get_param(struct pipe_screen *screen, int param)
{
   switch (param) {
   case PIPE_CAP_MAX_TEXTURE_IMAGE_UNITS:
      return 8;
   case PIPE_CAP_NPOT_TEXTURES:
      return 1;
   case PIPE_CAP_TWO_SIDED_STENCIL:
      return 1;
   case PIPE_CAP_GLSL:
      return 1;
   case PIPE_CAP_S3TC:
      return 0;
   case PIPE_CAP_ANISOTROPIC_FILTER:
      return 0;
   case PIPE_CAP_POINT_SPRITE:
      return 1;
   case PIPE_CAP_MAX_RENDER_TARGETS:
      return 1;
   case PIPE_CAP_OCCLUSION_QUERY:
      return 1;
   case PIPE_CAP_TEXTURE_SHADOW_MAP:
      return 1;
   case PIPE_CAP_MAX_TEXTURE_2D_LEVELS:
      return 12; /* max 2Kx2K */
   case PIPE_CAP_MAX_TEXTURE_3D_LEVELS:
      return 8;  /* max 128x128x128 */
   case PIPE_CAP_MAX_TEXTURE_CUBE_LEVELS:
      return 12; /* max 2Kx2K */
   default:
      return 0;
   }
}


static float
softpipe_get_paramf(struct pipe_screen *screen, int param)
{
   switch (param) {
   case PIPE_CAP_MAX_LINE_WIDTH:
      /* fall-through */
   case PIPE_CAP_MAX_LINE_WIDTH_AA:
      return 255.0; /* arbitrary */
   case PIPE_CAP_MAX_POINT_WIDTH:
      /* fall-through */
   case PIPE_CAP_MAX_POINT_WIDTH_AA:
      return 255.0; /* arbitrary */
   case PIPE_CAP_MAX_TEXTURE_ANISOTROPY:
      return 0.0;
   case PIPE_CAP_MAX_TEXTURE_LOD_BIAS:
      return 16.0; /* arbitrary */
   default:
      return 0;
   }
}


/**
 * Query format support for creating a texture, drawing surface, etc.
 * \param format  the format to test
 * \param type  one of PIPE_TEXTURE, PIPE_SURFACE
 */
static boolean
softpipe_is_format_supported( struct pipe_screen *screen,
                              enum pipe_format format, uint type )
{
   switch (type) {
   case PIPE_TEXTURE:
      /* softpipe supports all texture formats */
      return TRUE;
   case PIPE_SURFACE:
      /* softpipe supports all (off-screen) surface formats */
      return TRUE;
   default:
      assert(0);
      return FALSE;
   }
}


static void
softpipe_destroy_screen( struct pipe_screen *screen )
{
   FREE(screen);
}


/**
 * Create a new pipe_screen object
 * Note: we're not presently subclassing pipe_screen (no softpipe_screen).
 */
struct pipe_screen *
softpipe_create_screen(struct pipe_winsys *winsys)
{
   struct pipe_screen *screen = CALLOC_STRUCT(pipe_screen);

   if (!screen)
      return NULL;

   screen->winsys = winsys;

   screen->destroy = softpipe_destroy_screen;

   screen->get_name = softpipe_get_name;
   screen->get_vendor = softpipe_get_vendor;
   screen->get_param = softpipe_get_param;
   screen->get_paramf = softpipe_get_paramf;
   screen->is_format_supported = softpipe_is_format_supported;

   softpipe_init_screen_texture_funcs(screen);

   return screen;
}