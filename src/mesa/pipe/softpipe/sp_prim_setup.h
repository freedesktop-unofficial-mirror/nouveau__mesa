/*
 * Mesa 3-D graphics library
 * Version:  6.5
 *
 * Copyright (C) 1999-2005  Brian Paul   All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * BRIAN PAUL BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
 * AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */


#ifndef SP_PRIM_SETUP_H
#define SP_PRIM_SETUP_H


/* Vertices are just an array of floats, with all the attributes
 * packed.  We currently assume a layout like:
 *
 * attr[0][0..3] - window position
 * attr[1..n][0..3] - remaining attributes.
 *
 * Attributes are assumed to be 4 floats wide but are packed so that
 * all the enabled attributes run contiguously.
 */

struct draw_stage;
struct softpipe_context;


extern struct draw_stage *sp_draw_render_stage( struct softpipe_context *softpipe );


/* A special stage to gather the stream of triangles, lines, points
 * together and reconstruct vertex buffers for hardware upload.
 *
 * First attempt, work in progress.
 * 
 * TODO:
 *    - separate out vertex buffer building and primitive emit, ie >1 draw per vb.
 *    - tell vbuf stage how to build hw vertices directly
 *    - pass vbuf stage a buffer pointer for direct emit to agp/vram.
 */
typedef void (*vbuf_draw_func)( struct pipe_context *pipe,
                                unsigned prim,
                                const ushort *elements,
                                unsigned nr_elements,
                                const void *vertex_buffer,
                                unsigned nr_vertices );


extern struct draw_stage *sp_draw_vbuf_stage( struct draw_context *draw_context,
                                              struct pipe_context *pipe,
                                              vbuf_draw_func draw );



/* Test harness
 */
void sp_vbuf_setup_draw( struct pipe_context *pipe,
                         unsigned prim,
                         const ushort *elements,
                         unsigned nr_elements,
                         const void *vertex_buffer,
                         unsigned nr_vertices );



#endif /* SP_PRIM_SETUP_H */