#
# Makefile for creating the VirtualTaj demo.
# (Use this with GNU Make 3.77 or better.)
#
# Author: Ranjit Mathew (rmathew@gmail.com)
#

# Verify these values.
CC=gcc
SDL_DIR=/usr

CFLAGS=-I$(SDL_DIR)/include/SDL \
	-O3 -pipe -finline-functions -march=pentium3 -mtune=pentium3 \
	-msse -mfpmath=sse -fomit-frame-pointer -Wall -W \
	-DVTAJ_DEBUG -DBSPC_DEBUG -DOBJ3D_DEBUG -DGLD_DEBUG

GFX_LIBS=-L$(SDL_DIR)/lib -lSDL_image -lSDL -lGLU -lGL
LFLAGS=$(GFX_LIBS) -lm

VPATH=src:.

VTAJ_OBJS= \
	vtaj.o \
	gld.o \
	coldet.o \
	bspc.o \

GLD2BSP_OBJS= \
	gld2bsp.o \
	gld.o \
	bspc.o \

OBJ2GLD_OBJS= \
	obj2gld.o \
	obj3d.o \
	gld.o \

OBJS= \
	$(GLD2BSP_OBJS) \
	$(OBJ2GLD_OBJS) \
	$(VTAJ_OBJS) \

BIN_DIR=.

VTAJ_PROG=$(BIN_DIR)/vtaj
OBJ2GLD_PROG=$(BIN_DIR)/obj2gld
GLD2BSP_PROG=$(BIN_DIR)/gld2bsp

PROGS=\
	$(OBJ2GLD_PROG) \
	$(GLD2BSP_PROG) \
	$(VTAJ_PROG) \

MDL_DIR=models

INT_MDL=$(MDL_DIR)/internals
EXT_MDL=$(MDL_DIR)/externals
CX_INT_MDL=$(MDL_DIR)/cx_int
CX_EXT_MDL=$(MDL_DIR)/cx_ext

GLDS=\
	$(INT_MDL).gld \
	$(EXT_MDL).gld \
	$(CX_INT_MDL).gld \
	$(CX_EXT_MDL).gld \

BSPS=\
	$(INT_MDL).bsp \
	$(EXT_MDL).bsp \
	$(CX_INT_MDL).bsp \
	$(CX_EXT_MDL).bsp \


.PHONY: all clean run genbsp

SUFFIXES=.gld .bsp .obj .mtl

%.bsp: %.gld
	$(GLD2BSP_PROG) $< $@

all: $(PROGS) $(GLDS)

run: $(VTAJ_PROG) $(GLDS)
	$(VTAJ_PROG) -w -8

genbsp: $(GLD2BSP_PROG) $(GLDS) $(BSPS)

$(INT_MDL).gld: $(OBJ2GLD_PROG) $(INT_MDL).obj $(INT_MDL).mtl
	$(OBJ2GLD_PROG) $(INT_MDL).obj $(INT_MDL).mtl $(INT_MDL).gld

$(EXT_MDL).gld: $(OBJ2GLD_PROG) $(EXT_MDL).obj $(EXT_MDL).mtl
	$(OBJ2GLD_PROG) $(EXT_MDL).obj $(EXT_MDL).mtl $(EXT_MDL).gld

$(CX_INT_MDL).gld: $(OBJ2GLD_PROG) $(CX_INT_MDL).obj $(INT_MDL).mtl
	$(OBJ2GLD_PROG) $(CX_INT_MDL).obj $(INT_MDL).mtl $(CX_INT_MDL).gld

$(CX_EXT_MDL).gld: $(OBJ2GLD_PROG) $(CX_EXT_MDL).obj $(EXT_MDL).mtl
	$(OBJ2GLD_PROG) $(CX_EXT_MDL).obj $(EXT_MDL).mtl $(CX_EXT_MDL).gld

clean:
	rm -f $(PROGS)
	rm -f $(OBJS)
	rm -f $(GLDS)
	rm -f $(BSPS)

$(VTAJ_PROG): $(VTAJ_OBJS)
	$(CC) $(CFLAGS) -o $(VTAJ_PROG) $(VTAJ_OBJS) $(LFLAGS)

$(GLD2BSP_PROG): $(GLD2BSP_OBJS)
	$(CC) $(CFLAGS) -o $(GLD2BSP_PROG) $(GLD2BSP_OBJS) $(LFLAGS)

$(OBJ2GLD_PROG): $(OBJ2GLD_OBJS)
	$(CC) $(CFLAGS) -o $(OBJ2GLD_PROG) $(OBJ2GLD_OBJS) $(LFLAGS)
