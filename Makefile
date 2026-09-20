ifeq ($(OS),Windows_NT)
    PLATFORM ?= windows
else
    PLATFORM ?= linux
endif

CXX = g++

CXXFLAGS = -std=c++20 -Wall -Wextra -O2 -MMD -MP -Isrc -Isrc/Core/lua/include -ffunction-sections -fdata-sections
LDFLAGS  = -s

# Recursive wildcard -- pure make, so no dependency on `find` (on Windows,
# a stray find.exe from System32 would shadow the POSIX one).
rwildcard = $(foreach d,$(wildcard $(1:=/*)),$(call rwildcard,$d,$2) $(filter $(subst *,%,$2),$d))

ALL_SRC := main.cpp $(call rwildcard,src,*.cpp)

LINUX_ONLY_SRC   := src/OS_/LinuxWindow.cpp src/OS_/GLXGraphicsContext.cpp
WINDOWS_ONLY_SRC := src/OS_/WindowsWindow.cpp src/OS_/WGLGraphicsContext.cpp

ifeq ($(PLATFORM),windows)
    TARGET   := engine_test.exe
    SRC      := $(filter-out $(LINUX_ONLY_SRC),$(ALL_SRC))
    LUA_LIB  := src/Core/lua/win64/liblua.a
    # WindowsWindow.cpp uses L"..." literals with the generic (A/W-switching)
    # Win32 API names, so the W variants must be selected.
    CXXFLAGS += -DUNICODE -D_UNICODE
    # -static: no libstdc++/libgcc/winpthread DLLs needed next to the .exe,
    # so it runs from Explorer, not just from inside the MSYS2 shell.
    LDLIBS   := $(LUA_LIB) -lopengl32 -lgdi32 -luser32 -static
else ifeq ($(PLATFORM),linux)
    TARGET   := engine_test
    SRC      := $(filter-out $(WINDOWS_ONLY_SRC),$(ALL_SRC))
    LUA_LIB  := src/Core/lua/liblua.a
    LDLIBS   := $(LUA_LIB) -lX11 -lGL -ldl -pthread -lm
else
    $(error Unknown PLATFORM '$(PLATFORM)' -- expected 'linux' or 'windows')
endif

BUILD_DIR := build/$(PLATFORM)
OBJ := $(patsubst %.cpp,$(BUILD_DIR)/%.o,$(SRC))
DEP := $(OBJ:.o=.d)

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJ) $(LUA_LIB)
	$(CXX) $(OBJ) -o $@ $(LDFLAGS) $(LDLIBS)

$(BUILD_DIR)/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Only reached when the Lua archive is missing.
$(LUA_LIB):
	$(error Missing $(LUA_LIB) -- build it with src/OS_/Tools/$(if $(filter windows,$(PLATFORM)),winlua.sh (MSYS2 UCRT64 shell),build_lua.sh))

clean:
	rm -rf build engine_test engine_test.exe

-include $(DEP)