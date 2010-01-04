
VASTsim_console = ../../bin/VASTsim_console

.PHONY: all noace

LIBS_COMMON = -lvastsim -lvast -lvastutil -lz
LIBS_ACE    = -lACE
ifeq ($(TARGET),noace)
    LIBS = $(LIBS_COMMON) 
else
    LIBS = $(LIBS_COMMON) $(LIBS_ACE)
endif

all: VASTsim_console.cpp
	g++ -Wall -I../../include -L../../lib -L../../zlib/lib $< $(LIBS) \
	-o $(VASTsim_console)

noace:
	make TARGET=noace

clean:
	rm -f $(VASTsim_console)
