#ifndef _NOUVEAU_DRI_
#define _NOUVEAU_DRI_

#include "xf86drm.h"
#include "drm.h"
#include "nouveau_drm.h"

struct nouveau_dri {
	uint32_t device_id;	/**< \brief PCI device ID */
	uint32_t width;		/**< \brief width in pixels of display */
	uint32_t height;	/**< \brief height in scanlines of display */
	uint32_t depth;		/**< \brief depth of display (8, 15, 16, 24) */
	uint32_t bpp;		/**< \brief bit depth of display (8, 16, 24, 32) */

	uint32_t bus_type;	/**< \brief ths bus type */
	uint32_t bus_mode;	/**< \brief bus mode (used for AGP, maybe also for PCI-E ?) */

	uint32_t front_handle;
	uint32_t front_pitch;
};

#endif
