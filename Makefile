CXX ?= g++
BUILD_DIR := build
TARGET := $(BUILD_DIR)/oj_server
INIT_TEST_TARGET := $(BUILD_DIR)/tests/initialization_test
DB_SQL_TEST_TARGET := $(BUILD_DIR)/tests/database_sql_test
BACKEND_FOUNDATION_TEST_TARGET := $(BUILD_DIR)/tests/backend_foundation_test
USER_FEATURE_TEST_TARGET := $(BUILD_DIR)/tests/user_feature_test
GTEST_BACKEND_FOUNDATION_TARGET := $(BUILD_DIR)/tests/backend_foundation_gtest
GTEST_USER_FEATURE_TARGET := $(BUILD_DIR)/tests/user_feature_gtest

MYSQL_CFLAGS := $(shell mysql_config --cflags)
MYSQL_LIBS := $(shell mysql_config --libs)
GTEST_CFLAGS := $(shell pkg-config --cflags gtest gtest_main 2>/dev/null)
GTEST_LIBS := $(shell pkg-config --libs gtest gtest_main 2>/dev/null || echo -lgtest -lgtest_main)

CPPFLAGS += -Isrc -Ithird_party/httplib $(MYSQL_CFLAGS)
CXXFLAGS ?= -std=c++20 -Wall -Wextra -Wpedantic -O2
LDLIBS += $(MYSQL_LIBS) -pthread

SRCS := $(shell find src -name '*.cpp')
APP_LIB_SRCS := $(filter-out src/main.cpp,$(SRCS))
OBJS := $(patsubst %.cpp,$(BUILD_DIR)/%.o,$(SRCS))

.PHONY: all clean check-db test test-gtest test-api-curl test-api-curl-basic test-api-python test-api-python-basic

all: $(TARGET)

$(TARGET): $(OBJS)
	@mkdir -p $(dir $@)
	$(CXX) $^ -o $@ $(LDLIBS)

$(BUILD_DIR)/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

check-db: $(TARGET)
	./$(TARGET) --check-db config/app.example.conf

test: $(INIT_TEST_TARGET) $(DB_SQL_TEST_TARGET) $(BACKEND_FOUNDATION_TEST_TARGET) $(USER_FEATURE_TEST_TARGET)
	./$(INIT_TEST_TARGET) .
	./$(DB_SQL_TEST_TARGET) .
	./$(BACKEND_FOUNDATION_TEST_TARGET) .
	./$(USER_FEATURE_TEST_TARGET) .

test-gtest: $(GTEST_BACKEND_FOUNDATION_TARGET) $(GTEST_USER_FEATURE_TARGET)
	./$(GTEST_BACKEND_FOUNDATION_TARGET) .
	./$(GTEST_USER_FEATURE_TARGET) .

$(INIT_TEST_TARGET): tests/cpp/initialization_test.cpp src/config/config.cpp src/db/mysql_client.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $^ -o $@ $(LDLIBS)

$(DB_SQL_TEST_TARGET): tests/cpp/database_sql_test.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $^ -o $@

$(BACKEND_FOUNDATION_TEST_TARGET): tests/cpp/backend_foundation_test.cpp $(APP_LIB_SRCS)
	@mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $^ -o $@ $(LDLIBS)

$(USER_FEATURE_TEST_TARGET): tests/cpp/user_feature_test.cpp src/auth/password.cpp src/auth/session.cpp src/judge/comparator.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $^ -o $@ $(LDLIBS)

test-api-curl: $(TARGET)
	bash scripts/api_curl_test.sh config/app.conf

test-api-curl-basic: $(TARGET)
	bash scripts/api_curl_test.sh --basic config/app.example.conf

test-api-python: $(TARGET)
	python3 scripts/api_python_test.py config/app.conf

test-api-python-basic: $(TARGET)
	python3 scripts/api_python_test.py --basic config/app.example.conf

$(GTEST_BACKEND_FOUNDATION_TARGET): tests/cpp/backend_foundation_gtest.cpp $(APP_LIB_SRCS)
	@mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(GTEST_CFLAGS) $(CXXFLAGS) $^ -o $@ $(LDLIBS) $(GTEST_LIBS)

$(GTEST_USER_FEATURE_TARGET): tests/cpp/user_feature_gtest.cpp src/auth/password.cpp src/auth/session.cpp src/judge/comparator.cpp src/judge/judge_service.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(GTEST_CFLAGS) $(CXXFLAGS) $^ -o $@ $(LDLIBS) $(GTEST_LIBS)

clean:
	rm -rf $(BUILD_DIR)
