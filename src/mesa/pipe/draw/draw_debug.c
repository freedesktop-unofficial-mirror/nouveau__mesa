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
  */

#include "pipe/p_util.h"
#include "draw_private.h"
#include "draw_context.h"
#include "draw_prim.h"



static void
draw_prim_info(unsigned prim, unsigned *first, unsigned *incr)
{
   assert(prim >= PIPE_PRIM_POINTS);
   assert(prim <= PIPE_PRIM_POLYGON);

   switch (prim) {
   case PIPE_PRIM_POINTS:
      *first = 1;
      *incr = 1;
      break;
   case PIPE_PRIM_LINES:
      *first = 2;
      *incr = 2;
      break;
   case PIPE_PRIM_LINE_STRIP:
      *first = 2;
      *incr = 1;
      break;
   case PIPE_PRIM_LINE_LOOP:
      *first = 2;
      *incr = 1;
      break;
   case PIPE_PRIM_TRIANGLES:
      *first = 3;
      *incr = 3;
      break;
   case PIPE_PRIM_TRIANGLE_STRIP:
      *first = 3;
      *incr = 1;
      break;
   case PIPE_PRIM_TRIANGLE_FAN:
   case PIPE_PRIM_POLYGON:
      *first = 3;
      *incr = 1;
      break;
   case PIPE_PRIM_QUADS:
      *first = 4;
      *incr = 4;
      break;
   case PIPE_PRIM_QUAD_STRIP:
      *first = 4;
      *incr = 2;
      break;
   default:
      assert(0);
      *first = 1;
      *incr = 1;
      break;
   }
}


unsigned 
draw_trim_prim( unsigned mode, unsigned count )
{
   unsigned length, first, incr;

   draw_prim_info( mode, &first, &incr );

   if (count < first)
      length = 0;
   else
      length = count - (count - first) % incr; 

   return length;
}


boolean
draw_validate_prim( unsigned mode, unsigned count )
{
   return (count > 0 &&
           count == draw_trim_prim( mode, count ));
}
