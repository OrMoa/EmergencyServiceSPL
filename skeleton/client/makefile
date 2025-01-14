CFLAGS:=-c -Wall -Weffc++ -g -std=c++11 -Iinclude
LDFLAGS:=-lboost_system -lpthread

all: StompEMIClient

# Creating directory for binaries if it doesn't exist
$(shell mkdir -p bin)

# Collecting all source files
SRC_FILES := $(wildcard src/*.cpp)
OBJ_FILES := $(patsubst src/%.cpp,bin/%.o,$(SRC_FILES))

StompEMIClient: $(OBJ_FILES)
	g++ -o bin/StompEMIClient $(OBJ_FILES) $(LDFLAGS)

bin/%.o: src/%.cpp
	g++ $(CFLAGS) -o $@ $<

.PHONY: clean
clean:
	rm -f bin/*
	