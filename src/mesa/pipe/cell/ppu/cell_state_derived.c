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

#include "pipe/p_util.h"
#include "pipe/p_shader_tokens.h"
#include "pipe/draw/draw_context.h"
#include "pipe/draw/draw_vertex.h"
#include "cell_context.h"
#include "cell_state.h"


/**
 * Determine which post-transform / pre-rasterization vertex attributes
 * we need.
 * Derived from:  fs, setup states.
 */
static void calculate_vertex_layout( struct cell_context *cell )
{
#if 0
   const struct pipe_shader_state *vs = cell->vs->state;
   const struct pipe_shader_state *fs = &cell->fs->shader;
   const enum interp_mode colorInterp
      = cell->rasterizer->flatshade ? INTERP_CONSTANT : INTERP_LINEAR;
   struct vertex_info *vinfo = &cell->vertex_info;
   boolean emitBack0 = FALSE, emitBack1 = FALSE, emitPsize = FALSE;
   uint front0 = 0, back0 = 0, front1 = 0, back1 = 0;
   uint i;
#endif
   const enum interp_mode colorInterp
      = cell->rasterizer->flatshade ? INTERP_CONSTANT : INTERP_LINEAR;
   struct vertex_info *vinfo = &cell->vertex_info;
   uint front0;

   memset(vinfo, 0, sizeof(*vinfo));

#if 0
   if (fs->input_semantic_name[0] == TGSI_SEMANTIC_POSITION) {
      /* Need Z if depth test is enabled or the fragment program uses the
       * fragment position (XYZW).
       */
   }

   cell->psize_slot = -1;
#endif

   /* always emit vertex pos */
   draw_emit_vertex_attr(vinfo, FORMAT_4F, INTERP_LINEAR);

#if 1
   front0 = draw_emit_vertex_attr(vinfo, FORMAT_4F, colorInterp);
#endif

#if 0
   /*
    * XXX I think we need to reconcile the vertex shader outputs with
    * the fragment shader inputs here to make sure the slots line up.
    * Might just be getting lucky so far.
    * Or maybe do that in the state tracker?
    */

   for (i = 0; i < vs->num_outputs; i++) {
      switch (vs->output_semantic_name[i]) {

      case TGSI_SEMANTIC_POSITION:
         /* vertex programs always emit position, but might not be
          * needed for fragment progs.
          */
         /* no-op */
         break;

      case TGSI_SEMANTIC_COLOR:
         if (vs->output_semantic_index[i] == 0) {
            front0 = draw_emit_vertex_attr(vinfo, FORMAT_4F, colorInterp);
         }
         else {
            assert(vs->output_semantic_index[i] == 1);
            front1 = draw_emit_vertex_attr(vinfo, FORMAT_4F, colorInterp);
         }
         break;

      case TGSI_SEMANTIC_BCOLOR:
         if (vs->output_semantic_index[i] == 0) {
            emitBack0 = TRUE;
         }
         else {
            assert(vs->output_semantic_index[i] == 1);
            emitBack1 = TRUE;
         }
         break;

      case TGSI_SEMANTIC_FOG:
         draw_emit_vertex_attr(vinfo, FORMAT_1F, INTERP_PERSPECTIVE);
         break;

      case TGSI_SEMANTIC_PSIZE:
         /* XXX only emit if drawing points or front/back polygon mode
          * is point mode
          */
         emitPsize = TRUE;
         break;

      case TGSI_SEMANTIC_GENERIC:
         /* this includes texcoords and varying vars */
         draw_emit_vertex_attr(vinfo, FORMAT_4F, INTERP_PERSPECTIVE);
         break;

      default:
         assert(0);
      }
   }

   cell->nr_frag_attrs = fs->num_inputs;

   /* We want these after all other attribs since they won't get passed
    * to the fragment shader.  All prior vertex output attribs should match
    * up 1:1 with the fragment shader inputs.
    */
   if (emitBack0) {
      back0 = draw_emit_vertex_attr(vinfo, FORMAT_4F, colorInterp);
   }
   if (emitBack1) {
      back1 = draw_emit_vertex_attr(vinfo, FORMAT_4F, colorInterp);
   }
   if (emitPsize) {
      cell->psize_slot
         = draw_emit_vertex_attr(vinfo, FORMAT_1F, INTERP_CONSTANT);
   }

   /* If the attributes have changed, tell the draw module about
    * the new vertex layout.
    */
   /* XXX we also need to do this when the shading mode (interp modes) change: */
#endif

   if (1/*vinfo->attr_mask != cell->attr_mask*/) {
      /*cell->attr_mask = vinfo->attr_mask;*/

      draw_compute_vertex_size(vinfo);
      draw_set_vertex_info(cell->draw, vinfo);

#if 0
      draw_set_twoside_attributes(cell->draw,
                                  front0, back0, front1, back1);
#endif
   }
}


#if 0
/**
 * Recompute cliprect from scissor bounds, scissor enable and surface size.
 */
static void
compute_cliprect(struct cell_context *sp)
{
   unsigned surfWidth, surfHeight;

   if (sp->framebuffer.num_cbufs > 0) {
      surfWidth = sp->framebuffer.cbufs[0]->width;
      surfHeight = sp->framebuffer.cbufs[0]->height;
   }
   else {
      /* no surface? */
      surfWidth = sp->scissor.maxx;
      surfHeight = sp->scissor.maxy;
   }

   if (sp->rasterizer->scissor) {
      /* clip to scissor rect */
      sp->cliprect.minx = MAX2(sp->scissor.minx, 0);
      sp->cliprect.miny = MAX2(sp->scissor.miny, 0);
      sp->cliprect.maxx = MIN2(sp->scissor.maxx, surfWidth);
      sp->cliprect.maxy = MIN2(sp->scissor.maxy, surfHeight);
   }
   else {
      /* clip to surface bounds */
      sp->cliprect.minx = 0;
      sp->cliprect.miny = 0;
      sp->cliprect.maxx = surfWidth;
      sp->cliprect.maxy = surfHeight;
   }
}
#endif


void cell_update_derived( struct cell_context *cell )
{
   if (cell->dirty & (CELL_NEW_RASTERIZER | CELL_NEW_FS))
      calculate_vertex_layout( cell );

#if 0
   if (cell->dirty & (CELL_NEW_SCISSOR |
                      CELL_NEW_DEPTH_STENCIL_ALPHA |
                      CELL_NEW_FRAMEBUFFER))
      compute_cliprect(cell);
#endif

   //cell_emit_state(cell);

   cell->dirty = 0;
}