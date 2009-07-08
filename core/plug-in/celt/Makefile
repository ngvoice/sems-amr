plug_in_name = celt
COREPATH =../..

CELT_INC=
#/usr/include/
CELT_LIB = -lcelt

# or: 
# CELTDIR  ?= celt_src
# CELT_INC ?= -I $(CELTDIR)
# CELT_LIB ?= $(CELTDIR)/libcelt/.libs/libcelt.a
#
# CONFIGURE_FLAG=--with-pic


module_ldflags = $(CELT_LIB)
module_cflags  = -ansi # -DNOFPU


include ../Makefile.audio_module
