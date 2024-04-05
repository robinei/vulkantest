rwildcard=$(wildcard $1$2) $(foreach d,$(wildcard $1*),$(call rwildcard,$d/,$2))

#USE_GCC=1
MAKE=make
RM=rm -f
GLSLC=glslc
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

EXE:=
ifeq ($(OS),Windows_NT)
	EXE:=.exe
endif

CFLAGS=-c -g -O2 -I./3rdparty/include $(DEFINES) $(WARNINGS)
CXXFLAGS=$(CFLAGS) -std=c++17
LDFLAGS=-lm -latomic -lSDL2 -lSDL2main -lvulkan


GAME_TARGET=vulkantest$(EXE)

GAME_C_SOURCES=$(call rwildcard,src,*.c)
GAME_CXX_SOURCES=$(call rwildcard,src,*.cpp)
GAME_OBJECTS=$(GAME_C_SOURCES:.c=.o) $(GAME_CXX_SOURCES:.cpp=.o)

DEPS_C_SOURCES=$(call rwildcard,3rdparty/sources,*.c)
DEPS_CXX_SOURCES=$(call rwildcard,3rdparty/sources,*.cpp)
DEPS_OBJECTS=$(DEPS_C_SOURCES:.c=.o) $(DEPS_CXX_SOURCES:.cpp=.o)

ASSET_VERT_SOURCES=$(call rwildcard,assets/shaders,*.vert)
ASSET_FRAG_SOURCES=$(call rwildcard,assets/shaders,*.frag)
ASSET_SHADERS=$(ASSET_VERT_SOURCES:.vert=.vert.spv) $(ASSET_FRAG_SOURCES:.frag=.frag.spv)

.PHONY: all
all: rebuild
	./$(GAME_TARGET)

.PHONY: rebuild
rebuild: clean
	$(MAKE) build

.PHONY: build
build: $(GAME_TARGET) $(ASSET_SHADERS)

.PHONY: clean
clean:
	@echo "Cleaning"
	@find -name '*.o' | xargs $(RM)
	@find -name '*.spv' | xargs $(RM)
	@$(RM) $(GAME_TARGET)

%.o: %.cpp
	@echo "Compiling $@"
	@$(CXX) $(CXXFLAGS) -o $@ $<

%.o: %.c
	@echo "Compiling $@"
	@$(CC) $(CFLAGS) -o $@ $<

$(GAME_TARGET): $(GAME_OBJECTS) $(DEPS_OBJECTS)
	@echo "Linking $@"
	@$(LD) $^ -o $@ $(LDFLAGS)

%.vert.spv: %.vert
	@echo "Compiling $@"
	@$(GLSLC) $< -o $@

%.frag.spv: %.frag
	@echo "Compiling $@"
	@$(GLSLC) $< -o $@
