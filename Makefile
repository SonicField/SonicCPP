CXX      := /usr/bin/g++
CXXFLAGS := -O3 -Wall -Werror -std=c++20 -g
# For GCC
#LDFLAGS  := -L/usr/lib -lstdc++ -lm
# For Clang
LDFLAGS  :=
BUILD    := ./build
OBJ_DIR  := $(BUILD)/objects
APP_DIR  := $(BUILD)/apps
TARGET   := SonicField
INCLUDE  := -I.
SRC      :=  \
   $(wildcard src/*.cpp) \
   $(wildcard src/music/*.cpp) \
   $(wildcard src/test/*.cpp)


OBJECTS  := $(SRC:%.cpp=$(OBJ_DIR)/%.o)

all: build $(APP_DIR)/$(TARGET)

$(OBJ_DIR)/%.o: %.cpp
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) $(INCLUDE) -c $< -o $@ $(LDFLAGS)

$(APP_DIR)/$(TARGET): $(OBJECTS)
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) -o $(APP_DIR)/$(TARGET) $^ $(LDFLAGS)

.PHONY: all build clean debug release test

build:
	@mkdir -p $(APP_DIR)
	@mkdir -p $(OBJ_DIR)

debug: CXXFLAGS += -DDEBUG -g
debug: all

release: CXXFLAGS += -O3 -g
test: kill_main
release: all

test: CXXFLAGS += -DSF_TESTS
test: kill_main
test: all

kill_main:
	-@rm -vf $(OBJ_DIR)/src/main.o

clean:
	-@rm -rvf $(OBJ_DIR)/*
	-@rm -rvf $(APP_DIR)/*
