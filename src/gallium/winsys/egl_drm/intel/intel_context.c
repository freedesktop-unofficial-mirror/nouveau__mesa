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


#include "intel_screen.h"
#include "intel_context.h"
#include "intel_swapbuffers.h"
#include "intel_winsys.h"
#include "intel_batchbuffer.h"

#include "state_tracker/st_public.h"
#include "pipe/p_defines.h"
#include "pipe/p_context.h"
#include "intel_egl.h"
#include "utils.h"

#ifdef DEBUG
int __intel_debug = 0;
#endif


#define need_GL_ARB_multisample
#define need_GL_ARB_point_parameters
#define need_GL_ARB_texture_compression
#define need_GL_ARB_vertex_buffer_object
#define need_GL_ARB_vertex_program
#define need_GL_ARB_window_pos
#define need_GL_EXT_blend_color
#define need_GL_EXT_blend_equation_separate
#define need_GL_EXT_blend_func_separate
#define need_GL_EXT_blend_minmax
#define need_GL_EXT_cull_vertex
#define need_GL_EXT_fog_coord
#define need_GL_EXT_framebuffer_object
#define need_GL_EXT_multi_draw_arrays
#define need_GL_EXT_secondary_color
#define need_GL_NV_vertex_program
#include "extension_helper.h"


/**
 * Extension strings exported by the intel driver.
 *
 * \note
 * It appears that ARB_texture_env_crossbar has "disappeared" compared to the
 * old i830-specific driver.
 */
const struct dri_extension card_extensions[] = {
   {"GL_ARB_multisample", GL_ARB_multisample_functions},
   {"GL_ARB_multitexture", NULL},
   {"GL_ARB_point_parameters", GL_ARB_point_parameters_functions},
   {"GL_ARB_texture_border_clamp", NULL},
   {"GL_ARB_texture_compression", GL_ARB_texture_compression_functions},
   {"GL_ARB_texture_cube_map", NULL},
   {"GL_ARB_texture_env_add", NULL},
   {"GL_ARB_texture_env_combine", NULL},
   {"GL_ARB_texture_env_dot3", NULL},
   {"GL_ARB_texture_mirrored_repeat", NULL},
   {"GL_ARB_texture_rectangle", NULL},
   {"GL_ARB_vertex_buffer_object", GL_ARB_vertex_buffer_object_functions},
   {"GL_ARB_pixel_buffer_object", NULL},
   {"GL_ARB_vertex_program", GL_ARB_vertex_program_functions},
   {"GL_ARB_window_pos", GL_ARB_window_pos_functions},
   {"GL_EXT_blend_color", GL_EXT_blend_color_functions},
   {"GL_EXT_blend_equation_separate", GL_EXT_blend_equation_separate_functions},
   {"GL_EXT_blend_func_separate", GL_EXT_blend_func_separate_functions},
   {"GL_EXT_blend_minmax", GL_EXT_blend_minmax_functions},
   {"GL_EXT_blend_subtract", NULL},
   {"GL_EXT_cull_vertex", GL_EXT_cull_vertex_functions},
   {"GL_EXT_fog_coord", GL_EXT_fog_coord_functions},
   {"GL_EXT_framebuffer_object", GL_EXT_framebuffer_object_functions},
   {"GL_EXT_multi_draw_arrays", GL_EXT_multi_draw_arrays_functions},
   {"GL_EXT_packed_depth_stencil", NULL},
   {"GL_EXT_pixel_buffer_object", NULL},
   {"GL_EXT_secondary_color", GL_EXT_secondary_color_functions},
   {"GL_EXT_stencil_wrap", NULL},
   {"GL_EXT_texture_edge_clamp", NULL},
   {"GL_EXT_texture_env_combine", NULL},
   {"GL_EXT_texture_env_dot3", NULL},
   {"GL_EXT_texture_filter_anisotropic", NULL},
   {"GL_EXT_texture_lod_bias", NULL},
   {"GL_3DFX_texture_compression_FXT1", NULL},
   {"GL_APPLE_client_storage", NULL},
   {"GL_MESA_pack_invert", NULL},
   {"GL_MESA_ycbcr_texture", NULL},
   {"GL_NV_blend_square", NULL},
   {"GL_NV_vertex_program", GL_NV_vertex_program_functions},
   {"GL_NV_vertex_program1_1", NULL},
   {"GL_SGIS_generate_mipmap", NULL },
   {NULL, NULL}
};

int
intel_create_context(struct egl_drm_context *eglCon, const __GLcontextModes *visual, void *sharedContextPrivate)
{
	struct intel_context *iCon = CALLOC_STRUCT(intel_context);
	struct intel_screen *iScrn = (struct intel_screen *)eglCon->device->priv;
	struct pipe_context *pipe;
	struct st_context *st_share = NULL;

	eglCon->priv = iCon;

	iCon->intel_screen = iScrn;
	iCon->egl_context = eglCon;
	iCon->egl_device = eglCon->device;

	iCon->batch = intel_batchbuffer_alloc(iScrn);
	iCon->last_swap_fence = NULL;
	iCon->first_swap_fence = NULL;

	pipe = intel_create_i915simple(iCon, iScrn->winsys);
//	pipe = intel_create_softpipe(iCon, iScrn->winsys);

	pipe->priv = iCon;

	iCon->st = st_create_context(pipe, visual, st_share);
	return TRUE;
}

void
intel_make_current(struct egl_drm_context *context, struct egl_drm_drawable *draw, struct egl_drm_drawable *read)
{
	if (context) {
		struct intel_context *intel = (struct intel_context *)context->priv;
		struct intel_framebuffer *draw_fb = (struct intel_framebuffer *)draw->priv;
		struct intel_framebuffer *read_fb = (struct intel_framebuffer *)read->priv;

		assert(draw_fb->stfb);
		assert(read_fb->stfb);

		st_make_current(intel->st, draw_fb->stfb, read_fb->stfb);

		intel->egl_drawable = draw;

		st_resize_framebuffer(draw_fb->stfb, draw->w, draw->h);

		if (draw != read)
			st_resize_framebuffer(read_fb->stfb, read->w, read->h);

		//intelUpdateWindowSize(driDrawPriv);
	} else {
		st_make_current(NULL, NULL, NULL);
	}
}

void
intel_bind_frontbuffer(struct egl_drm_drawable *draw, struct egl_drm_frontbuffer *front)
{
	struct intel_screen *intelScreen = (struct intel_screen *)draw->device->priv;
	struct intel_framebuffer *draw_fb = (struct intel_framebuffer *)draw->priv;

	driBOUnReference(draw_fb->front_buffer);
	draw_fb->front_buffer = NULL;
	draw_fb->front = NULL;

	/* to unbind just call this function with front == NULL */
	if (!front)
		return;

	draw_fb->front = front;

	driGenBuffers(intelScreen->staticPool, "front", 1, &draw_fb->front_buffer, 0, 0, 0);
	driBOSetReferenced(draw_fb->front_buffer, front->handle);

	st_resize_framebuffer(draw_fb->stfb, draw->w, draw->h);
}

#if 0
GLboolean
intelCreateContext(const __GLcontextModes * visual,
                   __DRIcontextPrivate * driContextPriv,
                   void *sharedContextPrivate)
{
   struct intel_context *intel = CALLOC_STRUCT(intel_context);
   __DRIscreenPrivate *sPriv = driContextPriv->driScreenPriv;
   struct intel_screen *intelScreen = intel_screen(sPriv);
   drmI830Sarea *saPriv = intelScreen->sarea;
   int fthrottle_mode;
   GLboolean havePools;
   struct pipe_context *pipe;
   struct st_context *st_share = NULL;

   if (sharedContextPrivate) {
      st_share = ((struct intel_context *) sharedContextPrivate)->st;
   }

   driContextPriv->driverPrivate = intel;
   intel->intelScreen = intelScreen;
   intel->driScreen = sPriv;
   intel->sarea = saPriv;

   driParseConfigFiles(&intel->optionCache, &intelScreen->optionCache,
                       intel->driScreen->myNum, "i915");


   /*
    * memory pools
    */
   DRM_LIGHT_LOCK(sPriv->fd, &sPriv->pSAREA->lock, driContextPriv->hHWContext);
   // ZZZ JB should be per screen and not be done per context
   havePools = intelCreatePools(sPriv);
   DRM_UNLOCK(sPriv->fd, &sPriv->pSAREA->lock, driContextPriv->hHWContext);
   if (!havePools)
      return GL_FALSE;


   /* Dri stuff */
   intel->hHWContext = driContextPriv->hHWContext;
   intel->driFd = sPriv->fd;
   intel->driHwLock = (drmLock *) & sPriv->pSAREA->lock;

   fthrottle_mode = driQueryOptioni(&intel->optionCache, "fthrottle_mode");
   intel->iw.irq_seq = -1;
   intel->irqsEmitted = 0;

   intel->batch = intel_batchbuffer_alloc(intel);
   intel->last_swap_fence = NULL;
   intel->first_swap_fence = NULL;

#ifdef DEBUG
   __intel_debug = driParseDebugString(getenv("INTEL_DEBUG"), debug_control);
#endif

   /*
    * Pipe-related setup
    */
   if (getenv("INTEL_SP")) {
      /* use softpipe driver instead of hw */
      pipe = intel_create_softpipe( intel, intelScreen->winsys );
   }
   else {
      switch (intel->intelScreen->deviceID) {
      case PCI_CHIP_I945_G:
      case PCI_CHIP_I945_GM:
      case PCI_CHIP_I945_GME:
      case PCI_CHIP_G33_G:
      case PCI_CHIP_Q33_G:
      case PCI_CHIP_Q35_G:
      case PCI_CHIP_I915_G:
      case PCI_CHIP_I915_GM:
	 pipe = intel_create_i915simple( intel, intelScreen->winsys );
	 break;
      default:
	 fprintf(stderr, "Unknown PCIID %x in %s, using software driver\n", 
                 intel->intelScreen->deviceID, __FUNCTION__);

	 pipe = intel_create_softpipe( intel, intelScreen->winsys );
	 break;
      }
   }

   pipe->priv = intel;

   intel->st = st_create_context(pipe, visual, st_share);

   return GL_TRUE;
}


void
intelDestroyContext(__DRIcontextPrivate * driContextPriv)
{
   struct intel_context *intel = intel_context(driContextPriv);

   assert(intel);               /* should never be null */
   if (intel) {
      st_finish(intel->st);

      intel_batchbuffer_free(intel->batch);

      if (intel->last_swap_fence) {
	 driFenceFinish(intel->last_swap_fence, DRM_FENCE_TYPE_EXE, GL_TRUE);
	 driFenceUnReference(&intel->last_swap_fence);
	 intel->last_swap_fence = NULL;
      }
      if (intel->first_swap_fence) {
	 driFenceFinish(intel->first_swap_fence, DRM_FENCE_TYPE_EXE, GL_TRUE);
	 driFenceUnReference(&intel->first_swap_fence);
	 intel->first_swap_fence = NULL;
      }

      if (intel->intelScreen->dummyContext == intel)
         intel->intelScreen->dummyContext = NULL;

      st_destroy_context(intel->st);
      free(intel);
   }
}


GLboolean
intelUnbindContext(__DRIcontextPrivate * driContextPriv)
{
   struct intel_context *intel = intel_context(driContextPriv);
   st_flush(intel->st, PIPE_FLUSH_RENDER_CACHE, NULL);
   /* XXX make_current(NULL)? */
   return GL_TRUE;
}


GLboolean
intelMakeCurrent(__DRIcontextPrivate * driContextPriv,
                 __DRIdrawablePrivate * driDrawPriv,
                 __DRIdrawablePrivate * driReadPriv)
{
   if (driContextPriv) {
      struct intel_context *intel = intel_context(driContextPriv);
      struct intel_framebuffer *draw_fb = intel_framebuffer(driDrawPriv);
      struct intel_framebuffer *read_fb = intel_framebuffer(driReadPriv);

      assert(draw_fb->stfb);
      assert(read_fb->stfb);

      /* This is for situations in which we need a rendering context but
       * there may not be any currently bound.
       */
      intel->intelScreen->dummyContext = intel;

      st_make_current(intel->st, draw_fb->stfb, read_fb->stfb);

      if ((intel->driDrawable != driDrawPriv) ||
	  (intel->lastStamp != driDrawPriv->lastStamp)) {
         intel->driDrawable = driDrawPriv;
         intelUpdateWindowSize(driDrawPriv);
         intel->lastStamp = driDrawPriv->lastStamp;
      }

      /* The size of the draw buffer will have been updated above.
       * If the readbuffer is a different window, check/update its size now.
       */
      if (driReadPriv != driDrawPriv) {
         intelUpdateWindowSize(driReadPriv);
      }

   }
   else {
      st_make_current(NULL, NULL, NULL);
   }

   return GL_TRUE;
}
#endif