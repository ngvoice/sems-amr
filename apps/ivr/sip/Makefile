
COREPATH ?=../../../core

PY_VER = 2.3
SIP_FILES = $(wildcard *.sip)

.PHONY: all
all: py_ivr.so


.PHONY: clean
clean:
	$(MAKE) -f Makefile.gen clean; \
	rm -f *.cpp *.h *.sbf *~

.PHONY: sip_gen
sip_gen: configure.py $(SIP_FILES)
	python$(VER) configure.py

py_ivr.so: configure.py $(SIP_FILES)
	python$(VER) configure.py && \
	$(MAKE) -f Makefile.gen COREPATH=$(COREPATH)
