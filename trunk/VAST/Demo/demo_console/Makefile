
demo_console = ../../bin/demo_console

.PHONY: all noace

LIBS_COMMON = -lvastcommon -lvastsim -lvast -lvastnet  
#-lz -ldl -lpthread
LIBS_ACE    = -lACE 
LIBS_THREAD = -lpthread
LIBS_DL	    = -ldl

ACE_PATH    = ../../../ACE_wrappers

CFLAGS      = -Wall -static 

INC_PATHS   = -I../../common -I../../VASTsim -I$(ACE_PATH) 

LIB_PATHS   = -L../../lib -L../../Dependencies/lib/Release -L$(ACE_PATH)/lib -L$(ACE_PATH)/build/ace

LIBS = $(LIBS_COMMON) $(LIBS_ACE) $(LIBS_THREAD) $(LIBS_DL)

all: demo_console.cpp
	g++ $(CFLAGS) $(INC_PATHS) $(LIB_PATHS) $< $(LIBS) \
	-o $(demo_console)

noace:
	make TARGET=noace

clean:
	rm -f $(demo_console)
