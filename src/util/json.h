#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace oj::util::json {

class Value {
 public:
  using Array = std::vector<Value>;
  using Object = std::map<std::string, Value>;

  Value();
  Value(std::nullptr_t);
  Value(bool value);
  Value(std::int64_t value);
  Value(double value);
  Value(std::string value);
  Value(const char* value);
  Value(Array value);
  Value(Object value);

  bool is_null() const;
  bool is_bool() const;
  bool is_int() const;
  bool is_double() const;
  bool is_number() const;
  bool is_string() const;
  bool is_array() const;
  bool is_object() const;

  bool as_bool() const;
  std::int64_t as_int() const;
  double as_double() const;
  const std::string& as_string() const;
  const Array& as_array() const;
  const Object& as_object() const;

  Array& as_array();
  Object& as_object();

 private:
  using Storage = std::variant<std::nullptr_t, bool, std::int64_t, double,
                               std::string, Array, Object>;

  Storage value_;
};

std::string stringify(const Value& value);
std::optional<Value> parse(std::string_view input, std::string* error = nullptr);

std::string escape_string(std::string_view input);

}  // namespace oj::util::json
