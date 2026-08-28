CXX     := clang++
PY      := python3

SRC     := src/glob.cpp
EXE     := sloth

ifeq ($(OS),Windows_NT)
    HOST    := windows
    PY      := python
else
    HOST    := linux
endif

# TARGET: linux | windows (default = host)
TARGET  ?= $(HOST)

ifeq ($(TARGET),windows)
    SUFFIX  := .exe
    # Cross-compiling from Linux
    ifneq ($(HOST),windows)
        TARGET_FLAGS := --target=x86_64-w64-mingw32
    endif
else
    SUFFIX  :=
    TARGET_FLAGS :=
endif

# Common flags
COMMON  := -Ofast -flto -ftree-vectorize -funroll-loops -w \
           -DNDEBUG -finline-functions -pipe -std=c++23 -ffast-math \
           -fno-rtti -fstrict-aliasing -fomit-frame-pointer \
           -static -fuse-ld=lld

# NNUE embedding
ifdef EVALFILE
    COMMON  += -DEVALFILE_EMBEDDED
    NNUE_H  := src/embedded_net.cpp
    SRC_DEPS := $(NNUE_H)
else
    SRC_DEPS :=
endif

ARCH_sse3   := -msse3 -mssse3 -march=sandybridge -mtune=sandybridge
ARCH_sse4   := -msse4.1 -msse4.2 -march=sandybridge -mtune=sandybridge -mssse3 -mno-avx
ARCH_bmi2   := -march=haswell -msse4.1 -msse4.2 -mbmi -mfma -mavx2 -mbmi2 -mavx
ARCH_avx2   := -march=haswell -mavx2 -mfma -mtune=haswell -DNN_WITH_AVX2
ARCH_avx512 := -march=skylake-avx512 -mavx512f -mavx512cd -mavx512bw -mavx512dq -mtune=skylake-avx512

ARCHS       := sse3 sse4 bmi2 avx2 avx512

.PHONY: all clean $(ARCHS)

all: $(ARCHS)

define ARCH_template
$(1): $(EXE)_$(1)$(SUFFIX)

$(EXE)_$(1)$(SUFFIX): $(SRC) $(SRC_DEPS)
	@echo "Building $(1) ($(TARGET))..."
	$(CXX) -o $$@ $(SRC) $(COMMON) $(TARGET_FLAGS) $(ARCH_$(1))
endef

$(foreach arch,$(ARCHS),$(eval $(call ARCH_template,$(arch))))

ifdef EVALFILE
src/embedded_net.cpp: $(EVALFILE)
	@echo "Embedding NNUE: $(EVALFILE)"
ifeq ($(HOST),windows)
	$(PY) tools/embed_net.py "$(EVALFILE)" src/embedded_net.cpp
else
	xxd -i $(EVALFILE) | \
	  sed 's/unsigned char [a-z_]*/const unsigned char gEmbeddedNNUEData/' | \
	  sed 's/unsigned int [a-z_]*/const size_t gEmbeddedNNUESize/' \
	  > src/embedded_net.cpp
endif
endif

clean:
	rm -f $(foreach arch,$(ARCHS),$(EXE)_$(arch) $(EXE)_$(arch).exe) \
	    src/embedded_net.cpp