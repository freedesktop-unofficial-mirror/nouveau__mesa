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

#ifndef CELL_TEXTURE_H
#define CELL_TEXTURE_H


struct cell_context;
struct pipe_texture;


/**
 * Subclass of pipe_texture
 */
struct cell_texture
{
   struct pipe_texture base;

   unsigned long level_offset[CELL_MAX_TEXTURE_LEVELS];
   unsigned long stride[CELL_MAX_TEXTURE_LEVELS];

   /* The data is held here:
    */
   struct pipe_buffer *buffer;
   unsigned long buffer_size;

   /** Texture data in tiled layout is held here */
   struct pipe_buffer *tiled_buffer[CELL_MAX_TEXTURE_LEVELS];
   /** Mapped, tiled texture data */
   void *tiled_mapped[CELL_MAX_TEXTURE_LEVELS];
   void *untiled_data[CELL_MAX_TEXTURE_LEVELS];
};


/** cast wrapper */
static INLINE struct cell_texture *
cell_texture(struct pipe_texture *pt)
{
   return (struct cell_texture *) pt;
}



extern void
cell_init_screen_texture_funcs(struct pipe_screen *screen);


#endif /* CELL_TEXTURE_H */
