/*
 * Copyright © 2007 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 *
 * Authors:
 *    Eric Anholt <eric@anholt.net>
 *
 */

#define PCI_CHIP_I810			0x7121
#define PCI_CHIP_I810_DC100		0x7123
#define PCI_CHIP_I810_E			0x7125
#define PCI_CHIP_I815			0x1132

#define PCI_CHIP_I830_M			0x3577
#define PCI_CHIP_845_G			0x2562
#define PCI_CHIP_I855_GM		0x3582
#define PCI_CHIP_I865_G			0x2572

#define PCI_CHIP_I915_G			0x2582
#define PCI_CHIP_I915_GM		0x2592
#define PCI_CHIP_I945_G			0x2772
#define PCI_CHIP_I945_GM		0x27A2
#define PCI_CHIP_I945_GME		0x27AE

#define PCI_CHIP_Q35_G			0x29B2
#define PCI_CHIP_G33_G			0x29C2
#define PCI_CHIP_Q33_G			0x29D2

#define PCI_CHIP_I965_G			0x29A2
#define PCI_CHIP_I965_Q			0x2992
#define PCI_CHIP_I965_G_1		0x2982
#define PCI_CHIP_I946_GZ		0x2972
#define PCI_CHIP_I965_GM                0x2A02
#define PCI_CHIP_I965_GME               0x2A12

#define IS_MOBILE(devid)	(devid == PCI_CHIP_I855_GM || \
				 devid == PCI_CHIP_I915_GM || \
				 devid == PCI_CHIP_I945_GM || \
				 devid == PCI_CHIP_I945_GME || \
				 devid == PCI_CHIP_I965_GM || \
				 devid == PCI_CHIP_I965_GME)

#define IS_965(devid)		(devid == PCI_CHIP_I965_G || \
				 devid == PCI_CHIP_I965_Q || \
				 devid == PCI_CHIP_I965_G_1 || \
				 devid == PCI_CHIP_I965_GM || \
				 devid == PCI_CHIP_I965_GME || \
				 devid == PCI_CHIP_I946_GZ)

#define IS_9XX(devid)		(devid == PCI_CHIP_I915_G || \
				 devid == PCI_CHIP_I915_GM || \
				 devid == PCI_CHIP_I945_G || \
				 devid == PCI_CHIP_I945_GM || \
				 devid == PCI_CHIP_I945_GME || \
				 devid == PCI_CHIP_G33_G || \
				 devid == PCI_CHIP_Q35_G || \
				 devid == PCI_CHIP_Q33_G || \
				 IS_965(devid))

