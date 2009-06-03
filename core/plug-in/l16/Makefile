COREPATH =../..
plug_in_name = l16

module_ldflags = 
# if you want support for L16 stereo:
#  module_cflags = -DSUPPORT_STEREO
module_cflags  = 

ifeq ($(OS),solaris)
# For ntohs and friends.
	module_ldflags += -lsocket -lnsl
endif

include ../Makefile.audio_module
