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
#include "main/macros.h"

#include "pipe/p_winsys.h"
#include "pipe/p_format.h"
#include "pipe/p_context.h"
#include "pipe/softpipe/sp_winsys.h"


/**
 * Low-level OS/window system memory buffer
 */
struct xm_buffer
{
   boolean userBuffer;  /** Is this a user-space buffer? */
   int refcount;
   unsigned size;
   void *data;
   void *mapped;
};


struct xmesa_surface
{
   struct pipe_surface surface;
   /* no extra fields for now */
};


/**
 * Derived from softpipe_winsys.
 * We just need one extra field which indicates the pixel format to use for
 * drawing surfaces so that we're compatible with the XVisual/window format.
 */
struct xmesa_softpipe_winsys
{
   struct softpipe_winsys spws;
   uint pixelformat;
};



/** Cast wrapper */
static INLINE struct xmesa_surface *
xmesa_surface(struct pipe_surface *ps)
{
   assert(0);
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
xm_bo( struct pipe_buffer_handle *bo )
{
   return (struct xm_buffer *) bo;
}

static INLINE struct pipe_buffer_handle *
pipe_bo( struct xm_buffer *bo )
{
   return (struct pipe_buffer_handle *) bo;
}


/* Most callbacks map direcly onto dri_bufmgr operations:
 */
static void *
xm_buffer_map(struct pipe_winsys *pws, struct pipe_buffer_handle *buf,
              unsigned flags)
{
   struct xm_buffer *xm_buf = xm_bo(buf);
   xm_buf->mapped = xm_buf->data;
   return xm_buf->mapped;
}

static void
xm_buffer_unmap(struct pipe_winsys *pws, struct pipe_buffer_handle *buf)
{
   struct xm_buffer *xm_buf = xm_bo(buf);
   xm_buf->mapped = NULL;
}

static void
xm_buffer_reference(struct pipe_winsys *pws,
                    struct pipe_buffer_handle **ptr,
                    struct pipe_buffer_handle *buf)
{
   if (*ptr) {
      struct xm_buffer *oldBuf = xm_bo(*ptr);
      oldBuf->refcount--;
      assert(oldBuf->refcount >= 0);
      if (oldBuf->refcount == 0) {
         if (oldBuf->data) {
            if (!oldBuf->userBuffer)
               free(oldBuf->data);
            oldBuf->data = NULL;
         }
         free(oldBuf);
      }
      *ptr = NULL;
   }

   assert(!(*ptr));

   if (buf) {
      struct xm_buffer *newBuf = xm_bo(buf);
      newBuf->refcount++;
      *ptr = buf;
   }
}

static void
xm_buffer_data(struct pipe_winsys *pws, struct pipe_buffer_handle *buf,
               unsigned size, const void *data, unsigned usage )
{
   struct xm_buffer *xm_buf = xm_bo(buf);
   assert(!xm_buf->userBuffer);
   if (xm_buf->size != size) {
      if (xm_buf->data)
         free(xm_buf->data);
      xm_buf->data = malloc(size);
      xm_buf->size = size;
   }
   if (data)
      memcpy(xm_buf->data, data, size);
}

static void
xm_buffer_subdata(struct pipe_winsys *pws, struct pipe_buffer_handle *buf,
                  unsigned long offset, unsigned long size, const void *data)
{
   struct xm_buffer *xm_buf = xm_bo(buf);
   GLubyte *b = (GLubyte *) xm_buf->data;
   assert(!xm_buf->userBuffer);
   assert(b);
   memcpy(b + offset, data, size);
}

static void
xm_buffer_get_subdata(struct pipe_winsys *pws, struct pipe_buffer_handle *buf,
                      unsigned long offset, unsigned long size, void *data)
{
   const struct xm_buffer *xm_buf = xm_bo(buf);
   const GLubyte *b = (GLubyte *) xm_buf->data;
   assert(!xm_buf->userBuffer);
   assert(b);
   memcpy(data, b + offset, size);
}


/**
 * Display/copy the image in the surface into the X window specified
 * by the XMesaBuffer.
 */
void
xmesa_display_surface(XMesaBuffer b, const struct pipe_surface *surf)
{
   XImage *ximage = b->tempImage;
   struct xm_buffer *xm_buf = xm_bo(surf->buffer);

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


static struct pipe_buffer_handle *
xm_buffer_create(struct pipe_winsys *pws, unsigned flags)
{
   struct xm_buffer *buffer = CALLOC_STRUCT(xm_buffer);
   buffer->refcount = 1;
   return pipe_bo(buffer);
}


/**
 * Create buffer which wraps user-space data.
 */
static struct pipe_buffer_handle *
xm_user_buffer_create(struct pipe_winsys *pws, void *ptr, unsigned bytes)
{
   struct xm_buffer *buffer = CALLOC_STRUCT(xm_buffer);
   buffer->userBuffer = TRUE;
   buffer->refcount = 1;
   buffer->data = ptr;
   buffer->size = bytes;
   return pipe_bo(buffer);
}



/**
 * Round n up to next multiple.
 */
static INLINE unsigned
round_up(unsigned n, unsigned multiple)
{
   return (n + multiple - 1) & ~(multiple - 1);
}

static unsigned
xm_surface_pitch(struct pipe_winsys *winsys, unsigned cpp, unsigned width,
		 unsigned flags)
{
   return round_up(width, 64 / cpp);
}


/**
 * Called via pipe->surface_alloc() to create new surfaces (textures,
 * renderbuffers, etc.
 */
static struct pipe_surface *
xm_surface_alloc(struct pipe_winsys *ws, GLuint pipeFormat)
{
   struct xmesa_surface *xms = CALLOC_STRUCT(xmesa_surface);

   assert(ws);
   assert(pipeFormat);

   xms->surface.format = pipeFormat;
   xms->surface.refcount = 1;
   xms->surface.winsys = ws;

   return &xms->surface;
}



static void
xm_surface_release(struct pipe_winsys *winsys, struct pipe_surface **s)
{
   struct pipe_surface *surf = *s;
   surf->refcount--;
   if (surf->refcount == 0) {
      if (surf->buffer)
	winsys->buffer_reference(winsys, &surf->buffer, NULL);
      free(surf);
   }
   *s = NULL;
}



/**
 * Return pointer to a pipe_winsys object.
 * For Xlib, this is a singleton object.
 * Nothing special for the Xlib driver so no subclassing or anything.
 */
static struct pipe_winsys *
xmesa_get_pipe_winsys(void)
{
   static struct pipe_winsys *ws = NULL;

   if (!ws) {
      ws = CALLOC_STRUCT(pipe_winsys);
   
      /* Fill in this struct with callbacks that pipe will need to
       * communicate with the window system, buffer manager, etc. 
       */
      ws->buffer_create = xm_buffer_create;
      ws->user_buffer_create = xm_user_buffer_create;
      ws->buffer_map = xm_buffer_map;
      ws->buffer_unmap = xm_buffer_unmap;
      ws->buffer_reference = xm_buffer_reference;
      ws->buffer_data = xm_buffer_data;
      ws->buffer_subdata = xm_buffer_subdata;
      ws->buffer_get_subdata = xm_buffer_get_subdata;

      ws->surface_pitch = xm_surface_pitch;
      ws->surface_alloc = xm_surface_alloc;
      ws->surface_release = xm_surface_release;

      ws->flush_frontbuffer = xm_flush_frontbuffer;
      ws->printf = xm_printf;
      ws->get_name = xm_get_name;
   }

   return ws;
}


/**
 * The winsys being queried will have been created at glXCreateContext
 * time, with a pixel format corresponding to the context's visual.
 *
 * XXX we should pass a flag indicating if the format is going to be
 * use for a drawing surface vs. a texture.  In the later case, we
 * can support any format.
 */
static boolean
xmesa_is_format_supported(struct softpipe_winsys *sws, uint format)
{
   struct xmesa_softpipe_winsys *xmws = xmesa_softpipe_winsys(sws);

   if (format == xmws->pixelformat) {
      return TRUE;
   }
   else {
      /* non-color / window surface format */
      switch (format) {
      case PIPE_FORMAT_S_R16_G16_B16_A16:
      case PIPE_FORMAT_S8_Z24:
      case PIPE_FORMAT_U_S8:
      case PIPE_FORMAT_U_Z16:
      case PIPE_FORMAT_U_Z32:
         return TRUE;
      default:
         return FALSE;
      }
   }
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
   struct pipe_winsys *pws = xmesa_get_pipe_winsys();
   struct softpipe_winsys *spws = xmesa_get_softpipe_winsys(pixelformat);
   struct pipe_context *pipe;
   
   pipe = softpipe_create( pws, spws );
   if (pipe)
      pipe->priv = xmesa;

   return pipe;
}