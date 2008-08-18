
LIBS     := -lvastatesim -lvastate -lvast -lvastutil -lz 
LIBS_ACE := -lACE
LIB_PATH := -I../../include -I../VASTATEsim -I../../VASTATE/include -I../../include \
			-L../../lib -L../../../zlib/lib -L.

ifeq ($(TARGET),noace)
	LIB := $(LIBS)
else
	LIB := $(LIBS) $(LIBS_ACE)
endif

VASTATESIM_console = ../../bin/VASTATEsim_console

.PHONY: all clean noace

all: VASTATEsim_console.cpp
	g++ -Wall -o $(VASTATESIM_console) $(LIB_PATH) $< $(LIB)

clean:
	$(if $(VASTATESIM_console), rm -f $(VASTATESIM_console))

noace:
	$(MAKE) TARGET=noace

