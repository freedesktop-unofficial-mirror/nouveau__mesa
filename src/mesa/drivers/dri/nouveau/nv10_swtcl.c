/*
 * Copyright 1998-2003 VIA Technologies, Inc. All Rights Reserved.
 * Copyright 2001-2003 S3 Graphics, Inc. All Rights Reserved.
 * Copyright 2006 Stephane Marchesin. All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sub license,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. IN NO EVENT SHALL
 * VIA, S3 GRAPHICS, AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

/* Software TCL for NV10, NV20, NV30, NV40, NV50 */

#include <stdio.h>
#include <math.h>

#include "glheader.h"
#include "context.h"
#include "mtypes.h"
#include "macros.h"
#include "colormac.h"
#include "enums.h"

#include "swrast/swrast.h"
#include "swrast_setup/swrast_setup.h"
#include "tnl/t_context.h"
#include "tnl/t_pipeline.h"

#include "nouveau_swtcl.h"
#include "nv10_swtcl.h"
#include "nouveau_context.h"
#include "nouveau_span.h"
#include "nouveau_reg.h"
#include "nouveau_tex.h"
#include "nouveau_fifo.h"
#include "nouveau_msg.h"
#include "nouveau_object.h"

static void nv10RasterPrimitive( GLcontext *ctx, GLenum rprim, GLuint hwprim );
static void nv10RenderPrimitive( GLcontext *ctx, GLenum prim );
static void nv10ResetLineStipple( GLcontext *ctx );

static const int default_attr_size[8]={3,3,3,4,3,1,4,4};

/* Mesa requires us to put pos attribute as the first attribute of the
 * vertex, but on NV10 it is the last attribute.
 * To fix that we put the pos attribute first, and we swap the pos
 * attribute before sending it to the card.
 * Speed cost of the swap seems negligeable
 */
#if 0
/* old stuff where pos attribute isn't put first for mesa.
 * Usefull for speed comparaison
 */
#define INV_VERT(i) i
#define OUT_RING_VERTp(nmesa, ptr,sz, vertex_size) OUT_RINGp(ptr,sz)
#define OUT_RING_VERT(nmesa, ptr, vertex_size) OUT_RINGp(ptr,vertex_size)
#else

#define INV_VERT(i) (i==0?7:i-1)

#define OUT_RING_VERT_RAW(ptr,vertex_size) do{						\
	/* if the vertex size is not null, we have at least pos attribute */ \
	OUT_RINGp((GLfloat *)(ptr) + default_attr_size[_TNL_ATTRIB_POS], (vertex_size) - default_attr_size[_TNL_ATTRIB_POS]); \
	OUT_RINGp((GLfloat *)(ptr), default_attr_size[_TNL_ATTRIB_POS]); \
}while(0)

#define OUT_RING_VERT(nmesa,ptr,vertex_size) do{ \
	if (nmesa->screen->card->type>=NV_20) \
		OUT_RINGp(ptr, vertex_size); \
	else \
		OUT_RING_VERT_RAW(ptr, vertex_size); \
}while(0)


#define OUT_RING_VERTp(nmesa, ptr,sz, vertex_size) do{						\
	int nb_vert; \
	if (nmesa->screen->card->type>=NV_20) \
		OUT_RINGp(ptr, sz); \
	else \
		for (nb_vert = 0; nb_vert < (sz)/(vertex_size); nb_vert++) { \
			OUT_RING_VERT_RAW((GLfloat*)(ptr)+nb_vert*(vertex_size), vertex_size); \
		} \
}while(0)

#endif


static inline void nv10StartPrimitive(struct nouveau_context* nmesa,GLuint primitive,GLuint size)
{
	if ((nmesa->screen->card->type>=NV_10) && (nmesa->screen->card->type<=NV_17))
		BEGIN_RING_SIZE(NvSub3D,NV10_TCL_PRIMITIVE_3D_BEGIN_END,1);
	else if (nmesa->screen->card->type==NV_20)
		BEGIN_RING_SIZE(NvSub3D,NV20_TCL_PRIMITIVE_3D_BEGIN_END,1);
	else
		BEGIN_RING_SIZE(NvSub3D,NV30_TCL_PRIMITIVE_3D_BEGIN_END,1);
	OUT_RING(primitive);

	if ((nmesa->screen->card->type>=NV_10) && (nmesa->screen->card->type<=NV_17))
		BEGIN_RING_SIZE(NvSub3D,NV10_TCL_PRIMITIVE_3D_VERTEX_ARRAY_DATA|NONINC_METHOD,size);
	else if (nmesa->screen->card->type==NV_20)
		BEGIN_RING_SIZE(NvSub3D,NV20_TCL_PRIMITIVE_3D_VERTEX_DATA|NONINC_METHOD,size);
	else
		BEGIN_RING_SIZE(NvSub3D,NV30_TCL_PRIMITIVE_3D_VERTEX_DATA|NONINC_METHOD,size);
}

void nv10FinishPrimitive(struct nouveau_context *nmesa)
{
	if ((nmesa->screen->card->type>=NV_10) && (nmesa->screen->card->type<=NV_17))
		BEGIN_RING_SIZE(NvSub3D,NV10_TCL_PRIMITIVE_3D_BEGIN_END,1);
	else if (nmesa->screen->card->type==NV_20)
		BEGIN_RING_SIZE(NvSub3D,NV20_TCL_PRIMITIVE_3D_BEGIN_END,1);
	else
		BEGIN_RING_SIZE(NvSub3D,NV30_TCL_PRIMITIVE_3D_BEGIN_END,1);
	OUT_RING(0x0);
	FIRE_RING();
}


static inline void nv10ExtendPrimitive(struct nouveau_context* nmesa, int size)
{
	/* make sure there's enough room. if not, wait */
	if (RING_AVAILABLE()<size)
	{
		WAIT_RING(nmesa,size);
	}
}

/**********************************************************************/
/*               Render unclipped begin/end objects                   */
/**********************************************************************/

static inline void nv10_render_generic_primitive_verts(GLcontext *ctx,GLuint start,GLuint count,GLuint flags,GLuint prim)
{
	struct nouveau_context *nmesa = NOUVEAU_CONTEXT(ctx);
	GLfloat *vertptr = (GLfloat *)nmesa->verts;
	GLuint vertsize = nmesa->vertex_size;
	GLuint size_dword = vertsize*(count-start);

	nv10ExtendPrimitive(nmesa, size_dword);
	nv10StartPrimitive(nmesa,prim+1,size_dword);
	OUT_RING_VERTp(nmesa, (nouveauVertex*)(vertptr+(start*vertsize)),size_dword, vertsize);
	nv10FinishPrimitive(nmesa);
}

static void nv10_render_points_verts(GLcontext *ctx,GLuint start,GLuint count,GLuint flags)
{
	nv10_render_generic_primitive_verts(ctx,start,count,flags,GL_POINTS);
}

static void nv10_render_lines_verts(GLcontext *ctx,GLuint start,GLuint count,GLuint flags)
{
	nv10_render_generic_primitive_verts(ctx,start,count,flags,GL_LINES);
}

static void nv10_render_line_strip_verts(GLcontext *ctx,GLuint start,GLuint count,GLuint flags)
{
	nv10_render_generic_primitive_verts(ctx,start,count,flags,GL_LINE_STRIP);
}

static void nv10_render_line_loop_verts(GLcontext *ctx,GLuint start,GLuint count,GLuint flags)
{
	nv10_render_generic_primitive_verts(ctx,start,count,flags,GL_LINE_LOOP);
}

static void nv10_render_triangles_verts(GLcontext *ctx,GLuint start,GLuint count,GLuint flags)
{
	nv10_render_generic_primitive_verts(ctx,start,count,flags,GL_TRIANGLES);
}

static void nv10_render_tri_strip_verts(GLcontext *ctx,GLuint start,GLuint count,GLuint flags)
{
	nv10_render_generic_primitive_verts(ctx,start,count,flags,GL_TRIANGLE_STRIP);
}

static void nv10_render_tri_fan_verts(GLcontext *ctx,GLuint start,GLuint count,GLuint flags)
{
	nv10_render_generic_primitive_verts(ctx,start,count,flags,GL_TRIANGLE_FAN);
}

static void nv10_render_quads_verts(GLcontext *ctx,GLuint start,GLuint count,GLuint flags)
{
	nv10_render_generic_primitive_verts(ctx,start,count,flags,GL_QUADS);
}

static void nv10_render_quad_strip_verts(GLcontext *ctx,GLuint start,GLuint count,GLuint flags)
{
	nv10_render_generic_primitive_verts(ctx,start,count,flags,GL_QUAD_STRIP);
}

static void nv10_render_poly_verts(GLcontext *ctx,GLuint start,GLuint count,GLuint flags)
{
	nv10_render_generic_primitive_verts(ctx,start,count,flags,GL_POLYGON);
}

static void nv10_render_noop_verts(GLcontext *ctx,GLuint start,GLuint count,GLuint flags)
{
}

static void (*nv10_render_tab_verts[GL_POLYGON+2])(GLcontext *,
							   GLuint,
							   GLuint,
							   GLuint) =
{
   nv10_render_points_verts,
   nv10_render_lines_verts,
   nv10_render_line_loop_verts,
   nv10_render_line_strip_verts,
   nv10_render_triangles_verts,
   nv10_render_tri_strip_verts,
   nv10_render_tri_fan_verts,
   nv10_render_quads_verts,
   nv10_render_quad_strip_verts,
   nv10_render_poly_verts,
   nv10_render_noop_verts,
};


static inline void nv10_render_generic_primitive_elts(GLcontext *ctx,GLuint start,GLuint count,GLuint flags,GLuint prim)
{
	struct nouveau_context *nmesa = NOUVEAU_CONTEXT(ctx);
	GLfloat *vertptr = (GLfloat *)nmesa->verts;
	GLuint vertsize = nmesa->vertex_size;
	GLuint size_dword = vertsize*(count-start);
	const GLuint * const elt = TNL_CONTEXT(ctx)->vb.Elts;
	GLuint j;

	nv10ExtendPrimitive(nmesa, size_dword);
	nv10StartPrimitive(nmesa,prim+1,size_dword);
	for (j=start; j<count; j++ ) {
		OUT_RING_VERT(nmesa, (nouveauVertex*)(vertptr+(elt[j]*vertsize)),vertsize);
	}
	nv10FinishPrimitive(nmesa);
}

static void nv10_render_points_elts(GLcontext *ctx,GLuint start,GLuint count,GLuint flags)
{
	nv10_render_generic_primitive_elts(ctx,start,count,flags,GL_POINTS);
}

static void nv10_render_lines_elts(GLcontext *ctx,GLuint start,GLuint count,GLuint flags)
{
	nv10_render_generic_primitive_elts(ctx,start,count,flags,GL_LINES);
}

static void nv10_render_line_strip_elts(GLcontext *ctx,GLuint start,GLuint count,GLuint flags)
{
	nv10_render_generic_primitive_elts(ctx,start,count,flags,GL_LINE_STRIP);
}

static void nv10_render_line_loop_elts(GLcontext *ctx,GLuint start,GLuint count,GLuint flags)
{
	nv10_render_generic_primitive_elts(ctx,start,count,flags,GL_LINE_LOOP);
}

static void nv10_render_triangles_elts(GLcontext *ctx,GLuint start,GLuint count,GLuint flags)
{
	nv10_render_generic_primitive_elts(ctx,start,count,flags,GL_TRIANGLES);
}

static void nv10_render_tri_strip_elts(GLcontext *ctx,GLuint start,GLuint count,GLuint flags)
{
	nv10_render_generic_primitive_elts(ctx,start,count,flags,GL_TRIANGLE_STRIP);
}

static void nv10_render_tri_fan_elts(GLcontext *ctx,GLuint start,GLuint count,GLuint flags)
{
	nv10_render_generic_primitive_elts(ctx,start,count,flags,GL_TRIANGLE_FAN);
}

static void nv10_render_quads_elts(GLcontext *ctx,GLuint start,GLuint count,GLuint flags)
{
	nv10_render_generic_primitive_elts(ctx,start,count,flags,GL_QUADS);
}

static void nv10_render_quad_strip_elts(GLcontext *ctx,GLuint start,GLuint count,GLuint flags)
{
	nv10_render_generic_primitive_elts(ctx,start,count,flags,GL_QUAD_STRIP);
}

static void nv10_render_poly_elts(GLcontext *ctx,GLuint start,GLuint count,GLuint flags)
{
	nv10_render_generic_primitive_elts(ctx,start,count,flags,GL_POLYGON);
}

static void nv10_render_noop_elts(GLcontext *ctx,GLuint start,GLuint count,GLuint flags)
{
}

static void (*nv10_render_tab_elts[GL_POLYGON+2])(GLcontext *,
							   GLuint,
							   GLuint,
							   GLuint) =
{
   nv10_render_points_elts,
   nv10_render_lines_elts,
   nv10_render_line_loop_elts,
   nv10_render_line_strip_elts,
   nv10_render_triangles_elts,
   nv10_render_tri_strip_elts,
   nv10_render_tri_fan_elts,
   nv10_render_quads_elts,
   nv10_render_quad_strip_elts,
   nv10_render_poly_elts,
   nv10_render_noop_elts,
};


/**********************************************************************/
/*                    Choose render functions                         */
/**********************************************************************/


#define EMIT_ATTR( ATTR, STYLE )					\
do {									\
   nmesa->vertex_attrs[nmesa->vertex_attr_count].attrib = (ATTR);	\
   nmesa->vertex_attrs[nmesa->vertex_attr_count].format = (STYLE);	\
   nmesa->vertex_attr_count++;						\
} while (0)

static inline void nv10_render_point(GLcontext *ctx, GLfloat *vertptr)
{
	struct nouveau_context *nmesa = NOUVEAU_CONTEXT(ctx);
	GLuint vertsize = nmesa->vertex_size;
	GLuint size_dword = vertsize;

	nv10ExtendPrimitive(nmesa, size_dword);
	nv10StartPrimitive(nmesa,GL_POINTS+1,size_dword);
	OUT_RING_VERT(nmesa, (nouveauVertex*)(vertptr),vertsize);
	nv10FinishPrimitive(nmesa);
}

static inline void nv10_render_points(GLcontext *ctx,GLuint first,GLuint last)
{
	struct vertex_buffer *VB = &TNL_CONTEXT(ctx)->vb;
	struct nouveau_context *nmesa = NOUVEAU_CONTEXT(ctx);
	GLfloat *vertptr = (GLfloat *)nmesa->verts;
	GLuint vertsize = nmesa->vertex_size;
	GLuint i;

	if (VB->Elts) {
		for (i = first; i < last; i++)
			if (VB->ClipMask[VB->Elts[i]] == 0)
				nv10_render_point(ctx, vertptr + (VB->Elts[i]*vertsize));
	}
	else {
		for (i = first; i < last; i++)
			if (VB->ClipMask[i] == 0)
				nv10_render_point(ctx, vertptr + (i*vertsize));
	}
}

static inline void nv10_render_line(GLcontext *ctx,GLuint v1,GLuint v2)
{
	struct nouveau_context *nmesa = NOUVEAU_CONTEXT(ctx);
	GLfloat *vertptr = (GLfloat *)nmesa->verts;
	GLuint vertsize = nmesa->vertex_size;
	GLuint size_dword = vertsize*2;

	nv10ExtendPrimitive(nmesa, size_dword);
	nv10StartPrimitive(nmesa,GL_LINES+1,size_dword);
	OUT_RING_VERT(nmesa, (nouveauVertex*)(vertptr+(v1*vertsize)),vertsize);
	OUT_RING_VERT(nmesa, (nouveauVertex*)(vertptr+(v2*vertsize)),vertsize);
	nv10FinishPrimitive(nmesa);
}

static inline void nv10_render_triangle(GLcontext *ctx,GLuint v1,GLuint v2,GLuint v3)
{
	struct nouveau_context *nmesa = NOUVEAU_CONTEXT(ctx);
	GLfloat *vertptr = (GLfloat *)nmesa->verts;
	GLuint vertsize = nmesa->vertex_size;
	GLuint size_dword = vertsize*3;

	nv10ExtendPrimitive(nmesa, size_dword);
	nv10StartPrimitive(nmesa,GL_TRIANGLES+1,size_dword);
	OUT_RING_VERT(nmesa, (nouveauVertex*)(vertptr+(v1*vertsize)),vertsize);
	OUT_RING_VERT(nmesa, (nouveauVertex*)(vertptr+(v2*vertsize)),vertsize);
	OUT_RING_VERT(nmesa, (nouveauVertex*)(vertptr+(v3*vertsize)),vertsize);
	nv10FinishPrimitive(nmesa);
}

static inline void nv10_render_quad(GLcontext *ctx,GLuint v1,GLuint v2,GLuint v3,GLuint v4)
{
	struct nouveau_context *nmesa = NOUVEAU_CONTEXT(ctx);
	GLfloat *vertptr = (GLfloat *)nmesa->verts;
	GLuint vertsize = nmesa->vertex_size;
	GLuint size_dword = vertsize*4;

	nv10ExtendPrimitive(nmesa, size_dword);
	nv10StartPrimitive(nmesa,GL_QUADS+1,size_dword);
	OUT_RING_VERT(nmesa, (nouveauVertex*)(vertptr+(v1*vertsize)),vertsize);
	OUT_RING_VERT(nmesa, (nouveauVertex*)(vertptr+(v2*vertsize)),vertsize);
	OUT_RING_VERT(nmesa, (nouveauVertex*)(vertptr+(v3*vertsize)),vertsize);
	OUT_RING_VERT(nmesa, (nouveauVertex*)(vertptr+(v4*vertsize)),vertsize);
	nv10FinishPrimitive(nmesa);
}



static void nv10ChooseRenderState(GLcontext *ctx)
{
	TNLcontext *tnl = TNL_CONTEXT(ctx);
	struct nouveau_context *nmesa = NOUVEAU_CONTEXT(ctx);

	tnl->Driver.Render.PrimTabVerts = nv10_render_tab_verts;
	tnl->Driver.Render.PrimTabElts = nv10_render_tab_elts;
	tnl->Driver.Render.ClippedLine = _tnl_RenderClippedLine;
	tnl->Driver.Render.ClippedPolygon = _tnl_RenderClippedPolygon;
	tnl->Driver.Render.Points = nv10_render_points;
	tnl->Driver.Render.Line = nv10_render_line;
	tnl->Driver.Render.Triangle = nv10_render_triangle;
	tnl->Driver.Render.Quad = nv10_render_quad;
}



static inline void nv10OutputVertexFormat(struct nouveau_context* nmesa)
{
	GLcontext* ctx=nmesa->glCtx;
	TNLcontext *tnl = TNL_CONTEXT(ctx);
	DECLARE_RENDERINPUTS(index);
	struct vertex_buffer *VB = &tnl->vb;
	int attr_size[16];
	const int nv10_vtx_attribs[8]={
		_TNL_ATTRIB_FOG, _TNL_ATTRIB_WEIGHT,
		_TNL_ATTRIB_NORMAL, _TNL_ATTRIB_TEX1,
		_TNL_ATTRIB_TEX0, _TNL_ATTRIB_COLOR1,
		_TNL_ATTRIB_COLOR0, _TNL_ATTRIB_POS
	};
	int i;
	int slots=0;
	int total_size=0;

	nmesa->vertex_attr_count = 0;
	RENDERINPUTS_COPY(index, nmesa->render_inputs_bitset);

	/*
	 * Determine attribute sizes
	 */
	for(i=0;i<8;i++)
	{
		if (RENDERINPUTS_TEST(index, i))
			attr_size[i]=default_attr_size[i];
		else
			attr_size[i]=0;
	}
	for(i=8;i<16;i++)
	{
		if (RENDERINPUTS_TEST(index, i))
			attr_size[i]=VB->TexCoordPtr[i-8]->size;
		else
			attr_size[i]=0;
	}

	/*
	 * Tell t_vertex about the vertex format
	 */
	if ((nmesa->screen->card->type>=NV_10) && (nmesa->screen->card->type<=NV_17)) {
		for(i=0;i<8;i++) {
			int j = nv10_vtx_attribs[INV_VERT(i)];
			if (RENDERINPUTS_TEST(index, j)) {
				switch(attr_size[j])
				{
					case 1:
						EMIT_ATTR(j,EMIT_1F);
						break;
					case 2:
						EMIT_ATTR(j,EMIT_2F);
						break;
					case 3:
						EMIT_ATTR(j,EMIT_3F);
						break;
					case 4:
						EMIT_ATTR(j,EMIT_4F);
						break;
				}
				total_size+=attr_size[j];
			}
		}
	} else {
		for(i=0;i<16;i++)
		{
			if (RENDERINPUTS_TEST(index, i))
			{
				slots=i+1;
				switch(attr_size[i])
				{
					case 1:
						EMIT_ATTR(i,EMIT_1F);
						break;
					case 2:
						EMIT_ATTR(i,EMIT_2F);
						break;
					case 3:
						EMIT_ATTR(i,EMIT_3F);
						break;
					case 4:
						EMIT_ATTR(i,EMIT_4F);
						break;
				}
				if (i==_TNL_ATTRIB_COLOR0)
					nmesa->color_offset=total_size;
				if (i==_TNL_ATTRIB_COLOR1)
					nmesa->specular_offset=total_size;
				total_size+=attr_size[i];
			}
		}
	}

	nmesa->vertex_size=_tnl_install_attrs( ctx,
			nmesa->vertex_attrs, 
			nmesa->vertex_attr_count,
			NULL, 0 );
	/* OUT_RINGp wants size in DWORDS */
	nmesa->vertex_size = nmesa->vertex_size / 4;
	assert(nmesa->vertex_size==total_size);

	/* 
	 * Tell the hardware about the vertex format
	 */
	if ((nmesa->screen->card->type>=NV_10) && (nmesa->screen->card->type<=NV_17)) {
		int total_stride = 0;

#define NV_VERTEX_ATTRIBUTE_TYPE_FLOAT 2

		for(i=0;i<8;i++) {
			int j = nv10_vtx_attribs[i];
			int size;
			int stride = attr_size[j] << 2;
			if (j==_TNL_ATTRIB_POS) {
				stride += total_stride;
			}
			size = attr_size[j] << 4;
			size |= stride << 8;
			size |= NV_VERTEX_ATTRIBUTE_TYPE_FLOAT;
			BEGIN_RING_CACHE(NvSub3D, NV10_TCL_PRIMITIVE_3D_VERTEX_ATTR((7-i)),1);
			OUT_RING_CACHE(size);
			total_stride += stride;
		}

		BEGIN_RING_CACHE(NvSub3D, NV10_TCL_PRIMITIVE_3D_VERTEX_ARRAY_VALIDATE,1);
		OUT_RING_CACHE(0);
	} else if (nmesa->screen->card->type==NV_20) {
		for(i=0;i<16;i++)
		{
			int size=attr_size[i];
			BEGIN_RING_CACHE(NvSub3D,NV20_TCL_PRIMITIVE_3D_VERTEX_ATTR(i),1);
			OUT_RING_CACHE(NV_VERTEX_ATTRIBUTE_TYPE_FLOAT|(size*0x10));
		}
	} else {
		BEGIN_RING_SIZE(NvSub3D, NV30_TCL_PRIMITIVE_3D_DO_VERTICES, 1);
		OUT_RING(0);
		BEGIN_RING_CACHE(NvSub3D,NV20_TCL_PRIMITIVE_3D_VB_POINTER_ATTR8_TX0,slots);
		for(i=0;i<slots;i++)
		{
			int size=attr_size[i];
			OUT_RING_CACHE(NV_VERTEX_ATTRIBUTE_TYPE_FLOAT|(size*0x10));
		}
		// FIXME this is probably not needed
		BEGIN_RING_SIZE(NvSub3D,NV30_TCL_PRIMITIVE_3D_VERTEX_UNK_0,1);
		OUT_RING(0);
		BEGIN_RING_SIZE(NvSub3D,NV30_TCL_PRIMITIVE_3D_VERTEX_UNK_0,1);
		OUT_RING(0);
		BEGIN_RING_SIZE(NvSub3D,NV30_TCL_PRIMITIVE_3D_VERTEX_UNK_0,1);
		OUT_RING(0);
	}
}


static void nv10ChooseVertexState( GLcontext *ctx )
{
	struct nouveau_context *nmesa = NOUVEAU_CONTEXT(ctx);
	TNLcontext *tnl = TNL_CONTEXT(ctx);
	DECLARE_RENDERINPUTS(index);
	
	RENDERINPUTS_COPY(index, tnl->render_inputs_bitset);
	if (!RENDERINPUTS_EQUAL(index, nmesa->render_inputs_bitset))
	{
		RENDERINPUTS_COPY(nmesa->render_inputs_bitset, index);
		nv10OutputVertexFormat(nmesa);
	}

	if (nmesa->screen->card->type == NV_30) {
		nouveauShader *fp;
		
		if (ctx->FragmentProgram.Enabled) {
			fp = (nouveauShader *) ctx->FragmentProgram.Current;
			nvsUpdateShader(ctx, fp);
		} else
			nvsUpdateShader(ctx, nmesa->passthrough_fp);
	}

	if (nmesa->screen->card->type >= NV_40) {
		/* Ensure passthrough shader is being used, and mvp matrix
		 * is up to date
		 */
		nvsUpdateShader(ctx, nmesa->passthrough_vp);

		/* Update texenv shader / user fragprog */
		nvsUpdateShader(ctx, (nouveauShader*)ctx->FragmentProgram._Current);
	}
}


/**********************************************************************/
/*                 High level hooks for t_vb_render.c                 */
/**********************************************************************/


static void nv10RenderStart(GLcontext *ctx)
{
	TNLcontext *tnl = TNL_CONTEXT(ctx);
	struct nouveau_context *nmesa = NOUVEAU_CONTEXT(ctx);

	if (nmesa->new_state) {
		nmesa->new_render_state |= nmesa->new_state;
	}

	if (nmesa->new_render_state) {
		nv10ChooseVertexState(ctx);
		nv10ChooseRenderState(ctx);
		nmesa->new_render_state = 0;
	}
}

static void nv10RenderFinish(GLcontext *ctx)
{
}


/* System to flush dma and emit state changes based on the rasterized
 * primitive.
 */
void nv10RasterPrimitive(GLcontext *ctx,
		GLenum glprim,
		GLuint hwprim)
{
	struct nouveau_context *nmesa = NOUVEAU_CONTEXT(ctx);

	assert (!nmesa->new_state);

	if (hwprim != nmesa->current_primitive)
	{
		nmesa->current_primitive=hwprim;
		
	}
}

static const GLuint hw_prim[GL_POLYGON+1] = {
	GL_POINTS+1,
	GL_LINES+1,
	GL_LINE_STRIP+1,
	GL_LINE_LOOP+1,
	GL_TRIANGLES+1,
	GL_TRIANGLE_STRIP+1,
	GL_TRIANGLE_FAN+1,
	GL_QUADS+1,
	GL_QUAD_STRIP+1,
	GL_POLYGON+1
};

/* Callback for mesa:
 */
static void nv10RenderPrimitive( GLcontext *ctx, GLuint prim )
{
	nv10RasterPrimitive( ctx, prim, hw_prim[prim] );
}

static void nv10ResetLineStipple( GLcontext *ctx )
{
	/* FIXME do something here */
	WARN_ONCE("Unimplemented nv10ResetLineStipple\n");
}


/**********************************************************************/
/*                            Initialization.                         */
/**********************************************************************/

void nv10TriInitFunctions(GLcontext *ctx)
{
	struct nouveau_context *nmesa = NOUVEAU_CONTEXT(ctx);
	TNLcontext *tnl = TNL_CONTEXT(ctx);

	tnl->Driver.RunPipeline = nouveauRunPipeline;
	tnl->Driver.Render.Start = nv10RenderStart;
	tnl->Driver.Render.Finish = nv10RenderFinish;
	tnl->Driver.Render.PrimitiveNotify = nv10RenderPrimitive;
	tnl->Driver.Render.ResetLineStipple = nv10ResetLineStipple;
	tnl->Driver.Render.BuildVertices = _tnl_build_vertices;
	tnl->Driver.Render.CopyPV = _tnl_copy_pv;
	tnl->Driver.Render.Interp = _tnl_interp;

	_tnl_init_vertices( ctx, ctx->Const.MaxArrayLockSize + 12, 
			64 * sizeof(GLfloat) );

	nmesa->verts = (GLubyte *)tnl->clipspace.vertex_buf;
}


