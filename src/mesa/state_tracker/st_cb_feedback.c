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
 * GL_SELECT and GL_FEEDBACK render modes.
 * Basically, we use a private instance of the 'draw' module for doing
 * selection/feedback.  It would be nice to use the transform_feedback
 * hardware feature, but it's defined as happening pre-clip and we want
 * post-clipped primitives.  Also, there's concerns about the efficiency
 * of using the hardware for this anyway.
 *
 * Authors:
 *   Brian Paul
 */

#include "main/imports.h"
#include "main/context.h"
#include "main/feedback.h"
#include "main/macros.h"

#include "vbo/vbo.h"
#include "vbo/vbo_context.h"

#include "st_context.h"
#include "st_atom.h"
#include "st_draw.h"
#include "st_cb_feedback.h"
#include "st_cb_bufferobjects.h"

#include "pipe/p_context.h"
#include "pipe/p_defines.h"
#include "pipe/p_winsys.h"
#include "pipe/tgsi/exec/tgsi_attribs.h"
#include "vf/vf.h"

#include "pipe/draw/draw_context.h"
#include "pipe/draw/draw_private.h"


/**
 * This is actually used for both feedback and selection.
 */
struct feedback_stage
{
   struct draw_stage stage;   /**< Base class */
   GLcontext *ctx;            /**< Rendering context */
   GLboolean reset_stipple_counter;
};


/**********************************************************************
 * GL Feedback functions
 **********************************************************************/

static INLINE struct feedback_stage *
feedback_stage( struct draw_stage *stage )
{
   return (struct feedback_stage *)stage;
}


static void
feedback_vertex(GLcontext *ctx, const struct draw_context *draw,
                const struct vertex_header *v)
{
   const struct st_context *st = ctx->st;
   GLfloat win[4];
   const GLfloat *color, *texcoord;
   const GLfloat ci = 0;
   GLuint slot;

   win[0] = v->data[0][0];
   win[1] = v->data[0][1];
   win[2] = v->data[0][2];
   win[3] = 1.0F / v->data[0][3];

   /* XXX
    * When we compute vertex layout, save info about position of the
    * color and texcoord attribs to use here.
    */

   slot = st->vertex_attrib_to_slot[VERT_RESULT_COL0];
   if (slot)
      color = v->data[slot];
   else
      color = ctx->Current.Attrib[VERT_ATTRIB_COLOR0];

   slot = st->vertex_attrib_to_slot[VERT_RESULT_TEX0];
   if (slot)
      texcoord = v->data[slot];
   else
      texcoord = ctx->Current.Attrib[VERT_ATTRIB_TEX0];

   _mesa_feedback_vertex(ctx, win, color, ci, texcoord);
}


static void
feedback_tri( struct draw_stage *stage, struct prim_header *prim )
{
   struct feedback_stage *fs = feedback_stage(stage);
   struct draw_context *draw = stage->draw;
   FEEDBACK_TOKEN(fs->ctx, (GLfloat) GL_POLYGON_TOKEN);
   FEEDBACK_TOKEN(fs->ctx, (GLfloat) 3); /* three vertices */
   feedback_vertex(fs->ctx, draw, prim->v[0]);
   feedback_vertex(fs->ctx, draw, prim->v[1]);
   feedback_vertex(fs->ctx, draw, prim->v[2]);
}


static void
feedback_line( struct draw_stage *stage, struct prim_header *prim )
{
   struct feedback_stage *fs = feedback_stage(stage);
   struct draw_context *draw = stage->draw;
   if (fs->reset_stipple_counter) {
      FEEDBACK_TOKEN(fs->ctx, (GLfloat) GL_LINE_RESET_TOKEN);
      fs->reset_stipple_counter = GL_FALSE;
   }
   else {
      FEEDBACK_TOKEN(fs->ctx, (GLfloat) GL_LINE_TOKEN);
   }
   feedback_vertex(fs->ctx, draw, prim->v[0]);
   feedback_vertex(fs->ctx, draw, prim->v[1]);
}


static void
feedback_point( struct draw_stage *stage, struct prim_header *prim )
{
   struct feedback_stage *fs = feedback_stage(stage);
   struct draw_context *draw = stage->draw;
   FEEDBACK_TOKEN(fs->ctx, (GLfloat) GL_POINT_TOKEN);
   feedback_vertex(fs->ctx, draw, prim->v[0]);
}


static void
feedback_end( struct draw_stage *stage )
{
   /* no-op */
}


static void
feedback_begin( struct draw_stage *stage )
{
   /* no-op */
}


static void
feedback_reset_stipple_counter( struct draw_stage *stage )
{
   struct feedback_stage *fs = feedback_stage(stage);
   fs->reset_stipple_counter = GL_TRUE;
}


/**
 * Create GL feedback drawing stage.
 */
static struct draw_stage *
draw_glfeedback_stage(GLcontext *ctx, struct draw_context *draw)
{
   struct feedback_stage *fs = CALLOC_STRUCT(feedback_stage);

   fs->stage.draw = draw;
   fs->stage.next = NULL;
   fs->stage.begin = feedback_begin;
   fs->stage.point = feedback_point;
   fs->stage.line = feedback_line;
   fs->stage.tri = feedback_tri;
   fs->stage.end = feedback_end;
   fs->stage.reset_stipple_counter = feedback_reset_stipple_counter;
   fs->ctx = ctx;

   return &fs->stage;
}



/**********************************************************************
 * GL Selection functions
 **********************************************************************/

static void
select_tri( struct draw_stage *stage, struct prim_header *prim )
{
   struct feedback_stage *fs = feedback_stage(stage);
   _mesa_update_hitflag( fs->ctx, prim->v[0]->data[0][2] );
   _mesa_update_hitflag( fs->ctx, prim->v[1]->data[0][2] );
   _mesa_update_hitflag( fs->ctx, prim->v[2]->data[0][2] );
}

static void
select_line( struct draw_stage *stage, struct prim_header *prim )
{
   struct feedback_stage *fs = feedback_stage(stage);
   _mesa_update_hitflag( fs->ctx, prim->v[0]->data[0][2] );
   _mesa_update_hitflag( fs->ctx, prim->v[1]->data[0][2] );
}


static void
select_point( struct draw_stage *stage, struct prim_header *prim )
{
   struct feedback_stage *fs = feedback_stage(stage);
   _mesa_update_hitflag( fs->ctx, prim->v[0]->data[0][2] );
}


static void
select_begin( struct draw_stage *stage )
{
   /* no-op */
}


static void
select_end( struct draw_stage *stage )
{
   /* no-op */
}


static void
select_reset_stipple_counter( struct draw_stage *stage )
{
   /* no-op */
}


/**
 * Create GL selection mode drawing stage.
 */
static struct draw_stage *
draw_glselect_stage(GLcontext *ctx, struct draw_context *draw)
{
   struct feedback_stage *fs = CALLOC_STRUCT(feedback_stage);

   fs->stage.draw = draw;
   fs->stage.next = NULL;
   fs->stage.begin = select_begin;
   fs->stage.point = select_point;
   fs->stage.line = select_line;
   fs->stage.tri = select_tri;
   fs->stage.end = select_end;
   fs->stage.reset_stipple_counter = select_reset_stipple_counter;
   fs->ctx = ctx;

   return &fs->stage;
}


static void
st_RenderMode(GLcontext *ctx, GLenum newMode )
{
   struct st_context *st = ctx->st;
   struct vbo_context *vbo = (struct vbo_context *) ctx->swtnl_im;
   struct draw_context *draw = st->draw;

   if (newMode == GL_RENDER) {
      /* restore normal VBO draw function */
      vbo->draw_prims = st_draw_vbo;
   }
   else if (newMode == GL_SELECT) {
      if (!st->selection_stage)
         st->selection_stage = draw_glselect_stage(ctx, draw);
      draw_set_rasterize_stage(draw, st->selection_stage);
      /* Plug in new vbo draw function */
      vbo->draw_prims = st_feedback_draw_vbo;
   }
   else {
      if (!st->feedback_stage)
         st->feedback_stage = draw_glfeedback_stage(ctx, draw);
      draw_set_rasterize_stage(draw, st->feedback_stage);
      /* Plug in new vbo draw function */
      vbo->draw_prims = st_feedback_draw_vbo;
   }
}



void st_init_feedback_functions(struct dd_function_table *functions)
{
   functions->RenderMode = st_RenderMode;
}