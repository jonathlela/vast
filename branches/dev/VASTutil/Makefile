
lib_vastutil = ../lib/libvastutil.a
AR = ar
CC = g++
CFLAGS = -fPIC -c -Wall -I../include -I../zlib/include

sources = Compressor.cpp \
		  errout.cpp \
		  MovementGenerator.cpp \
		  SectionedFile.cpp \
		  StdIO_SectionedFile.cpp


objects = $(subst .cpp,.o,$(sources))

all: $(lib_vastutil)	

# $@ refers $(lib_vast) and $^ refers to $(objects)
$(lib_vastutil): $(objects)
	$(AR) rcs $@ $^
	
.cpp.o:
	$(CC) $(CFLAGS) $^

clean:
	$(if $(objects), rm -f $(objects))
	$(if $(lib_vastutil), rm -f $(lib_vastutil))

noace:
	make TARGET=noace
