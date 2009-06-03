plug_in_name = jack

module_ldflags = $(shell pkg-config --cflags --libs jack)
module_cflags  = 

extra_install = $(plug_in_name)_audio

COREPATH ?=../../core
include $(COREPATH)/plug-in/Makefile.app_module
