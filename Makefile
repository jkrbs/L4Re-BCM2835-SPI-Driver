PKGDIR  ?= .
L4DIR   ?= ../l4re/src/l4

O=../l4re/obj/l4/arm64

TARGET          = spi
SRC_CC          = helper.cc bcm2835.cc main.cc
REQUIRES_LIBS   = libio
DEPENDS_PKGS    = $(REQUIRES_LIBS)
include $(L4DIR)/mk/prog.mk
