/**************************************************************************
 * 
 * Copyright 2007 Tungsten Graphics, Inc., Bismarck, ND., USA
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
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. IN NO EVENT SHALL
 * THE COPYRIGHT HOLDERS, AUTHORS AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR 
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE 
 * USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 * 
 * 
 **************************************************************************/

/*
 * Authors:
 *   Keith Whitwell
 *   Brian Paul
 */


#include "glxheader.h"
#include "xmesaP.h"

#undef ASSERT

#include "pipe/p_winsys.h"
#include "pipe/p_format.h"
#include "pipe/p_context.h"
#include "pipe/p_util.h"
#include "pipe/p_inlines.h"
#include "softpipe/sp_winsys.h"

#ifdef GALLIUM_CELL
#include "cell/ppu/cell_context.h"
#include "cell/ppu/cell_screen.h"
#include "cell/ppu/cell_winsys.h"
#else
#define TILE_SIZE 32  /* avoid compilation errors */
#endif

#include "xm_winsys_aub.h"


/**
 * Low-level OS/window system memory buffer
 */
struct xm_buffer
{
   struct pipe_buffer base;
   boolean userBuffer;  /** Is this a user-space buffer? */
   void *data;
   void *mapped;
};


struct xmesa_surface
{
   struct pipe_surface surface;

   int tileSize;
};


/**
 * Derived from softpipe_winsys.
 * We just need one extra field which indicates the pixel format to use for
 * drawing surfaces so that we're compatible with the XVisual/window format.
 */
struct xmesa_softpipe_winsys
{
   struct softpipe_winsys spws;
   enum pipe_format pixelformat;
};



/** Cast wrapper */
static INLINE struct xmesa_surface *
xmesa_surface(struct pipe_surface *ps)
{
//   assert(0);
   return (struct xmesa_surface *) ps;
}

/** cast wrapper */
static INLINE struct xmesa_softpipe_winsys *
xmesa_softpipe_winsys(struct softpipe_winsys *spws)
{
   return (struct xmesa_softpipe_winsys *) spws;
}

/**
 * Turn the softpipe opaque buffer pointer into a dri_bufmgr opaque
 * buffer pointer...
 */
static INLINE struct xm_buffer *
xm_buffer( struct pipe_buffer *buf )
{
   return (struct xm_buffer *)buf;
}



/* Most callbacks map direcly onto dri_bufmgr operations:
 */
static void *
xm_buffer_map(struct pipe_winsys *pws, struct pipe_buffer *buf,
              unsigned flags)
{
   struct xm_buffer *xm_buf = xm_buffer(buf);
   xm_buf->mapped = xm_buf->data;
   return xm_buf->mapped;
}

static void
xm_buffer_unmap(struct pipe_winsys *pws, struct pipe_buffer *buf)
{
   struct xm_buffer *xm_buf = xm_buffer(buf);
   xm_buf->mapped = NULL;
}

static void
xm_buffer_destroy(struct pipe_winsys *pws,
		  struct pipe_buffer *buf)
{
   struct xm_buffer *oldBuf = xm_buffer(buf);

   if (oldBuf->data) {
      if (!oldBuf->userBuffer)
	 align_free(oldBuf->data);
      oldBuf->data = NULL;
   }

   free(oldBuf);
}


/**
 * Display a surface that's in a tiled configuration.  That is, all the
 * pixels for a TILE_SIZExTILE_SIZE block are contiguous in memory.
 */
static void
xmesa_display_surface_tiled(XMesaBuffer b, const struct pipe_surface *surf)
{
   XImage *ximage = b->tempImage;
   struct xm_buffer *xm_buf = xm_buffer(surf->buffer);
   const uint tilesPerRow = (surf->width + TILE_SIZE - 1) / TILE_SIZE;
   uint x, y;

   /* check that the XImage has been previously initialized */
   assert(ximage->format);
   assert(ximage->bitmap_unit);

   /* update XImage's fields */
   ximage->width = TILE_SIZE;
   ximage->height = TILE_SIZE;
   ximage->bytes_per_line = TILE_SIZE * 4;

   for (y = 0; y < surf->height; y += TILE_SIZE) {
      for (x = 0; x < surf->width; x += TILE_SIZE) {
         int dx = x;
         int dy = y;
         int tx = x / TILE_SIZE;
         int ty = y / TILE_SIZE;
         int offset = ty * tilesPerRow + tx;

         offset *= 4 * TILE_SIZE * TILE_SIZE;

         ximage->data = (char *) xm_buf->data + offset;

         XPutImage(b->xm_visual->display, b->drawable, b->gc,
                   ximage, 0, 0, dx, dy, TILE_SIZE, TILE_SIZE);
      }
   }
}


/**
 * Display/copy the image in the surface into the X window specified
 * by the XMesaBuffer.
 */
void
xmesa_display_surface(XMesaBuffer b, const struct pipe_surface *surf)
{
   XImage *ximage = b->tempImage;
   struct xm_buffer *xm_buf = xm_buffer(surf->buffer);
   const struct xmesa_surface *xm_surf
      = xmesa_surface((struct pipe_surface *) surf);

   if (xm_surf->tileSize) {
      xmesa_display_surface_tiled(b, surf);
      return;
   }

   /* check that the XImage has been previously initialized */
   assert(ximage->format);
   assert(ximage->bitmap_unit);

   /* update XImage's fields */
   ximage->width = surf->width;
   ximage->height = surf->height;
   ximage->bytes_per_line = surf->pitch * (ximage->bits_per_pixel / 8);
   ximage->data = xm_buf->data;

   /* display image in Window */
   XPutImage(b->xm_visual->display, b->drawable, b->gc,
             ximage, 0, 0, 0, 0, surf->width, surf->height);
}


static void
xm_flush_frontbuffer(struct pipe_winsys *pws,
                     struct pipe_surface *surf,
                     void *context_private)
{
   /* The Xlib driver's front color surfaces are actually X Windows so
    * this flush is a no-op.
    * If we instead did front buffer rendering to a temporary XImage,
    * this would be the place to copy the Ximage to the on-screen Window.
    */
   XMesaContext xmctx = (XMesaContext) context_private;
   xmesa_display_surface(xmctx->xm_buffer, surf);
}



static void
xm_printf(struct pipe_winsys *pws, const char *fmtString, ...)
{
   va_list args;
   va_start( args, fmtString );  
   vfprintf(stderr, fmtString, args);
   va_end( args );
}


static const char *
xm_get_name(struct pipe_winsys *pws)
{
   return "Xlib";
}


static struct pipe_buffer *
xm_buffer_create(struct pipe_winsys *pws, 
                 unsigned alignment, 
                 unsigned usage,
                 unsigned size)
{
   struct xm_buffer *buffer = CALLOC_STRUCT(xm_buffer);
   buffer->base.refcount = 1;
   buffer->base.alignment = alignment;
   buffer->base.usage = usage;
   buffer->base.size = size;

   /* align to 16-byte multiple for Cell */
   buffer->data = align_malloc(size, max(alignment, 16));

   return &buffer->base;
}


/**
 * Create buffer which wraps user-space data.
 */
static struct pipe_buffer *
xm_user_buffer_create(struct pipe_winsys *pws, void *ptr, unsigned bytes)
{
   struct xm_buffer *buffer = CALLOC_STRUCT(xm_buffer);
   buffer->base.refcount = 1;
   buffer->base.size = bytes;
   buffer->userBuffer = TRUE;
   buffer->data = ptr;

   return &buffer->base;
}



/**
 * Round n up to next multiple.
 */
static INLINE unsigned
round_up(unsigned n, unsigned multiple)
{
   return (n + multiple - 1) & ~(multiple - 1);
}

static int
xm_surface_alloc_storage(struct pipe_winsys *winsys,
                         struct pipe_surface *surf,
                         unsigned width, unsigned height,
                         enum pipe_format format, 
                         unsigned flags)
{
   const unsigned alignment = 64;

   surf->width = width;
   surf->height = height;
   surf->format = format;
   surf->cpp = pf_get_size(format);
   surf->pitch = round_up(width, alignment / surf->cpp);

#ifdef GALLIUM_CELL /* XXX a bit of a hack */
   height = round_up(height, TILE_SIZE);
#endif

   assert(!surf->buffer);
   surf->buffer = winsys->buffer_create(winsys, alignment,
                                        PIPE_BUFFER_USAGE_PIXEL,
                                        surf->pitch * surf->cpp * height);
   if(!surf->buffer)
      return -1;
   
   return 0;
}


/**
 * Called via pipe->surface_alloc() to create new surfaces (textures,
 * renderbuffers, etc.
 */
static struct pipe_surface *
xm_surface_alloc(struct pipe_winsys *ws)
{
   struct xmesa_surface *xms = CALLOC_STRUCT(xmesa_surface);

   assert(ws);

   xms->surface.refcount = 1;
   xms->surface.winsys = ws;

#ifdef GALLIUM_CELL
   if (!getenv("GALLIUM_NOCELL")) {
      xms->tileSize = 32; /** probably temporary */
   }
#endif

   return &xms->surface;
}



static void
xm_surface_release(struct pipe_winsys *winsys, struct pipe_surface **s)
{
   struct pipe_surface *surf = *s;
   surf->refcount--;
   if (surf->refcount == 0) {
      if (surf->buffer)
	pipe_buffer_reference(winsys, &surf->buffer, NULL);
      free(surf);
   }
   *s = NULL;
}



/**
 * Return pointer to a pipe_winsys object.
 * For Xlib, this is a singleton object.
 * Nothing special for the Xlib driver so no subclassing or anything.
 */
struct pipe_winsys *
xmesa_get_pipe_winsys_aub(void)
{
   static struct pipe_winsys *ws = NULL;

   if (!ws && getenv("XM_AUB")) {
      ws = xmesa_create_pipe_winsys_aub();
   }
   else if (!ws) {
      ws = CALLOC_STRUCT(pipe_winsys);
   
      /* Fill in this struct with callbacks that pipe will need to
       * communicate with the window system, buffer manager, etc. 
       */
      ws->buffer_create = xm_buffer_create;
      ws->user_buffer_create = xm_user_buffer_create;
      ws->buffer_map = xm_buffer_map;
      ws->buffer_unmap = xm_buffer_unmap;
      ws->buffer_destroy = xm_buffer_destroy;

      ws->surface_alloc = xm_surface_alloc;
      ws->surface_alloc_storage = xm_surface_alloc_storage;
      ws->surface_release = xm_surface_release;

      ws->flush_frontbuffer = xm_flush_frontbuffer;
      ws->printf = xm_printf;
      ws->get_name = xm_get_name;
   }

   return ws;
}


/**
 * Called via softpipe_winsys->is_format_supported().
 * This function is only called to test formats for front/back color surfaces.
 * The winsys being queried will have been created at glXCreateContext
 * time, with a pixel format corresponding to the context's visual.
 */
static boolean
xmesa_is_format_supported(struct softpipe_winsys *sws,
                          enum pipe_format format)
{
   struct xmesa_softpipe_winsys *xmws = xmesa_softpipe_winsys(sws);
   return (format == xmws->pixelformat);
}


/**
 * Return pointer to a softpipe_winsys object.
 */
static struct softpipe_winsys *
xmesa_get_softpipe_winsys(uint pixelformat)
{
   struct xmesa_softpipe_winsys *xmws
      = CALLOC_STRUCT(xmesa_softpipe_winsys);
   if (!xmws)
      return NULL;

   xmws->spws.is_format_supported = xmesa_is_format_supported;
   xmws->pixelformat = pixelformat;

   return &xmws->spws;
}


struct pipe_context *
xmesa_create_pipe_context(XMesaContext xmesa, uint pixelformat)
{
   struct pipe_winsys *pws = xmesa_get_pipe_winsys_aub();
   struct pipe_context *pipe;
   
#ifdef GALLIUM_CELL
   if (!getenv("GALLIUM_NOCELL")) {
      struct cell_winsys *cws = cell_get_winsys(pixelformat);
      struct pipe_screen *screen = cell_create_screen(pws);
      pipe = cell_create_context(screen, cws);
      if (pipe)
         pipe->priv = xmesa;
      return pipe;
   }
   else
#endif
   {
      struct softpipe_winsys *spws = xmesa_get_softpipe_winsys(pixelformat);
      struct pipe_screen *screen = softpipe_create_screen(pws);
      pipe = softpipe_create( screen, pws, spws );
      if (pipe)
         pipe->priv = xmesa;

      return pipe;
   }
}