

lib_vastnet = ../lib/libvastnet.a
AR = ar
CC = g++

ACE_ROOT = ../../ACE_wrappers

CFLAGS = -static -c -Wall -I../common -I../../ACE_wrappers
#CFLAGS = -fPIC -c -Wall -I../common -I$(ACE_ROOT)

#Note that ACE path is not included here, as we assume it's already properly installed on the system

sources_o = MessageHandler.cpp MessageQueue.cpp net_emu.cpp net_emubridge.cpp \
            net_emu_bl.cpp net_emubridge_bl.cpp VASTnet.cpp
sources_ace = net_ace.cpp net_ace_handler.cpp

ifeq ($(TARGET),noace)
  sources=$(sources_o)
else
  sources=$(sources_o) $(sources_ace)
endif

objects = $(subst .cpp,.o,$(sources))

all: $(lib_vastnet)	

# $@ refers $(lib_vast) and $^ refers to $(objects)
$(lib_vastnet): $(objects)
	$(AR) rcs $@ $^
	
.cpp.o:
	$(CC) $(CFLAGS) $^

clean:
	$(if $(objects), rm -f $(objects))
	$(if $(lib_vastnet), rm -f $(lib_vastnet))

noace:
	make TARGET=noace

