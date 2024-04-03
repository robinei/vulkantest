rwildcard=$(wildcard $1$2) $(foreach d,$(wildcard $1*),$(call rwildcard,$d/,$2))

#USE_GCC=1
MAKE=make
RM=rm -f
GLSLC=glslc
EXECUTABLE=vulkantest
DEFINES=-DGLM_FORCE_LEFT_HANDED -DGLM_FORCE_DEPTH_ZERO_TO_ONE -DGLM_FORCE_INTRINSICS
WARNINGS=-Wall -Wno-strict-aliasing -Wno-unknown-pragmas

ifdef USE_GCC
	CC=ccache gcc
	CXX=ccache g++
	LD=g++ -fuse-ld=mold
	WARNINGS+= -Wno-interference-size
else
	CC=ccache clang
	CXX=ccache clang++
	LD=clang++ -fuse-ld=mold
	WARNINGS+= -Wno-unused-private-field
endif

CFLAGS=-c -g -O2 -I./src/deps/include $(DEFINES) $(WARNINGS)
CXXFLAGS=$(CFLAGS) -std=c++17
LDFLAGS=-lm -latomic -lSDL2 -lSDL2main -lvulkan


ifeq ($(OS),Windows_NT)
	EXECUTABLE=$(EXECUTABLE).exe
endif


VERT_SOURCES=$(call rwildcard,assets/shaders,*.vert)
FRAG_SOURCES=$(call rwildcard,assets/shaders,*.frag)
SPVFILES=$(VERT_SOURCES:.vert=.vert.spv) $(FRAG_SOURCES:.frag=.frag.spv)

C_SOURCES=$(call rwildcard,src,*.c)
CXX_SOURCES=$(call rwildcard,src,*.cpp)
OBJECTS=$(C_SOURCES:.c=.o) $(CXX_SOURCES:.cpp=.o)


.PHONY: all
all: rebuild
	./$(EXECUTABLE)

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
