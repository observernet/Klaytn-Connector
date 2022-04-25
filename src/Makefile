CSRC_HOME := $(shell if [ "$$PWD" != "" ]; then echo $$PWD; else pwd; fi)
export CSRC_HOME
 
include $(CSRC_HOME)/config.mk

all	:
	cd lib; make;
	cd manage; make;
	cd process; make
