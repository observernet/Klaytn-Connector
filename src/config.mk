CC	=	gcc
PROC	=	proc

DEFINES	=	-D_DEBUG

SCLIB_PATH = $(HOME)/LIB
PROG_PATH = $(HOME)/KASConn

LIB_PATH = $(PROG_PATH)/lib
INSTALL_PATH = $(PROG_PATH)/bin

PFLAGS	=	ltype=long \
			lines=yes \
			include=$(PROG_PATH)/include/lib \
			include=$(PROG_PATH)/include/io \
			include=$(PROG_PATH)/include \
			include=$(SCLIB_PATH)/include \
			include=$(SCLIB_PATH)/common \
			sqlcheck=syntax \
			NLS_LOCAL=YES

COMP =	-Wall -Wno-pointer-sign -Wno-deprecated-declarations -Wno-implicit-function-declaration -c -g -fPIC $(DEFINES) \
		-I. \
		-I/usr/include/libxml2/ \
		-I$(PROG_PATH)/include/lib \
		-I$(PROG_PATH)/include/io \
		-I$(PROG_PATH)/include \
		-I$(SCLIB_PATH)/include \
		-I$(SCLIB_PATH)/common

ORA_COMP = 	-I$(ORACLE_HOME)/precomp/public \
			-I$(ORACLE_HOME)/rdbms/public \
			-I$(ORACLE_HOME)/rdbms/demo \
			-I$(ORACLE_HOME)/plsql/public \
			-I$(ORACLE_HOME)/network/admin
