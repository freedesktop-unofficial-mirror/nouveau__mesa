
TOP = ../../../../..
include $(TOP)/configs/current

LIBNAME = nouveau_dri.so

MINIGLX_SOURCES =

PIPE_DRIVERS = \
	$(TOP)/src/gallium/drivers/softpipe/libsoftpipe.a \
	$(TOP)/src/gallium/drivers/nv04/libnv04.a \
	$(TOP)/src/gallium/drivers/nv10/libnv10.a \
	$(TOP)/src/gallium/drivers/nv20/libnv20.a \
	$(TOP)/src/gallium/drivers/nv30/libnv30.a \
	$(TOP)/src/gallium/drivers/nv40/libnv40.a \
	$(TOP)/src/gallium/drivers/nv50/libnv50.a

DRIVER_SOURCES = \
	nouveau_context.c \
	nouveau_lock.c \
	nouveau_screen.c \
	nouveau_swapbuffers.c \
	nouveau_winsys.c \
	nouveau_winsys_pipe.c \
	nouveau_winsys_softpipe.c \
	nv04_surface.c \
	nv50_surface.c

C_SOURCES = \
	$(COMMON_GALLIUM_SOURCES) \
	$(DRIVER_SOURCES)

ASM_SOURCES = 

DRIVER_DEFINES = $(shell pkg-config --cflags libnouveau_drm)
DRI_LIB_DEPS += $(shell pkg-config --libs libnouveau_drm)

include ../Makefile.template

symlinks:
