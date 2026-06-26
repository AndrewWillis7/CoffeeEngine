SRC = $(shell find src -name "*.cpp")

all:
    g++ main.cpp $(SRC) \
    -o mini.exe \
    -I./src \
    -lgdi32 -lgdiplus \
    -municode -static-libgcc -static-libstdc++