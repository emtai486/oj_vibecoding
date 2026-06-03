CXX ?= g++
BUILD_DIR := build
TARGET := $(BUILD_DIR)/oj_server

MYSQL_CFLAGS := $(shell mysql_config --cflags)
MYSQL_LIBS := $(shell mysql_config --libs)

CPPFLAGS += -Isrc -Ithird_party/httplib $(MYSQL_CFLAGS)
CXXFLAGS ?= -std=c++20 -Wall -Wextra -Wpedantic -O2
LDLIBS += $(MYSQL_LIBS) -pthread

SRCS := $(shell find src -name '*.cpp')
OBJS := $(patsubst %.cpp,$(BUILD_DIR)/%.o,$(SRCS))

.PHONY: all clean check-db

all: $(TARGET)

$(TARGET): $(OBJS)
	@mkdir -p $(dir $@)
	$(CXX) $^ -o $@ $(LDLIBS)

$(BUILD_DIR)/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

check-db: $(TARGET)
	./$(TARGET) --check-db config/app.example.conf

clean:
	rm -rf $(BUILD_DIR)
