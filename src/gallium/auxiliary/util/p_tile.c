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
 * RGBA/float tile get/put functions.
 * Usable both by drivers and state trackers.
 * Surfaces should already be in a mapped state.
 */


#include "pipe/p_defines.h"
#include "pipe/p_util.h"
#include "pipe/p_inlines.h"

#include "p_tile.h"



/**
 * Move raw block of pixels from surface to user memory.
 * This should be usable by any hw driver that has mappable surfaces.
 */
void
pipe_get_tile_raw(struct pipe_context *pipe,
                  struct pipe_surface *ps,
                  uint x, uint y, uint w, uint h,
                  void *p, int dst_stride)
{
   const uint cpp = ps->cpp;
   const ubyte *pSrc;
   const uint src_stride = ps->pitch * cpp;
   ubyte *pDest;
   uint i;

   if (dst_stride == 0) {
      dst_stride = w * cpp;
   }

   if (pipe_clip_tile(x, y, &w, &h, ps))
      return;

   pSrc = (const ubyte *) pipe_surface_map(ps) + (y * ps->pitch + x) * cpp;
   pDest = (ubyte *) p;

   for (i = 0; i < h; i++) {
      memcpy(pDest, pSrc, w * cpp);
      pDest += dst_stride;
      pSrc += src_stride;
   }

   pipe_surface_unmap(ps);
}


/**
 * Move raw block of pixels from user memory to surface.
 * This should be usable by any hw driver that has mappable surfaces.
 */
void
pipe_put_tile_raw(struct pipe_context *pipe,
                  struct pipe_surface *ps,
                  uint x, uint y, uint w, uint h,
                  const void *p, int src_stride)
{
   const uint cpp = ps->cpp;
   const ubyte *pSrc;
   const uint dst_stride = ps->pitch * cpp;
   ubyte *pDest;
   uint i;

   if (src_stride == 0) {
      src_stride = w * cpp;
   }

   if (pipe_clip_tile(x, y, &w, &h, ps))
      return;

   pSrc = (const ubyte *) p;
   pDest = (ubyte *) pipe_surface_map(ps) + (y * ps->pitch + x) * cpp;

   for (i = 0; i < h; i++) {
      memcpy(pDest, pSrc, w * cpp);
      pDest += dst_stride;
      pSrc += src_stride;
   }

   pipe_surface_unmap(ps);
}




/** Convert short in [-32768,32767] to GLfloat in [-1.0,1.0] */
#define SHORT_TO_FLOAT(S)   ((2.0F * (S) + 1.0F) * (1.0F/65535.0F))

#define UNCLAMPED_FLOAT_TO_SHORT(us, f)  \
   us = ( (short) ( CLAMP((f), -1.0, 1.0) * 32767.0F) )



/*** PIPE_FORMAT_A8R8G8B8_UNORM ***/

static void
a8r8g8b8_get_tile_rgba(unsigned *src,
                       unsigned w, unsigned h,
                       float *p,
                       unsigned dst_stride)
{
   unsigned i, j;

   for (i = 0; i < h; i++) {
      float *pRow = p;
      for (j = 0; j < w; j++, pRow += 4) {
         const unsigned pixel = *src++;
         pRow[0] = UBYTE_TO_FLOAT((pixel >> 16) & 0xff);
         pRow[1] = UBYTE_TO_FLOAT((pixel >>  8) & 0xff);
         pRow[2] = UBYTE_TO_FLOAT((pixel >>  0) & 0xff);
         pRow[3] = UBYTE_TO_FLOAT((pixel >> 24) & 0xff);
      }
      p += dst_stride;
   }
}


static void
a8r8g8b8_put_tile_rgba(unsigned *dst,
                       unsigned w, unsigned h,
                       const float *p,
                       unsigned src_stride)
{
   unsigned i, j;

   for (i = 0; i < h; i++) {
      const float *pRow = p;
      for (j = 0; j < w; j++, pRow += 4) {
         unsigned r, g, b, a;
         UNCLAMPED_FLOAT_TO_UBYTE(r, pRow[0]);
         UNCLAMPED_FLOAT_TO_UBYTE(g, pRow[1]);
         UNCLAMPED_FLOAT_TO_UBYTE(b, pRow[2]);
         UNCLAMPED_FLOAT_TO_UBYTE(a, pRow[3]);
         *dst++ = (a << 24) | (r << 16) | (g << 8) | b;
      }
      p += src_stride;
   }
}


/*** PIPE_FORMAT_B8G8R8A8_UNORM ***/

static void
b8g8r8a8_get_tile_rgba(unsigned *src,
                       unsigned w, unsigned h,
                       float *p,
                       unsigned dst_stride)
{
   unsigned i, j;

   for (i = 0; i < h; i++) {
      float *pRow = p;
      for (j = 0; j < w; j++, pRow += 4) {
         const unsigned pixel = *src++;
         pRow[0] = UBYTE_TO_FLOAT((pixel >>  8) & 0xff);
         pRow[1] = UBYTE_TO_FLOAT((pixel >> 16) & 0xff);
         pRow[2] = UBYTE_TO_FLOAT((pixel >> 24) & 0xff);
         pRow[3] = UBYTE_TO_FLOAT((pixel >>  0) & 0xff);
      }
      p += dst_stride;
   }
}


static void
b8g8r8a8_put_tile_rgba(unsigned *dst,
                       unsigned w, unsigned h,
                       const float *p,
                       unsigned src_stride)
{
   unsigned i, j;

   for (i = 0; i < h; i++) {
      const float *pRow = p;
      for (j = 0; j < w; j++, pRow += 4) {
         unsigned r, g, b, a;
         UNCLAMPED_FLOAT_TO_UBYTE(r, pRow[0]);
         UNCLAMPED_FLOAT_TO_UBYTE(g, pRow[1]);
         UNCLAMPED_FLOAT_TO_UBYTE(b, pRow[2]);
         UNCLAMPED_FLOAT_TO_UBYTE(a, pRow[3]);
         *dst++ = (b << 24) | (g << 16) | (r << 8) | a;
      }
      p += src_stride;
   }
}


/*** PIPE_FORMAT_A1R5G5B5_UNORM ***/

static void
a1r5g5b5_get_tile_rgba(ushort *src,
                       unsigned w, unsigned h,
                       float *p,
                       unsigned dst_stride)
{
   unsigned i, j;

   for (i = 0; i < h; i++) {
      float *pRow = p;
      for (j = 0; j < w; j++, pRow += 4) {
         const ushort pixel = *src++;
         pRow[0] = ((pixel >> 10) & 0x1f) * (1.0f / 31.0f);
         pRow[1] = ((pixel >>  5) & 0x1f) * (1.0f / 31.0f);
         pRow[2] = ((pixel      ) & 0x1f) * (1.0f / 31.0f);
         pRow[3] = ((pixel >> 15)       ) * 1.0f;
      }
      p += dst_stride;
   }
}


/*** PIPE_FORMAT_A4R4G4B4_UNORM ***/

static void
a4r4g4b4_get_tile_rgba(ushort *src,
                       unsigned w, unsigned h,
                       float *p,
                       unsigned dst_stride)
{
   unsigned i, j;

   for (i = 0; i < h; i++) {
      float *pRow = p;
      for (j = 0; j < w; j++, pRow += 4) {
         const ushort pixel = *src++;
         pRow[0] = ((pixel >>  8) & 0xf) * (1.0f / 15.0f);
         pRow[1] = ((pixel >>  4) & 0xf) * (1.0f / 15.0f);
         pRow[2] = ((pixel      ) & 0xf) * (1.0f / 15.0f);
         pRow[3] = ((pixel >> 12)      ) * (1.0f / 15.0f);
      }
      p += dst_stride;
   }
}


/*** PIPE_FORMAT_R5G6B5_UNORM ***/

static void
r5g6b5_get_tile_rgba(ushort *src,
                     unsigned w, unsigned h,
                     float *p,
                     unsigned dst_stride)
{
   unsigned i, j;

   for (i = 0; i < h; i++) {
      float *pRow = p;
      for (j = 0; j < w; j++, pRow += 4) {
         const ushort pixel = *src++;
         pRow[0] = ((pixel >> 11) & 0x1f) * (1.0f / 31.0f);
         pRow[1] = ((pixel >>  5) & 0x3f) * (1.0f / 63.0f);
         pRow[2] = ((pixel      ) & 0x1f) * (1.0f / 31.0f);
         pRow[3] = 1.0f;
      }
      p += dst_stride;
   }
}


static void
r5g5b5_put_tile_rgba(ushort *dst,
                     unsigned w, unsigned h,
                     const float *p,
                     unsigned src_stride)
{
   unsigned i, j;

   for (i = 0; i < h; i++) {
      const float *pRow = p;
      for (j = 0; j < w; j++, pRow += 4) {
         uint r = (uint) (CLAMP(pRow[0], 0.0, 1.0) * 31.0);
         uint g = (uint) (CLAMP(pRow[1], 0.0, 1.0) * 63.0);
         uint b = (uint) (CLAMP(pRow[2], 0.0, 1.0) * 31.0);
         *dst++ = (r << 11) | (g << 5) | (b);
      }
      p += src_stride;
   }
}



/*** PIPE_FORMAT_Z16_UNORM ***/

/**
 * Return each Z value as four floats in [0,1].
 */
static void
z16_get_tile_rgba(ushort *src,
                  unsigned w, unsigned h,
                  float *p,
                  unsigned dst_stride)
{
   const float scale = 1.0f / 65535.0f;
   unsigned i, j;

   for (i = 0; i < h; i++) {
      float *pRow = p;
      for (j = 0; j < w; j++, pRow += 4) {
         pRow[0] =
         pRow[1] =
         pRow[2] =
         pRow[3] = *src++ * scale;
      }
      p += dst_stride;
   }
}




/*** PIPE_FORMAT_U_L8 ***/

static void
l8_get_tile_rgba(ubyte *src,
                 unsigned w, unsigned h,
                 float *p,
                 unsigned dst_stride)
{
   unsigned i, j;

   for (i = 0; i < h; i++) {
      float *pRow = p;
      for (j = 0; j < w; j++, src++, pRow += 4) {
         pRow[0] =
         pRow[1] =
         pRow[2] = UBYTE_TO_FLOAT(*src);
         pRow[3] = 1.0;
      }
      p += dst_stride;
   }
}


/*** PIPE_FORMAT_U_A8 ***/

static void
a8_get_tile_rgba(ubyte *src,
                 unsigned w, unsigned h,
                 float *p,
                 unsigned dst_stride)
{
   unsigned i, j;

   for (i = 0; i < h; i++) {
      float *pRow = p;
      for (j = 0; j < w; j++, src++, pRow += 4) {
         pRow[0] =
         pRow[1] =
         pRow[2] = 0.0;
         pRow[3] = UBYTE_TO_FLOAT(*src);
      }
      p += dst_stride;
   }
}


/*** PIPE_FORMAT_R16G16B16A16_SNORM ***/

static void
r16g16b16a16_get_tile_rgba(short *src,
                           unsigned w, unsigned h,
                           float *p,
                           unsigned dst_stride)
{
   unsigned i, j;

   for (i = 0; i < h; i++) {
      float *pRow = p;
      for (j = 0; j < w; j++, src += 4, pRow += 4) {
         pRow[0] = SHORT_TO_FLOAT(src[0]);
         pRow[1] = SHORT_TO_FLOAT(src[1]);
         pRow[2] = SHORT_TO_FLOAT(src[2]);
         pRow[3] = SHORT_TO_FLOAT(src[3]);
      }
      p += dst_stride;
   }
}


static void
r16g16b16a16_put_tile_rgba(short *dst,
                           unsigned w, unsigned h,
                           const float *p,
                           unsigned src_stride)
{
   unsigned i, j;

   for (i = 0; i < h; i++) {
      const float *pRow = p;
      for (j = 0; j < w; j++, dst += 4, pRow += 4) {
         UNCLAMPED_FLOAT_TO_SHORT(dst[0], pRow[0]);
         UNCLAMPED_FLOAT_TO_SHORT(dst[1], pRow[1]);
         UNCLAMPED_FLOAT_TO_SHORT(dst[2], pRow[2]);
         UNCLAMPED_FLOAT_TO_SHORT(dst[3], pRow[3]);
      }
      p += src_stride;
   }
}



/*** PIPE_FORMAT_U_I8 ***/

static void
i8_get_tile_rgba(ubyte *src,
                 unsigned w, unsigned h,
                 float *p,
                 unsigned dst_stride)
{
   unsigned i, j;

   for (i = 0; i < h; i++) {
      float *pRow = p;
      for (j = 0; j < w; j++, src++, pRow += 4) {
         pRow[0] =
         pRow[1] =
         pRow[2] =
         pRow[3] = UBYTE_TO_FLOAT(*src);
      }
      p += dst_stride;
   }
}


/*** PIPE_FORMAT_U_A8_L8 ***/

static void
a8_l8_get_tile_rgba(ushort *src,
                    unsigned w, unsigned h,
                    float *p,
                    unsigned dst_stride)
{
   unsigned i, j;

   for (i = 0; i < h; i++) {
      float *pRow = p;
      for (j = 0; j < w; j++, pRow += 4) {
         ushort p = *src++;
         pRow[0] =
         pRow[1] =
         pRow[2] = UBYTE_TO_FLOAT(p & 0xff);
         pRow[3] = UBYTE_TO_FLOAT(p >> 8);
      }
      p += dst_stride;
   }
}




/*** PIPE_FORMAT_Z32_UNORM ***/

/**
 * Return each Z value as four floats in [0,1].
 */
static void
z32_get_tile_rgba(unsigned *src,
                  unsigned w, unsigned h,
                  float *p,
                  unsigned dst_stride)
{
   const double scale = 1.0 / (double) 0xffffffff;
   unsigned i, j;

   for (i = 0; i < h; i++) {
      float *pRow = p;
      for (j = 0; j < w; j++, pRow += 4) {
         pRow[0] =
         pRow[1] =
         pRow[2] =
         pRow[3] = (float) (*src++ * scale);
      }
      p += dst_stride;
   }
}


/*** PIPE_FORMAT_S8Z24_UNORM ***/

/**
 * Return Z component as four float in [0,1].  Stencil part ignored.
 */
static void
s8z24_get_tile_rgba(unsigned *src,
                    unsigned w, unsigned h,
                    float *p,
                    unsigned dst_stride)
{
   const double scale = 1.0 / ((1 << 24) - 1);
   unsigned i, j;

   for (i = 0; i < h; i++) {
      float *pRow = p;
      for (j = 0; j < w; j++, pRow += 4) {
         pRow[0] =
         pRow[1] =
         pRow[2] =
         pRow[3] = (float) (scale * (*src++ & 0xffffff));
      }
      p += dst_stride;
   }
}


/*** PIPE_FORMAT_Z24S8_UNORM ***/

/**
 * Return Z component as four float in [0,1].  Stencil part ignored.
 */
static void
z24s8_get_tile_rgba(unsigned *src,
                    unsigned w, unsigned h,
                    float *p,
                    unsigned dst_stride)
{
   const double scale = 1.0 / ((1 << 24) - 1);
   unsigned i, j;

   for (i = 0; i < h; i++) {
      float *pRow = p;
      for (j = 0; j < w; j++, pRow += 4) {
         pRow[0] =
         pRow[1] =
         pRow[2] =
         pRow[3] = (float) (scale * (*src++ >> 8));
      }
      p += dst_stride;
   }
}


void
pipe_get_tile_rgba(struct pipe_context *pipe,
                   struct pipe_surface *ps,
                   uint x, uint y, uint w, uint h,
                   float *p)
{
   unsigned dst_stride = w * 4;
   void *packed;

   if (pipe_clip_tile(x, y, &w, &h, ps))
      return;

   packed = MALLOC(h * w * ps->cpp);

   if (!packed)
      return;

   pipe_get_tile_raw(pipe, ps, x, y, w, h, packed, w * ps->cpp);

   switch (ps->format) {
   case PIPE_FORMAT_A8R8G8B8_UNORM:
      a8r8g8b8_get_tile_rgba((unsigned *) packed, w, h, p, dst_stride);
      break;
   case PIPE_FORMAT_B8G8R8A8_UNORM:
      b8g8r8a8_get_tile_rgba((unsigned *) packed, w, h, p, dst_stride);
      break;
   case PIPE_FORMAT_A1R5G5B5_UNORM:
      a1r5g5b5_get_tile_rgba((ushort *) packed, w, h, p, dst_stride);
      break;
   case PIPE_FORMAT_A4R4G4B4_UNORM:
      a4r4g4b4_get_tile_rgba((ushort *) packed, w, h, p, dst_stride);
      break;
   case PIPE_FORMAT_R5G6B5_UNORM:
      r5g6b5_get_tile_rgba((ushort *) packed, w, h, p, dst_stride);
      break;
   case PIPE_FORMAT_U_L8:
      l8_get_tile_rgba((ubyte *) packed, w, h, p, dst_stride);
      break;
   case PIPE_FORMAT_U_A8:
      a8_get_tile_rgba((ubyte *) packed, w, h, p, dst_stride);
      break;
   case PIPE_FORMAT_U_I8:
      i8_get_tile_rgba((ubyte *) packed, w, h, p, dst_stride);
      break;
   case PIPE_FORMAT_U_A8_L8:
      a8_l8_get_tile_rgba((ushort *) packed, w, h, p, dst_stride);
      break;
   case PIPE_FORMAT_R16G16B16A16_SNORM:
      r16g16b16a16_get_tile_rgba((short *) packed, w, h, p, dst_stride);
      break;
   case PIPE_FORMAT_Z16_UNORM:
      z16_get_tile_rgba((ushort *) packed, w, h, p, dst_stride);
      break;
   case PIPE_FORMAT_Z32_UNORM:
      z32_get_tile_rgba((unsigned *) packed, w, h, p, dst_stride);
      break;
   case PIPE_FORMAT_S8Z24_UNORM:
      s8z24_get_tile_rgba((unsigned *) packed, w, h, p, dst_stride);
      break;
   case PIPE_FORMAT_Z24S8_UNORM:
      z24s8_get_tile_rgba((unsigned *) packed, w, h, p, dst_stride);
      break;
   default:
      assert(0);
   }

   FREE(packed);
}


void
pipe_put_tile_rgba(struct pipe_context *pipe,
                   struct pipe_surface *ps,
                   uint x, uint y, uint w, uint h,
                   const float *p)
{
   unsigned src_stride = w * 4;
   void *packed;

   if (pipe_clip_tile(x, y, &w, &h, ps))
      return;

   packed = MALLOC(h * w * ps->cpp);

   if (!packed)
      return;

   switch (ps->format) {
   case PIPE_FORMAT_A8R8G8B8_UNORM:
      a8r8g8b8_put_tile_rgba((unsigned *) packed, w, h, p, src_stride);
      break;
   case PIPE_FORMAT_B8G8R8A8_UNORM:
      b8g8r8a8_put_tile_rgba((unsigned *) packed, w, h, p, src_stride);
      break;
   case PIPE_FORMAT_A1R5G5B5_UNORM:
      /*a1r5g5b5_put_tile_rgba((ushort *) packed, w, h, p, src_stride);*/
      break;
   case PIPE_FORMAT_R5G6B5_UNORM:
      r5g5b5_put_tile_rgba((ushort *) packed, w, h, p, src_stride);
      break;
   case PIPE_FORMAT_R8G8B8A8_UNORM:
      break;
   case PIPE_FORMAT_U_L8:
      /*l8_put_tile_rgba((ubyte *) packed, w, h, p, src_stride);*/
      break;
   case PIPE_FORMAT_U_A8:
      /*a8_put_tile_rgba((ubyte *) packed, w, h, p, src_stride);*/
      break;
   case PIPE_FORMAT_U_I8:
      /*i8_put_tile_rgba((ubyte *) packed, w, h, p, src_stride);*/
      break;
   case PIPE_FORMAT_U_A8_L8:
      /*a8_l8_put_tile_rgba((ushort *) packed, w, h, p, src_stride);*/
      break;
   case PIPE_FORMAT_R16G16B16A16_SNORM:
      r16g16b16a16_put_tile_rgba((short *) packed, w, h, p, src_stride);
      break;
   case PIPE_FORMAT_Z16_UNORM:
      /*z16_put_tile_rgba((ushort *) packed, w, h, p, src_stride);*/
      break;
   case PIPE_FORMAT_Z32_UNORM:
      /*z32_put_tile_rgba((unsigned *) packed, w, h, p, src_stride);*/
      break;
   case PIPE_FORMAT_S8Z24_UNORM:
      /*s8z24_put_tile_rgba((unsigned *) packed, w, h, p, src_stride);*/
      break;
   case PIPE_FORMAT_Z24S8_UNORM:
      /*z24s8_put_tile_rgba((unsigned *) packed, w, h, p, src_stride);*/
      break;
   default:
      assert(0);
   }

   pipe_put_tile_raw(pipe, ps, x, y, w, h, packed, w * ps->cpp);

   FREE(packed);
}


/**
 * Get a block of Z values, converted to 32-bit range.
 */
void
pipe_get_tile_z(struct pipe_context *pipe,
                struct pipe_surface *ps,
                uint x, uint y, uint w, uint h,
                uint *z)
{
   const uint dstStride = w;
   uint *pDest = z;
   uint i, j;

   if (pipe_clip_tile(x, y, &w, &h, ps))
      return;

   switch (ps->format) {
   case PIPE_FORMAT_Z32_UNORM:
      {
         const uint *pSrc
            = (const uint *) pipe_surface_map(ps) + (y * ps->pitch + x);
         for (i = 0; i < h; i++) {
            memcpy(pDest, pSrc, 4 * w);
            pDest += dstStride;
            pSrc += ps->pitch;
         }
      }
      break;
   case PIPE_FORMAT_S8Z24_UNORM:
      {
         const uint *pSrc
            = (const uint *) pipe_surface_map(ps) + (y * ps->pitch + x);
         for (i = 0; i < h; i++) {
            for (j = 0; j < w; j++) {
               /* convert 24-bit Z to 32-bit Z */
               pDest[j] = (pSrc[j] << 8) | (pSrc[j] & 0xff);
            }
            pDest += dstStride;
            pSrc += ps->pitch;
         }
      }
      break;
   case PIPE_FORMAT_Z16_UNORM:
      {
         const ushort *pSrc
            = (const ushort *) pipe_surface_map(ps) + (y * ps->pitch + x);
         for (i = 0; i < h; i++) {
            for (j = 0; j < w; j++) {
               /* convert 16-bit Z to 32-bit Z */
               pDest[j] = (pSrc[j] << 16) | pSrc[j];
            }
            pDest += dstStride;
            pSrc += ps->pitch;
         }
      }
      break;
   default:
      assert(0);
   }

   pipe_surface_unmap(ps);
}


void
pipe_put_tile_z(struct pipe_context *pipe,
                struct pipe_surface *ps,
                uint x, uint y, uint w, uint h,
                const uint *zSrc)
{
   const uint srcStride = w;
   const uint *pSrc = zSrc;
   uint i, j;

   if (pipe_clip_tile(x, y, &w, &h, ps))
      return;

   switch (ps->format) {
   case PIPE_FORMAT_Z32_UNORM:
      {
         uint *pDest = (uint *) pipe_surface_map(ps) + (y * ps->pitch + x);
         for (i = 0; i < h; i++) {
            memcpy(pDest, pSrc, 4 * w);
            pDest += ps->pitch;
            pSrc += srcStride;
         }
      }
      break;
   case PIPE_FORMAT_S8Z24_UNORM:
      {
         uint *pDest = (uint *) pipe_surface_map(ps) + (y * ps->pitch + x);
         for (i = 0; i < h; i++) {
            for (j = 0; j < w; j++) {
               /* convert 32-bit Z to 24-bit Z (0 stencil) */
               pDest[j] = pSrc[j] >> 8;
            }
            pDest += ps->pitch;
            pSrc += srcStride;
         }
      }
      break;
   case PIPE_FORMAT_Z16_UNORM:
      {
         ushort *pDest = (ushort *) pipe_surface_map(ps) + (y * ps->pitch + x);
         for (i = 0; i < h; i++) {
            for (j = 0; j < w; j++) {
               /* convert 32-bit Z to 16-bit Z */
               pDest[j] = pSrc[j] >> 16;
            }
            pDest += ps->pitch;
            pSrc += srcStride;
         }
      }
      break;
   default:
      assert(0);
   }

   pipe_surface_unmap(ps);
}

