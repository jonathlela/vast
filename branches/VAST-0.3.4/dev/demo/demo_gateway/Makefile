
gateway = ../../bin/gateway

all: gateway.cpp
	g++ -Wall -I../../include -L../../lib $< -lvast -lACE -lz \
	-o $(gateway)

clean: 
	rm $(gateway)
