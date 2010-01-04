
lib_vastate = ../lib/libvastate.a
AR = ar
CC = g++
CFLAGS = -static -c -Wall -I../common -I../VAST
#CFLAGS = -static -c -Wall -I../common -I../VAST

sources = AgentImpl.cpp AgentLogic.cpp ArbitratorImpl.cpp ArbitratorLogic.cpp VASTATE.cpp  VASTATETypes.cpp
 
objects = $(subst .cpp,.o,$(sources))

all: $(lib_vastate)	

# $@ refers $(lib_vast) and $^ refers to $(objects)
$(lib_vastate): $(objects)
	$(AR) rcs $@ $^
	
.cpp.o:
	$(CC) $(CFLAGS) $^

clean:
	$(if $(objects), rm -f $(objects))
	$(if $(lib_vastate), rm -f $(lib_vastate))

