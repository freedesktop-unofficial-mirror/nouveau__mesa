# progs/demos/Makefile

TOP = ../..
include $(TOP)/configs/current

INCDIR = $(TOP)/include

LIB_DEP = $(TOP)/$(LIB_DIR)/$(GL_LIB_NAME) $(TOP)/$(LIB_DIR)/$(GLU_LIB_NAME) $(TOP)/$(LIB_DIR)/$(GLUT_LIB_NAME)

LIBS = -L$(TOP)/$(LIB_DIR) -l$(GLUT_LIB) -l$(GLU_LIB) -l$(GL_LIB) $(APP_LIB_DEPS)

PROGS = \
	bitmap \
	brick \
	bump \
	deriv \
	mandelbrot \
	noise \
	points \
	texdemo1 \
	toyball \
	trirast


##### RULES #####

.SUFFIXES:
.SUFFIXES: .c


# make executable from .c file:
.c: $(LIB_DEP)
	$(CC) -I$(INCDIR) $(CFLAGS) $(LDFLAGS) $< $(LIBS) -o $@


##### TARGETS #####

default: $(PROGS)



##### Extra dependencies

extfuncs.h:  $(TOP)/progs/util/extfuncs.h
	cp $< .

readtex.c: $(TOP)/progs/util/readtex.c
	cp $< .

readtex.h: $(TOP)/progs/util/readtex.h
	cp $< .

readtex.o: readtex.c readtex.h
	$(CC) -c -I$(INCDIR) $(CFLAGS) readtex.c

bitmap.c: extfuncs.h

brick.c: extfuncs.h

bump.c: extfuncs.h

mandelbrot.c: extfuncs.h

points.c: extfuncs.h

toyball.c: extfuncs.h

texdemo1: texdemo1.o readtex.o
	$(CC) -I$(INCDIR) $(CFLAGS) $(LDFLAGS) texdemo1.o readtex.o $(LIBS) -o $@

texdemo1.o: texdemo1.c readtex.h extfuncs.h
	$(CC) -c -I$(INCDIR) $(CFLAGS) texdemo1.c


clean:
	-rm -f $(PROGS)
	-rm -f *.o *~
	-rm -f extfuncs.h
