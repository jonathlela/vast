
VASTATEsim_console = ../../bin/VASTATEsim_console

.PHONY: all noace

LIBS_COMMON = -lvastatesim -lvastsim -lvastate -lvast -lvastnet -lvastcommon -lz
LIBS_ACE    = -lACE

ifeq ($(TARGET),noace)
    LIBS = $(LIBS_COMMON) 
else
    LIBS = $(LIBS_COMMON) $(LIBS_ACE)
endif

all: VASTATEsim_console.cpp
	g++ -Wall -I../../common -I../../VASTsim -I../../VASTATEsim -L../../lib -L../../Dependencies/lib/Release -L../../../ACE_wrappers/build/ace/.libs $< $(LIBS) \
	-o $(VASTATEsim_console)

noace:
	make TARGET=noace

clean:
	rm -f $(VASTATEsim_console)
