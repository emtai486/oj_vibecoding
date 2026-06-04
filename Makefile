CXX ?= g++
BUILD_DIR := build
TARGET := $(BUILD_DIR)/oj_server
INIT_TEST_TARGET := $(BUILD_DIR)/tests/initialization_test
DB_SQL_TEST_TARGET := $(BUILD_DIR)/tests/database_sql_test
BACKEND_FOUNDATION_TEST_TARGET := $(BUILD_DIR)/tests/backend_foundation_test
GTEST_BACKEND_FOUNDATION_TARGET := $(BUILD_DIR)/tests/backend_foundation_gtest

MYSQL_CFLAGS := $(shell mysql_config --cflags)
MYSQL_LIBS := $(shell mysql_config --libs)
GTEST_CFLAGS := $(shell pkg-config --cflags gtest gtest_main 2>/dev/null)
GTEST_LIBS := $(shell pkg-config --libs gtest gtest_main 2>/dev/null || echo -lgtest -lgtest_main)

CPPFLAGS += -Isrc -Ithird_party/httplib $(MYSQL_CFLAGS)
CXXFLAGS ?= -std=c++20 -Wall -Wextra -Wpedantic -O2
LDLIBS += $(MYSQL_LIBS) -pthread

SRCS := $(shell find src -name '*.cpp')
OBJS := $(patsubst %.cpp,$(BUILD_DIR)/%.o,$(SRCS))

.PHONY: all clean check-db test test-gtest

all: $(TARGET)

$(TARGET): $(OBJS)
	@mkdir -p $(dir $@)
	$(CXX) $^ -o $@ $(LDLIBS)

$(BUILD_DIR)/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

check-db: $(TARGET)
	./$(TARGET) --check-db config/app.example.conf

test: $(INIT_TEST_TARGET) $(DB_SQL_TEST_TARGET) $(BACKEND_FOUNDATION_TEST_TARGET)
	./$(INIT_TEST_TARGET) .
	./$(DB_SQL_TEST_TARGET) .
	./$(BACKEND_FOUNDATION_TEST_TARGET) .

test-gtest: $(GTEST_BACKEND_FOUNDATION_TARGET)
	./$(GTEST_BACKEND_FOUNDATION_TARGET) .

$(INIT_TEST_TARGET): tests/cpp/initialization_test.cpp src/config/config.cpp src/db/mysql_client.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $^ -o $@ $(LDLIBS)

$(DB_SQL_TEST_TARGET): tests/cpp/database_sql_test.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $^ -o $@

$(BACKEND_FOUNDATION_TEST_TARGET): tests/cpp/backend_foundation_test.cpp src/app/server.cpp src/config/config.cpp src/db/mysql_client.cpp src/util/json.cpp src/util/json_response.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $^ -o $@ $(LDLIBS)

$(GTEST_BACKEND_FOUNDATION_TARGET): tests/cpp/backend_foundation_gtest.cpp src/app/server.cpp src/config/config.cpp src/db/mysql_client.cpp src/util/json.cpp src/util/json_response.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(GTEST_CFLAGS) $(CXXFLAGS) $^ -o $@ $(LDLIBS) $(GTEST_LIBS)

clean:
	rm -rf $(BUILD_DIR)
