CXX = g++

CXXFLAGS = -std=c++20 -Wall -Wextra -O2 -MMD -MP -Isrc -Isrc/external
LDFLAGS = src/external/liblua.a -lX11 -lGL -ldl -pthread -

TARGET = engine_test

SRC := $(shell find . -name "*.cpp")
OBJ := $(patsubst %.cpp,build/%.o,$(SRC))
DEP := $(OBJ:.o=.d)

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CXX) $(OBJ) -o $@ $(LDFLAGS)

build/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -rf build $(TARGET)

-include $(DEP)