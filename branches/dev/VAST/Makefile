
lib_vast = ../lib/libvast.a
AR = ar
CC = g++
CFLAGS = -fPIC -c -Wall -I../include/ -I../ACE_wrappers/ace/ -I../zlib/include/

sources_o = msghandler.cpp net_emubridge.cpp vor_SF_algorithm.cpp net_emu_bl.cpp net_emu.cpp vastid_base.cpp vor_SF.cpp \
 net_emubridge_bl.cpp vast_dc.cpp vastverse.cpp vast_mc.cpp

#sources_o = net_emu.cpp net_emubridge.cpp vast_dc.cpp vastverse.cpp vor_SF.cpp vor_SF_algorithm.cpp 
sources_ace = net_ace.cpp net_ace_handler.cpp
ifeq ($(TARGET),noace)
  sources=$(sources_o)
else
  sources=$(sources_o) $(sources_ace)
endif

objects = $(subst .cpp,.o,$(sources))

all: $(lib_vast)	

# $@ refers $(lib_vast) and $^ refers to $(objects)
$(lib_vast): $(objects)
	$(AR) rcs $@ $^
	
.cpp.o:
	$(CC) $(CFLAGS) $^

clean:
	$(if $(objects), rm -f $(objects))
	$(if $(lib_vast), rm -f $(lib_vast))

noace:
	make TARGET=noace
