#
# C++ linenoise
#
CXXFLAGS = -std=c++0x -Wall -W -g

all: linenoise_example keycodes ksm


example.o: linenoise.h
linenoise.o: linenoise.h

linenoise_example: linenoise.a example.o key_state_machine.o
	$(CXX) $(CXXFLAGS) -o linenoise_example example.o  ./linenoise.a

linenoise.a: linenoise.h linenoise.o string_fmt.o
	$(AR) rcs linenoise.a linenoise.o string_fmt.o

ksm: linenoise.a ksm.o key_state_machine.o
	$(CXX) $(CXXFLAGS) -o ksm ksm.o key_state_machine.o ./linenoise.a

keycodes: linenoise.a keycodes.o
	$(CXX) $(CXXFLAGS) -o keycodes keycodes.o key_state_machine.o ./linenoise.a

test: string_fmt.o
	$(CXX) $(CXXFLAGS) -D_TEST -o test_string_fmt string_fmt.cpp

clean:
	rm -f linenoise_example keycodes ksm *.o *.a
