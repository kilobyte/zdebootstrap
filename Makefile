ALL=zunpack

CXX=g++
CXXFLAGS=-Wall -g -O2 -pthread

all: $(ALL)

.cc.o:
	$(CXX) $(CXXFLAGS) -c $<

zunpack: deb.o util.o zunpack.o 822.o
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $^ -larchive

clean:
	rm -f $(ALL) *.o
