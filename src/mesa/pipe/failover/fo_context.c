/**************************************************************************
 * 
 * Copyright 2003 Tungsten Graphics, Inc., Cedar Park, Texas.
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


#include "pipe/p_defines.h"
#include "pipe/p_winsys.h"
#include "pipe/p_util.h"
#include "pipe/p_context.h"

#include "fo_context.h"
#include "fo_winsys.h"



static void failover_destroy( struct pipe_context *pipe )
{
   struct failover_context *failover = failover_context( pipe );

   free( failover );
}



static boolean failover_draw_elements( struct pipe_context *pipe,
				       struct pipe_buffer_handle *indexBuffer,
				       unsigned indexSize,
				       unsigned prim, unsigned start, unsigned count)
{
   struct failover_context *failover = failover_context( pipe );

   /* If there has been any statechange since last time, try hardware
    * rendering again:
    */
   if (failover->dirty) {
      failover->mode = FO_HW;
   }

   /* Try hardware:
    */
   if (failover->mode == FO_HW) {
      if (!failover->hw->draw_elements( failover->hw, 
					indexBuffer, 
					indexSize, 
					prim, 
					start, 
					count )) {

	 failover->hw->flush( failover->hw, ~0 );
	 failover->mode = FO_SW;
      }
   }

   /* Possibly try software:
    */
   if (failover->mode == FO_SW) {

      if (failover->dirty) 
	 failover_state_emit( failover );

      failover->sw->draw_elements( failover->sw, 
				   indexBuffer, 
				   indexSize, 
				   prim, 
				   start, 
				   count );

      /* Be ready to switch back to hardware rendering without an
       * intervening flush.  Unlikely to be much performance impact to
       * this:
       */
      failover->sw->flush( failover->sw, ~0 );
   }

   return TRUE;
}


static boolean failover_draw_arrays( struct pipe_context *pipe,
				     unsigned prim, unsigned start, unsigned count)
{
   return failover_draw_elements(pipe, NULL, 0, prim, start, count);
}



struct pipe_context *failover_create( struct pipe_context *hw,
				      struct pipe_context *sw )
{
   struct failover_context *failover = CALLOC_STRUCT(failover_context);
   if (failover == NULL)
      return NULL;

   failover->pipe.winsys = hw->winsys;
   failover->pipe.destroy = failover_destroy;
   failover->pipe.supported_formats = hw->supported_formats;
   failover->pipe.max_texture_size = hw->max_texture_size;
   failover->pipe.get_name = hw->get_name;
   failover->pipe.get_vendor = hw->get_vendor;

   failover->pipe.draw_arrays = failover_draw_arrays;
   failover->pipe.draw_elements = failover_draw_elements;
   failover->pipe.clear = hw->clear;

   /* No software occlusion fallback (or other optional functionality)
    * at this point - if the hardware doesn't support it, don't
    * advertise it to the application.
    */
   failover->pipe.reset_occlusion_counter = hw->reset_occlusion_counter;
   failover->pipe.get_occlusion_counter = hw->get_occlusion_counter;

   failover_init_state_functions( failover );

   failover->pipe.surface_alloc = hw->surface_alloc;
   failover->pipe.get_tex_surface = hw->get_tex_surface;

   failover->pipe.region_alloc = hw->region_alloc;
   failover->pipe.region_release = hw->region_release;
   failover->pipe.region_idle = hw->region_idle;
   failover->pipe.region_map = hw->region_map;
   failover->pipe.region_unmap = hw->region_unmap;
   failover->pipe.region_data = hw->region_data;
   failover->pipe.region_copy = hw->region_copy;
   failover->pipe.region_fill = hw->region_fill;
   failover->pipe.mipmap_tree_layout = hw->mipmap_tree_layout;
   failover->pipe.flush = hw->flush;

   failover->dirty = 0;

   return &failover->pipe;
}
