rwildcard=$(wildcard $1$2) $(foreach d,$(wildcard $1*),$(call rwildcard,$d/,$2))

USE_GCC=1
#USE_ASAN=1
#USE_MOLD=1

MAKE=make
RM=rm -f
GLSLC=glslc
DEFINES=-DGLM_FORCE_LEFT_HANDED -DGLM_FORCE_DEPTH_ZERO_TO_ONE -DGLM_FORCE_INTRINSICS

CFLAGS=-c -g -O2 -I./3rdparty/include $(DEFINES) -Wall -Wno-strict-aliasing -Wno-unknown-pragmas
CXXFLAGS=-std=c++20 $(CFLAGS)
LDFLAGS0=
LDFLAGS=$(LDFLAGS0) -lm -latomic -lSDL2main -lSDL2

ifdef USE_GCC
	CC=ccache gcc
	CXX=ccache g++
	LD=g++
	CXXFLAGS+= -Wno-interference-size
else
	CC=ccache clang
	CXX=ccache clang++
	LD=clang++
	CXXFLAGS+= -Wno-unused-private-field
endif

ifdef USE_MOLD
	LD += -fuse-ld=mold
endif

ifdef USE_ASAN
	CFLAGS += -fsanitize=address
	LDFLAGS += -fsanitize=address
endif

EXE_EXTENSION:=
ifeq ($(OS),Windows_NT)
	EXE_EXTENSION:=.exe
	LDFLAGS += -mwindows
	LDFLAGS0 += -lmingw32
	DEFINES += -DVK_USE_PLATFORM_WIN32_KHR
else
	DEFINES += -DVK_USE_PLATFORM_XLIB_KHR
endif

GAME_TARGET=vulkantest$(EXE_EXTENSION)

GAME_C_SOURCES=$(call rwildcard,src,*.c)
GAME_CXX_SOURCES=$(call rwildcard,src,*.cpp)
GAME_OBJECTS=$(GAME_C_SOURCES:.c=.o) $(GAME_CXX_SOURCES:.cpp=.o)

DEPS_C_SOURCES=$(call rwildcard,3rdparty/sources,*.c)
DEPS_CXX_SOURCES=$(call rwildcard,3rdparty/sources,*.cpp)
DEPS_OBJECTS=$(DEPS_C_SOURCES:.c=.o) $(DEPS_CXX_SOURCES:.cpp=.o)

ASSET_VERT_SOURCES=$(call rwildcard,assets/shaders,*.vert)
ASSET_FRAG_SOURCES=$(call rwildcard,assets/shaders,*.frag)
ASSET_SHADERS=$(ASSET_VERT_SOURCES:.vert=.vert.spv) $(ASSET_FRAG_SOURCES:.frag=.frag.spv)

#export ASAN_OPTIONS=fast_unwind_on_malloc=0

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
