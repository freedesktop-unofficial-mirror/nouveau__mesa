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

/* Authors:  Keith Whitwell <keith@tungstengraphics.com>
 */

#include "fo_context.h"


/* This looks like a lot of work at the moment - we're keeping a
 * duplicate copy of the state up-to-date.  
 *
 * This can change in two ways:
 * - With constant state objects we would only need to save a pointer,
 *     not the whole object.
 * - By adding a callback in the state tracker to re-emit state.  The
 *     state tracker knows the current state already and can re-emit it 
 *     without additional complexity.
 *
 * This works as a proof-of-concept, but a final version will have
 * lower overheads.
 */

static void *
failover_create_alpha_test_state(struct pipe_context *pipe,
                                 const struct pipe_alpha_test_state *templ)
{
   struct fo_state *state = malloc(sizeof(struct fo_state));
   struct failover_context *failover = failover_context(pipe);

   state->sw_state = failover->sw->create_alpha_test_state(pipe, templ);
   state->hw_state = failover->hw->create_alpha_test_state(pipe, templ);

   return state;
}

static void
failover_bind_alpha_test_state(struct pipe_context *pipe,
                               void *alpha)
{
   struct failover_context *failover = failover_context(pipe);
   struct fo_state *state = (struct fo_state *)alpha;

   failover->alpha_test = state;
   failover->dirty |= FO_NEW_ALPHA_TEST;
   failover->hw->bind_alpha_test_state(failover->hw,
                                       state->hw_state);
}

static void
failover_delete_alpha_test_state(struct pipe_context *pipe,
                                 void *alpha)
{
   struct fo_state *state = (struct fo_state*)alpha;
   struct failover_context *failover = failover_context(pipe);

   failover->sw->delete_alpha_test_state(pipe, state->sw_state);
   failover->hw->delete_alpha_test_state(pipe, state->hw_state);
   state->sw_state = 0;
   state->hw_state = 0;
   free(state);
}


static void *
failover_create_blend_state( struct pipe_context *pipe,
                             const struct pipe_blend_state *blend )
{
   struct fo_state *state = malloc(sizeof(struct fo_state));
   struct failover_context *failover = failover_context(pipe);

   state->sw_state = failover->sw->create_blend_state(pipe, blend);
   state->hw_state = failover->hw->create_blend_state(pipe, blend);

   return state;
}

static void
failover_bind_blend_state( struct pipe_context *pipe,
                           void *blend )
{
   struct failover_context *failover = failover_context(pipe);
   struct fo_state *state = (struct fo_state *)blend;
   failover->blend = state;
   failover->dirty |= FO_NEW_BLEND;
   failover->hw->bind_blend_state( failover->hw, state->hw_state );
}

static void
failover_delete_blend_state( struct pipe_context *pipe,
                             void *blend )
{
   struct fo_state *state = (struct fo_state*)blend;
   struct failover_context *failover = failover_context(pipe);

   failover->sw->delete_blend_state(pipe, state->sw_state);
   failover->hw->delete_blend_state(pipe, state->hw_state);
   state->sw_state = 0;
   state->hw_state = 0;
   free(state);
}

static void
failover_set_blend_color( struct pipe_context *pipe,
			  const struct pipe_blend_color *blend_color )
{
   struct failover_context *failover = failover_context(pipe);

   failover->blend_color = *blend_color;
   failover->dirty |= FO_NEW_BLEND_COLOR;
   failover->hw->set_blend_color( failover->hw, blend_color );
}

static void 
failover_set_clip_state( struct pipe_context *pipe,
			 const struct pipe_clip_state *clip )
{
   struct failover_context *failover = failover_context(pipe);

   failover->clip = *clip;
   failover->dirty |= FO_NEW_CLIP;
   failover->hw->set_clip_state( failover->hw, clip );
}

static void 
failover_set_clear_color_state( struct pipe_context *pipe,
				const struct pipe_clear_color_state *clear_color )
{
   struct failover_context *failover = failover_context(pipe);

   failover->clear_color = *clear_color;
   failover->dirty |= FO_NEW_CLEAR_COLOR;
   failover->hw->set_clear_color_state( failover->hw, clear_color );
}

static void *
failover_create_depth_stencil_state(struct pipe_context *pipe,
                              const struct pipe_depth_stencil_state *templ)
{
   struct fo_state *state = malloc(sizeof(struct fo_state));
   struct failover_context *failover = failover_context(pipe);

   state->sw_state = failover->sw->create_depth_stencil_state(pipe, templ);
   state->hw_state = failover->hw->create_depth_stencil_state(pipe, templ);

   return state;
}

static void
failover_bind_depth_stencil_state(struct pipe_context *pipe,
                                  void *depth_stencil)
{
   struct failover_context *failover = failover_context(pipe);
   struct fo_state *state = (struct fo_state *)depth_stencil;
   failover->depth_stencil = state;
   failover->dirty |= FO_NEW_DEPTH_STENCIL;
   failover->hw->bind_depth_stencil_state(failover->hw, state->hw_state);
}

static void
failover_delete_depth_stencil_state(struct pipe_context *pipe,
                                    void *ds)
{
   struct fo_state *state = (struct fo_state*)ds;
   struct failover_context *failover = failover_context(pipe);

   failover->sw->delete_depth_stencil_state(pipe, state->sw_state);
   failover->hw->delete_depth_stencil_state(pipe, state->hw_state);
   state->sw_state = 0;
   state->hw_state = 0;
   free(state);
}

static void
failover_set_framebuffer_state(struct pipe_context *pipe,
			       const struct pipe_framebuffer_state *framebuffer)
{
   struct failover_context *failover = failover_context(pipe);

   failover->framebuffer = *framebuffer;
   failover->dirty |= FO_NEW_FRAMEBUFFER;
   failover->hw->set_framebuffer_state( failover->hw, framebuffer );
}


static void *
failover_create_fs_state(struct pipe_context *pipe,
                         const struct pipe_shader_state *templ)
{
   struct fo_state *state = malloc(sizeof(struct fo_state));
   struct failover_context *failover = failover_context(pipe);

   state->sw_state = failover->sw->create_fs_state(pipe, templ);
   state->hw_state = failover->hw->create_fs_state(pipe, templ);

   return state;
}

static void
failover_bind_fs_state(struct pipe_context *pipe, void *fs)
{
   struct failover_context *failover = failover_context(pipe);
   struct fo_state *state = (struct fo_state*)fs;
   failover->fragment_shader = state;
   failover->dirty |= FO_NEW_FRAGMENT_SHADER;
   failover->hw->bind_fs_state(failover->hw, state->hw_state);
}

static void
failover_delete_fs_state(struct pipe_context *pipe,
                         void *fs)
{
   struct fo_state *state = (struct fo_state*)fs;
   struct failover_context *failover = failover_context(pipe);

   failover->sw->delete_fs_state(pipe, state->sw_state);
   failover->hw->delete_fs_state(pipe, state->hw_state);
   state->sw_state = 0;
   state->hw_state = 0;
   free(state);
}

static void *
failover_create_vs_state(struct pipe_context *pipe,
                         const struct pipe_shader_state *templ)
{
   struct fo_state *state = malloc(sizeof(struct fo_state));
   struct failover_context *failover = failover_context(pipe);

   state->sw_state = failover->sw->create_vs_state(pipe, templ);
   state->hw_state = failover->hw->create_vs_state(pipe, templ);

   return state;
}

static void
failover_bind_vs_state(struct pipe_context *pipe,
                       void *vs)
{
   struct failover_context *failover = failover_context(pipe);

   struct fo_state *state = (struct fo_state*)vs;
   failover->vertex_shader = state;
   failover->dirty |= FO_NEW_VERTEX_SHADER;
   failover->hw->bind_vs_state(failover->hw, state->hw_state);
}

static void
failover_delete_vs_state(struct pipe_context *pipe,
                         void *vs)
{
   struct fo_state *state = (struct fo_state*)vs;
   struct failover_context *failover = failover_context(pipe);

   failover->sw->delete_vs_state(pipe, state->sw_state);
   failover->hw->delete_vs_state(pipe, state->hw_state);
   state->sw_state = 0;
   state->hw_state = 0;
   free(state);
}

static void 
failover_set_polygon_stipple( struct pipe_context *pipe,
			      const struct pipe_poly_stipple *stipple )
{
   struct failover_context *failover = failover_context(pipe);

   failover->poly_stipple = *stipple;
   failover->dirty |= FO_NEW_STIPPLE;
   failover->hw->set_polygon_stipple( failover->hw, stipple );
}

static void
failover_set_sampler_units( struct pipe_context *pipe,
                            uint num_samplers, const uint *units )
{
   struct failover_context *failover = failover_context(pipe);
   uint i;

   for (i = 0; i < num_samplers; i++)
      failover->sampler_units[i] = units[i];
   failover->dirty |= FO_NEW_SAMPLER;
   failover->hw->set_sampler_units(failover->hw, num_samplers, units);
}

static void *
failover_create_rasterizer_state(struct pipe_context *pipe,
                                 const struct pipe_rasterizer_state *templ)
{
   struct fo_state *state = malloc(sizeof(struct fo_state));
   struct failover_context *failover = failover_context(pipe);

   state->sw_state = failover->sw->create_rasterizer_state(pipe, templ);
   state->hw_state = failover->hw->create_rasterizer_state(pipe, templ);

   return state;
}

static void
failover_bind_rasterizer_state(struct pipe_context *pipe,
                               void *raster)
{
   struct failover_context *failover = failover_context(pipe);

   struct fo_state *state = (struct fo_state*)raster;
   failover->rasterizer = state;
   failover->dirty |= FO_NEW_RASTERIZER;
   failover->hw->bind_rasterizer_state(failover->hw, state->hw_state);
}

static void
failover_delete_rasterizer_state(struct pipe_context *pipe,
                                 void *raster)
{
   struct fo_state *state = (struct fo_state*)raster;
   struct failover_context *failover = failover_context(pipe);

   failover->sw->delete_rasterizer_state(pipe, state->sw_state);
   failover->hw->delete_rasterizer_state(pipe, state->hw_state);
   state->sw_state = 0;
   state->hw_state = 0;
   free(state);
}


static void 
failover_set_scissor_state( struct pipe_context *pipe,
                                 const struct pipe_scissor_state *scissor )
{
   struct failover_context *failover = failover_context(pipe);

   failover->scissor = *scissor;
   failover->dirty |= FO_NEW_SCISSOR;
   failover->hw->set_scissor_state( failover->hw, scissor );
}


static void *
failover_create_sampler_state(struct pipe_context *pipe,
                              const struct pipe_sampler_state *templ)
{
   struct fo_state *state = malloc(sizeof(struct fo_state));
   struct failover_context *failover = failover_context(pipe);

   state->sw_state = failover->sw->create_sampler_state(pipe, templ);
   state->hw_state = failover->hw->create_sampler_state(pipe, templ);

   return state;
}

static void
failover_bind_sampler_state(struct pipe_context *pipe,
			   unsigned unit, void *sampler)
{
   struct failover_context *failover = failover_context(pipe);
   struct fo_state *state = (struct fo_state*)sampler;
   failover->sampler[unit] = state;
   failover->dirty |= FO_NEW_SAMPLER;
   failover->dirty_sampler |= (1<<unit);
   failover->hw->bind_sampler_state(failover->hw, unit,
                                    state->hw_state);
}

static void
failover_delete_sampler_state(struct pipe_context *pipe, void *sampler)
{
   struct fo_state *state = (struct fo_state*)sampler;
   struct failover_context *failover = failover_context(pipe);

   failover->sw->delete_sampler_state(pipe, state->sw_state);
   failover->hw->delete_sampler_state(pipe, state->hw_state);
   state->sw_state = 0;
   state->hw_state = 0;
   free(state);
}


static void
failover_set_texture_state(struct pipe_context *pipe,
			   unsigned unit,
                           struct pipe_texture *texture)
{
   struct failover_context *failover = failover_context(pipe);

   failover->texture[unit] = texture;
   failover->dirty |= FO_NEW_TEXTURE;
   failover->dirty_texture |= (1<<unit);
   failover->hw->set_texture_state( failover->hw, unit, texture );
}


static void 
failover_set_viewport_state( struct pipe_context *pipe,
			     const struct pipe_viewport_state *viewport )
{
   struct failover_context *failover = failover_context(pipe);

   failover->viewport = *viewport; 
   failover->dirty |= FO_NEW_VIEWPORT;
   failover->hw->set_viewport_state( failover->hw, viewport );
}


static void
failover_set_vertex_buffer(struct pipe_context *pipe,
			   unsigned unit,
                           const struct pipe_vertex_buffer *vertex_buffer)
{
   struct failover_context *failover = failover_context(pipe);

   failover->vertex_buffer[unit] = *vertex_buffer;
   failover->dirty |= FO_NEW_VERTEX_BUFFER;
   failover->dirty_vertex_buffer |= (1<<unit);
   failover->hw->set_vertex_buffer( failover->hw, unit, vertex_buffer );
}


static void
failover_set_vertex_element(struct pipe_context *pipe,
			    unsigned unit,
			    const struct pipe_vertex_element *vertex_element)
{
   struct failover_context *failover = failover_context(pipe);

   failover->vertex_element[unit] = *vertex_element;
   failover->dirty |= FO_NEW_VERTEX_ELEMENT;
   failover->dirty_vertex_element |= (1<<unit);
   failover->hw->set_vertex_element( failover->hw, unit, vertex_element );
}


void
failover_init_state_functions( struct failover_context *failover )
{
   failover->pipe.create_alpha_test_state = failover_create_alpha_test_state;
   failover->pipe.bind_alpha_test_state   = failover_bind_alpha_test_state;
   failover->pipe.delete_alpha_test_state = failover_delete_alpha_test_state;
   failover->pipe.create_blend_state = failover_create_blend_state;
   failover->pipe.bind_blend_state   = failover_bind_blend_state;
   failover->pipe.delete_blend_state = failover_delete_blend_state;
   failover->pipe.create_sampler_state = failover_create_sampler_state;
   failover->pipe.bind_sampler_state   = failover_bind_sampler_state;
   failover->pipe.delete_sampler_state = failover_delete_sampler_state;
   failover->pipe.create_depth_stencil_state = failover_create_depth_stencil_state;
   failover->pipe.bind_depth_stencil_state   = failover_bind_depth_stencil_state;
   failover->pipe.delete_depth_stencil_state = failover_delete_depth_stencil_state;
   failover->pipe.create_rasterizer_state = failover_create_rasterizer_state;
   failover->pipe.bind_rasterizer_state = failover_bind_rasterizer_state;
   failover->pipe.delete_rasterizer_state = failover_delete_rasterizer_state;
   failover->pipe.create_fs_state = failover_create_fs_state;
   failover->pipe.bind_fs_state   = failover_bind_fs_state;
   failover->pipe.delete_fs_state = failover_delete_fs_state;
   failover->pipe.create_vs_state = failover_create_vs_state;
   failover->pipe.bind_vs_state   = failover_bind_vs_state;
   failover->pipe.delete_vs_state = failover_delete_vs_state;

   failover->pipe.set_blend_color = failover_set_blend_color;
   failover->pipe.set_clip_state = failover_set_clip_state;
   failover->pipe.set_clear_color_state = failover_set_clear_color_state;
   failover->pipe.set_framebuffer_state = failover_set_framebuffer_state;
   failover->pipe.set_polygon_stipple = failover_set_polygon_stipple;
   failover->pipe.set_sampler_units = failover_set_sampler_units;
   failover->pipe.set_scissor_state = failover_set_scissor_state;
   failover->pipe.set_texture_state = failover_set_texture_state;
   failover->pipe.set_viewport_state = failover_set_viewport_state;
   failover->pipe.set_vertex_buffer = failover_set_vertex_buffer;
   failover->pipe.set_vertex_element = failover_set_vertex_element;
}