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

/**
 * Authors
 *  Brian Paul
 */


#include <stdio.h>

#include "pipe/p_defines.h"
#include "pipe/p_format.h"
#include "pipe/p_util.h"
#include "pipe/p_winsys.h"
#include "pipe/cell/common.h"
#include "pipe/draw/draw_context.h"
#include "cell_context.h"
#include "cell_draw_arrays.h"
#include "cell_flush.h"
#include "cell_render.h"
#include "cell_state.h"
#include "cell_surface.h"
#include "cell_spu.h"



static boolean
cell_is_format_supported( struct pipe_context *pipe,
                          enum pipe_format format, uint type )
{
   struct cell_context *cell = cell_context( pipe );

   switch (type) {
   case PIPE_TEXTURE:
      /* cell supports all texture formats, XXX for now anyway */
      return TRUE;
   case PIPE_SURFACE:
      /* cell supports all (off-screen) surface formats, XXX for now */
      return TRUE;
   default:
      assert(0);
      return FALSE;
   }
}


static int cell_get_param(struct pipe_context *pipe, int param)
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

static float cell_get_paramf(struct pipe_context *pipe, int param)
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


static const char *
cell_get_name( struct pipe_context *pipe )
{
   return "Cell";
}

static const char *
cell_get_vendor( struct pipe_context *pipe )
{
   return "Tungsten Graphics, Inc.";
}



static void
cell_destroy_context( struct pipe_context *pipe )
{
   struct cell_context *cell = cell_context(pipe);

   cell_spu_exit(cell);
   wait_spus(cell->num_spus);

   free(cell);
}




struct pipe_context *
cell_create_context(struct pipe_winsys *winsys, struct cell_winsys *cws)
{
   struct cell_context *cell;

   /* some fields need to be 16-byte aligned, so align the whole object */
   cell = (struct cell_context*) align_malloc(sizeof(struct cell_context), 16);
   if (!cell)
      return NULL;

   memset(cell, 0, sizeof(*cell));

   cell->winsys = cws;
   cell->pipe.winsys = winsys;
   cell->pipe.destroy = cell_destroy_context;

   /* queries */
   cell->pipe.is_format_supported = cell_is_format_supported;
   cell->pipe.get_name = cell_get_name;
   cell->pipe.get_vendor = cell_get_vendor;
   cell->pipe.get_param = cell_get_param;
   cell->pipe.get_paramf = cell_get_paramf;


   /* state setters */
   cell->pipe.create_blend_state = cell_create_blend_state;
   cell->pipe.bind_blend_state   = cell_bind_blend_state;
   cell->pipe.delete_blend_state = cell_delete_blend_state;

   cell->pipe.create_sampler_state = cell_create_sampler_state;
   cell->pipe.bind_sampler_state   = cell_bind_sampler_state;
   cell->pipe.delete_sampler_state = cell_delete_sampler_state;

   cell->pipe.create_depth_stencil_alpha_state = cell_create_depth_stencil_alpha_state;
   cell->pipe.bind_depth_stencil_alpha_state   = cell_bind_depth_stencil_alpha_state;
   cell->pipe.delete_depth_stencil_alpha_state = cell_delete_depth_stencil_alpha_state;

   cell->pipe.create_rasterizer_state = cell_create_rasterizer_state;
   cell->pipe.bind_rasterizer_state   = cell_bind_rasterizer_state;
   cell->pipe.delete_rasterizer_state = cell_delete_rasterizer_state;

   cell->pipe.create_fs_state = cell_create_fs_state;
   cell->pipe.bind_fs_state   = cell_bind_fs_state;
   cell->pipe.delete_fs_state = cell_delete_fs_state;

   cell->pipe.create_vs_state = cell_create_vs_state;
   cell->pipe.bind_vs_state   = cell_bind_vs_state;
   cell->pipe.delete_vs_state = cell_delete_vs_state;

   cell->pipe.set_blend_color = cell_set_blend_color;
   cell->pipe.set_clip_state = cell_set_clip_state;
   cell->pipe.set_constant_buffer = cell_set_constant_buffer;

   cell->pipe.set_framebuffer_state = cell_set_framebuffer_state;

   cell->pipe.set_polygon_stipple = cell_set_polygon_stipple;
   cell->pipe.set_scissor_state = cell_set_scissor_state;
   cell->pipe.set_viewport_state = cell_set_viewport_state;

   cell->pipe.set_vertex_buffer = cell_set_vertex_buffer;
   cell->pipe.set_vertex_element = cell_set_vertex_element;

   cell->pipe.draw_arrays = cell_draw_arrays;
   cell->pipe.draw_elements = cell_draw_elements;

   cell->pipe.clear = cell_clear_surface;
   cell->pipe.flush = cell_flush;

#if 0
   cell->pipe.begin_query = cell_begin_query;
   cell->pipe.end_query = cell_end_query;
   cell->pipe.wait_query = cell_wait_query;

   /* textures */
   cell->pipe.mipmap_tree_layout = cell_mipmap_tree_layout;
   cell->pipe.get_tex_surface = cell_get_tex_surface;
#endif


   cell->draw = draw_create();

   cell->render_stage = cell_draw_render_stage(cell);
   draw_set_rasterize_stage(cell->draw, cell->render_stage);


   /*
    * SPU stuff
    */
   cell->num_spus = 6;  /* XXX >6 seems to fail */

   cell_start_spus(cell->num_spus);

#if 0
   test_spus(cell);
   wait_spus();
#endif

   return &cell->pipe;
}