rwildcard=$(wildcard $1$2) $(foreach d,$(wildcard $1*),$(call rwildcard,$d/,$2))

MAKE=make
CC=ccache gcc
CXX=ccache g++
LD=g++
RM=rm -f

EXCLUDE=
C_SOURCES=$(filter-out $(EXCLUDE),$(call rwildcard,src,*.c))
CXX_SOURCES=$(filter-out $(EXCLUDE),$(call rwildcard,src,*.cpp))

CFLAGS=-c -I./src/deps/include -g
CXXFLAGS=$(CFLAGS) -std=c++17
LDFLAGS=-lrt -lpthread -ldl -lSDL2 -lSDLmain -lvulkan


OBJECTS=$(C_SOURCES:.c=.o) $(CXX_SOURCES:.cpp=.o)
EXECUTABLE=cppgame

.PHONY: all
all: rebuild
	./$(EXECUTABLE)

.PHONY: rebuild
rebuild : clean
	$(MAKE) build

.PHONY: build
build: $(C_SOURCES) $(CXX_SOURCES) $(EXECUTABLE)

.PHONY: clean
clean:
	find -name '*.o' | xargs $(RM)
	$(RM) $(EXECUTABLE)
	$(RM) $(EXECUTABLE).exe

.cpp.o:
	$(CXX) $(CXXFLAGS) -o $@ $<

.c.o:
	$(CC) $(CFLAGS) -o $@ $<

$(EXECUTABLE): $(OBJECTS)
	$(LD) $(OBJECTS) -o $@ $(LDFLAGS)
