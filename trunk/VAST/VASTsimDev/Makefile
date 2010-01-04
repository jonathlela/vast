
lib_vastsim = ../../lib/libvastsim.a
sources = vastsim.cpp
objects = $(subst .cpp,.o,$(sources))

noace:
	make all

all: $(objects)
	$(AR) rcs $(lib_vastsim) $^

$(objects): $(sources)
	g++ -static -c -Wall -I../../include $<

clean: 
	rm -f *.o
	rm -f $(lib_vastsim)
