all: example_concurrent example_io

headers=sesstypes.hh concurrentmedium.hh

example_concurrent: $(headers) example_concurrent.cc
	$(CXX) -std=c++2b example_concurrent.cc -o example_concurrent

example_io: $(headers) example_io.cc
	$(CXX) -std=c++2b example_io.cc -o example_io

clean:
	rm -f example_concurrent example_io
