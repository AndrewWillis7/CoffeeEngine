CXX = g++

CXXFLAGS = -std=c++20 -Wall -Wextra -O2 -MMD -MP -Isrc -Isrc/Core/lua/include -ffunction-sections -fdata-sections
LDFLAGS = -s src/Core/lua/liblua.a -lX11 -lGL -ldl -pthread -lm

TARGET = engine_test

SRC := main.cpp $(shell find src -name "*.cpp")
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