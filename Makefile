ALL=zunpack

CXX=g++
CXXFLAGS=-Wall -g -O2

all: $(ALL)

.cc.o:
	$(CXX) $(CXXFLAGS) -c $<

zunpack: deb_ar.o deb_control.o deb_data.o zunpack.o
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $^ -larchive

clean:
	rm -f $(ALL) *.o
