CSRC_HOME := $(shell if [ "$$CSRC_HOME" != "" ]; then echo $$CSRC_HOME; else echo "$$PWD/.."; fi)
include $(CSRC_HOME)/config.mk

CLIBS	=	ft_common
PCLIBS	=	ora_common
OBJS	=	ft_common.o ora_common.o

all	:	$(CLIBS) \
		$(PCLIBS) \
		install	\
		clean

$(CLIBS) :	$@
		$(CC) $(COMP) -c $@.c

$(PCLIBS) :	$@
		$(PROC) $(PFLAGS) iname=$@
		$(CC) $(COMP) $(ORA_COMP) -c $@.c
		rm -f $@.c

install	:
		cp *.h ../../include/lib
		ar rcv ../../lib/libKASConn.a $(OBJS)

clean   :
		rm -f *.o *.lis tp*
