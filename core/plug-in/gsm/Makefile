COREPATH =../..

plug_in_name = gsm

SPANDSP_INC=
#/usr/include/
SPANDSP_LIB = -lspandsp

module_ldflags = $(SPANDSP_LIB)
module_cflags  = -ansi # -DNOFPU

ifdef NOFPU
	module_cflags += -DNOFPU
endif

