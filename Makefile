rwildcard=$(wildcard $1$2) $(foreach d,$(wildcard $1*),$(call rwildcard,$d/,$2))

MAKE=make
CC=ccache gcc
CXX=ccache g++
LD=g++ -fuse-ld=mold
RM=rm -f
GLSLC=glslc


VERT_SOURCES=$(call rwildcard,shaders,*.vert)
FRAG_SOURCES=$(call rwildcard,shaders,*.frag)

C_SOURCES=$(call rwildcard,src,*.c)
CXX_SOURCES=$(call rwildcard,src,*.cpp)

DEFINES=-DGLM_FORCE_LEFT_HANDED -DGLM_FORCE_DEPTH_ZERO_TO_ONE
CFLAGS=-c -I./src/deps/include -Wall -Wno-strict-aliasing -Wno-interference-size -g -O2 $(DEFINES)
CXXFLAGS=$(CFLAGS) -std=c++17
LDFLAGS=-lSDL2 -lSDL2main -lvulkan -latomic


SPVFILES=$(VERT_SOURCES:.vert=.vert.spv) $(FRAG_SOURCES:.frag=.frag.spv)
OBJECTS=$(C_SOURCES:.c=.o) $(CXX_SOURCES:.cpp=.o)
EXECUTABLE=vulkantest

ifeq ($(OS),Windows_NT)
	EXECUTABLE=$(EXECUTABLE).exe
endif

.PHONY: all
all: rebuild
	nice -20 ./$(EXECUTABLE)

.PHONY: rebuild
rebuild: clean
	$(MAKE) build

.PHONY: build
build: $(EXECUTABLE) $(SPVFILES)

.PHONY: clean
clean:
	find -name '*.o' | xargs $(RM)
	find -name '*.spv' | xargs $(RM)
	$(RM) $(EXECUTABLE)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -o $@ $<

%.o: %.c
	$(CC) $(CFLAGS) -o $@ $<

$(EXECUTABLE): $(OBJECTS)
	$(LD) $(OBJECTS) -o $@ $(LDFLAGS)

%.vert.spv: %.vert
	$(GLSLC) $< -o $@

%.frag.spv: %.frag
	$(GLSLC) $< -o $@
