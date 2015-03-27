#
# C++ linenoise
#
CXX_FLAGS = -std=c++0x -Wall -W -g

all: linenoise_example keycodes


linenoise_example: linenoise.a example.cpp
	$(CXX) $(CXX_FLAGS)  -o example.o -c example.cpp
	$(CXX)  -o linenoise_example example.o  ./linenoise.a

linenoise.a: linenoise.h linenoise.cpp string_fmt.cpp
	$(CXX) $(CXX_FLAGS) -o linenoise.o -c linenoise.cpp
	$(CXX) $(CXX_FLAGS) -o string_fmt.o -c string_fmt.cpp
	$(AR) rcs linenoise.a linenoise.o string_fmt.o

keycodes: linenoise.a keycodes.cpp
	$(CXX) $(CXX_FLAGS)  -o keycodes.o -c keycodes.cpp
	$(CXX)  -o keycodes keycodes.o ./linenoise.a

test: string_fmt.cpp
	$(CXX) $(CXX_FLAGS) -D_TEST -o test_string_fmt string_fmt.cpp

clean:
	rm -f linenoise_example *.o *.a
