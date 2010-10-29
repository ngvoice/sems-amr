plug_in_name = callcenter

module_ldflags = -lmysqlpp
module_cflags  = -DMOD_NAME=\"$(plug_in_name)\" -DUSE_MYSQL -I/usr/local/include/mysql++ -I/usr/include/mysql

COREPATH ?=../../core
include $(COREPATH)/plug-in/Makefile.app_module
