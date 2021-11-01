PKGDIR  ?= .
L4DIR   ?= /home/repos/l4re-snapshot-21.07.0/src/l4

O=/home/repos/l4re-snapshot-21.07.0/obj/l4/arm64

TARGET          = spi
SRC_CC          = helper.cc bcm2835.cc main.cc
REQUIRES_LIBS   = libio
DEPENDS_PKGS    = $(REQUIRES_LIBS)
include $(L4DIR)/mk/prog.mk
