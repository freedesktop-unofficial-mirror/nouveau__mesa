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
  *   Brian Paul
  */

#include "pipe/p_util.h"
#include "draw_private.h"
#include "draw_context.h"
#include "draw_vertex.h"

#include "pipe/tgsi/exec/tgsi_core.h"

static INLINE unsigned
compute_clipmask(float cx, float cy, float cz, float cw)
{
   unsigned mask = 0;

   if (-cx + cw < 0) mask |= CLIP_RIGHT_BIT;
   if ( cx + cw < 0) mask |= CLIP_LEFT_BIT;
   if (-cy + cw < 0) mask |= CLIP_TOP_BIT;
   if ( cy + cw < 0) mask |= CLIP_BOTTOM_BIT;
   if (-cz + cw < 0) mask |= CLIP_FAR_BIT;
   if ( cz + cw < 0) mask |= CLIP_NEAR_BIT;

   return mask;
}




#if !defined(XSTDCALL) 
#if defined(WIN32)
#define XSTDCALL __stdcall
#else
#define XSTDCALL
#endif
#endif

#if defined(USE_X86_ASM) || defined(SLANG_X86)
typedef void (XSTDCALL *sse2_function)(
   const struct tgsi_exec_vector *input,
   struct tgsi_exec_vector *output,
   float (*constant)[4],
   struct tgsi_exec_vector *temporary );
#endif

/**
 * Transform vertices with the current vertex program/shader
 * Up to four vertices can be shaded at a time.
 * \param vbuffer  the input vertex data
 * \param elts  indexes of four input vertices
 * \param count  number of vertices to shade [1..4]
 * \param vOut  array of pointers to four output vertices
 */
static void
run_vertex_program(struct draw_context *draw,
                   unsigned elts[4], unsigned count,
                   struct vertex_header *vOut[])
{
   struct tgsi_exec_machine machine;
   unsigned int j;

   ALIGN16_DECL(struct tgsi_exec_vector, inputs, PIPE_ATTRIB_MAX);
   ALIGN16_DECL(struct tgsi_exec_vector, outputs, PIPE_ATTRIB_MAX);
   const float *scale = draw->viewport.scale;
   const float *trans = draw->viewport.translate;

   assert(count <= 4);
   assert(draw->vertex_shader.outputs_written & (1 << TGSI_ATTRIB_POS));

#ifdef DEBUG
   memset( &machine, 0, sizeof( machine ) );
#endif

   /* init machine state */
   tgsi_exec_machine_init(&machine,
                          draw->vertex_shader.tokens,
                          PIPE_MAX_SAMPLERS,
                          NULL /*samplers*/ );

   /* Consts does not require 16 byte alignment. */
   machine.Consts = (float (*)[4]) draw->mapped_constants;

   machine.Inputs = ALIGN16_ASSIGN(inputs);
   machine.Outputs = ALIGN16_ASSIGN(outputs);

   draw_vertex_fetch( draw, &machine, elts, count );


   /* run shader */
   if( draw->vertex_shader.executable != NULL ) {
#if defined(USE_X86_ASM) || defined(SLANG_X86)
      sse2_function func = (sse2_function) draw->vertex_shader.executable;
      func(
         machine.Inputs,
         machine.Outputs,
         machine.Consts,
         machine.Temps );
#else
      assert( 0 );
#endif
   }
   else {
      tgsi_exec_machine_run( &machine );
   }


   /* store machine results */
   for (j = 0; j < count; j++) {
      unsigned slot;
      float x, y, z, w;

      /* Handle attr[0] (position) specially: */
      x = vOut[j]->clip[0] = machine.Outputs[0].xyzw[0].f[j];
      y = vOut[j]->clip[1] = machine.Outputs[0].xyzw[1].f[j];
      z = vOut[j]->clip[2] = machine.Outputs[0].xyzw[2].f[j];
      w = vOut[j]->clip[3] = machine.Outputs[0].xyzw[3].f[j];

      vOut[j]->clipmask = compute_clipmask(x, y, z, w) | draw->user_clipmask;
      vOut[j]->edgeflag = 1;

      /* divide by w */
      w = 1.0f / w;
      x *= w;
      y *= w;
      z *= w;

      /* Viewport mapping */
      vOut[j]->data[0][0] = x * scale[0] + trans[0];
      vOut[j]->data[0][1] = y * scale[1] + trans[1];
      vOut[j]->data[0][2] = z * scale[2] + trans[2];
      vOut[j]->data[0][3] = w;

      /* remaining attributes are packed into sequential post-transform
       * vertex attrib slots.
       */
      for (slot = 1; slot < draw->vertex_info.num_attribs; slot++) {
         vOut[j]->data[slot][0] = machine.Outputs[slot].xyzw[0].f[j];
         vOut[j]->data[slot][1] = machine.Outputs[slot].xyzw[1].f[j];
         vOut[j]->data[slot][2] = machine.Outputs[slot].xyzw[2].f[j];
         vOut[j]->data[slot][3] = machine.Outputs[slot].xyzw[3].f[j];
      }
   } /* loop over vertices */
}


/**
 * Called by the draw module when the vertx cache needs to be flushed.
 * This involves running the vertex shader.
 */
void draw_vertex_shader_queue_flush( struct draw_context *draw )
{
   unsigned i, j;

   /* run vertex shader on vertex cache entries, four per invokation */
   for (i = 0; i < draw->vs.queue_nr; i += 4) {
      struct vertex_header *dests[4];
      unsigned elts[4];
      int n;

      for (j = 0; j < 4; j++) {
         elts[j] = draw->vs.queue[i + j].elt;
         dests[j] = draw->vs.queue[i + j].dest;
      }

      n = MIN2(4, draw->vs.queue_nr - i);
      assert(n > 0);
      assert(n <= 4);

      run_vertex_program(draw, elts, n, dests);
   }

   draw->vs.queue_nr = 0;
}
