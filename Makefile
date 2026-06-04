CXX ?= g++
BUILD_DIR := build
TARGET := $(BUILD_DIR)/oj_server
INIT_TEST_TARGET := $(BUILD_DIR)/tests/initialization_test
DB_SQL_TEST_TARGET := $(BUILD_DIR)/tests/database_sql_test

MYSQL_CFLAGS := $(shell mysql_config --cflags)
MYSQL_LIBS := $(shell mysql_config --libs)

CPPFLAGS += -Isrc -Ithird_party/httplib $(MYSQL_CFLAGS)
CXXFLAGS ?= -std=c++20 -Wall -Wextra -Wpedantic -O2
LDLIBS += $(MYSQL_LIBS) -pthread

SRCS := $(shell find src -name '*.cpp')
OBJS := $(patsubst %.cpp,$(BUILD_DIR)/%.o,$(SRCS))

.PHONY: all clean check-db test

all: $(TARGET)

$(TARGET): $(OBJS)
	@mkdir -p $(dir $@)
	$(CXX) $^ -o $@ $(LDLIBS)

$(BUILD_DIR)/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

check-db: $(TARGET)
	./$(TARGET) --check-db config/app.example.conf

test: $(INIT_TEST_TARGET) $(DB_SQL_TEST_TARGET)
	./$(INIT_TEST_TARGET) .
	./$(DB_SQL_TEST_TARGET) .

$(INIT_TEST_TARGET): tests/unit/initialization_test.cpp src/config/config.cpp src/db/mysql_client.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $^ -o $@ $(LDLIBS)

$(DB_SQL_TEST_TARGET): tests/unit/database_sql_test.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $^ -o $@

clean:
	rm -rf $(BUILD_DIR)
