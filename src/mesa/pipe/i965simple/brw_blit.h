#ifndef BRW_BLIT_H
#define BRW_BLIT_H

#include "pipe/p_compiler.h"

struct pipe_buffer_handle;
struct brw_context;

void brw_fill_blit(struct brw_context *intel,
                   unsigned cpp,
                   short dst_pitch,
                   struct pipe_buffer_handle *dst_buffer,
                   unsigned dst_offset,
                   boolean dst_tiled,
                   short x, short y,
                   short w, short h,
                   unsigned color);
void brw_copy_blit(struct brw_context *intel,
                   unsigned cpp,
                   short src_pitch,
                   struct pipe_buffer_handle *src_buffer,
                   unsigned  src_offset,
                   boolean src_tiled,
                   short dst_pitch,
                   struct pipe_buffer_handle *dst_buffer,
                   unsigned  dst_offset,
                   boolean dst_tiled,
                   short src_x, short src_y,
                   short dst_x, short dst_y,
                   short w, short h,
                   unsigned logic_op);
#endif