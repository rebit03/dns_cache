CXXFLAGS = -std=c++1z -Wall -c

all: lib

cache.o: cache.cpp
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) cache.cpp

 # link library files into a static library
dns_cachelib.a: cache.o
	ar rcs dns_cachelib.a cache.o

lib: dns_cachelib.a

clean:
	rm -f cache *.o *.a *.gch
