#include "pipe/p_screen.h"
#include "pipe/p_util.h"

#include "nv10_context.h"
#include "nv10_screen.h"

static const char *
nv10_screen_get_name(struct pipe_screen *screen)
{
	struct nv10_screen *nv10screen = nv10_screen(screen);
	static char buffer[128];

	snprintf(buffer, sizeof(buffer), "NV%02X", nv10screen->chipset);
	return buffer;
}

static const char *
nv10_screen_get_vendor(struct pipe_screen *screen)
{
	return "nouveau";
}

static int
nv10_screen_get_param(struct pipe_screen *screen, int param)
{
	switch (param) {
	case PIPE_CAP_MAX_TEXTURE_IMAGE_UNITS:
		return 2;
	case PIPE_CAP_NPOT_TEXTURES:
		return 0;
	case PIPE_CAP_TWO_SIDED_STENCIL:
		return 0;
	case PIPE_CAP_GLSL:
		return 0;
	case PIPE_CAP_S3TC:
		return 0;
	case PIPE_CAP_ANISOTROPIC_FILTER:
		return 1;
	case PIPE_CAP_POINT_SPRITE:
		return 0;
	case PIPE_CAP_MAX_RENDER_TARGETS:
		return 1;
	case PIPE_CAP_OCCLUSION_QUERY:
		return 0;
	case PIPE_CAP_TEXTURE_SHADOW_MAP:
		return 0;
	case PIPE_CAP_MAX_TEXTURE_2D_LEVELS:
		return 12;
	case PIPE_CAP_MAX_TEXTURE_3D_LEVELS:
		return 0;
	case PIPE_CAP_MAX_TEXTURE_CUBE_LEVELS:
		return 12;
	case NOUVEAU_CAP_HW_VTXBUF:
	case NOUVEAU_CAP_HW_IDXBUF:
		return 0;
	default:
		NOUVEAU_ERR("Unknown PIPE_CAP %d\n", param);
		return 0;
	}
}

static float
nv10_screen_get_paramf(struct pipe_screen *screen, int param)
{
	switch (param) {
	case PIPE_CAP_MAX_LINE_WIDTH:
	case PIPE_CAP_MAX_LINE_WIDTH_AA:
		return 10.0;
	case PIPE_CAP_MAX_POINT_WIDTH:
	case PIPE_CAP_MAX_POINT_WIDTH_AA:
		return 64.0;
	case PIPE_CAP_MAX_TEXTURE_ANISOTROPY:
		return 2.0;
	case PIPE_CAP_MAX_TEXTURE_LOD_BIAS:
		return 4.0;
	case PIPE_CAP_BITMAP_TEXCOORD_BIAS:
		return 0.0;
	default:
		NOUVEAU_ERR("Unknown PIPE_CAP %d\n", param);
		return 0.0;
	}
}

static boolean
nv10_screen_is_format_supported(struct pipe_screen *screen,
				enum pipe_format format, uint type)
{
	switch (type) {
	case PIPE_SURFACE:
		switch (format) {
		case PIPE_FORMAT_A8R8G8B8_UNORM:
		case PIPE_FORMAT_R5G6B5_UNORM: 
		case PIPE_FORMAT_Z24S8_UNORM:
		case PIPE_FORMAT_Z16_UNORM:
			return TRUE;
		default:
			break;
		}
		break;
	case PIPE_TEXTURE:
		switch (format) {
		case PIPE_FORMAT_A8R8G8B8_UNORM:
		case PIPE_FORMAT_A1R5G5B5_UNORM:
		case PIPE_FORMAT_A4R4G4B4_UNORM:
		case PIPE_FORMAT_R5G6B5_UNORM: 
		case PIPE_FORMAT_U_L8:
		case PIPE_FORMAT_U_A8:
		case PIPE_FORMAT_U_I8:
			return TRUE;
		default:
			break;
		}
		break;
	default:
		assert(0);
	};

	return FALSE;
}

static void
nv10_screen_destroy(struct pipe_screen *pscreen)
{
	struct nv10_screen *screen = nv10_screen(pscreen);
	struct nouveau_winsys *nvws = screen->nvws;

	nvws->notifier_free(&screen->sync);
	nvws->grobj_free(&screen->celsius);

	FREE(pscreen);
}

struct pipe_screen *
nv10_screen_create(struct pipe_winsys *ws, struct nouveau_winsys *nvws,
		   unsigned chipset)
{
	struct nv10_screen *screen = CALLOC_STRUCT(nv10_screen);
	unsigned celsius_class;
	int ret;

	if (!screen)
		return NULL;
	screen->chipset = chipset;
	screen->nvws = nvws;

	/* 3D object */
	if (chipset>=0x20)
		celsius_class=NV11TCL;
	else if (chipset>=0x17)
		celsius_class=NV17TCL;
	else if (chipset>=0x11)
		celsius_class=NV11TCL;
	else
		celsius_class=NV10TCL;

	if (!celsius_class) {
		NOUVEAU_ERR("Unknown nv1x chipset: nv%02x\n", chipset);
		return NULL;
	}

	ret = nvws->grobj_alloc(nvws, celsius_class, &screen->celsius);
	if (ret) {
		NOUVEAU_ERR("Error creating 3D object: %d\n", ret);
		return FALSE;
	}

	/* Notifier for sync purposes */
	ret = nvws->notifier_alloc(nvws, 1, &screen->sync);
	if (ret) {
		NOUVEAU_ERR("Error creating notifier object: %d\n", ret);
		nv10_screen_destroy(&screen->pipe);
		return NULL;
	}

	screen->pipe.winsys = ws;
	screen->pipe.destroy = nv10_screen_destroy;

	screen->pipe.get_name = nv10_screen_get_name;
	screen->pipe.get_vendor = nv10_screen_get_vendor;
	screen->pipe.get_param = nv10_screen_get_param;
	screen->pipe.get_paramf = nv10_screen_get_paramf;

	screen->pipe.is_format_supported = nv10_screen_is_format_supported;

	nv10_screen_init_miptree_functions(&screen->pipe);

	return &screen->pipe;
}
