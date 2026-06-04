#include "util/json.h"

#include <cctype>
#include <cmath>
#include <iomanip>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace oj::util::json {
namespace {

class Parser {
 public:
  explicit Parser(std::string_view input) : input_(input) {}

  std::optional<Value> parse(std::string* error) {
    skip_whitespace();
    auto value = parse_value();
    if (!value.has_value()) {
      set_external_error(error);
      return std::nullopt;
    }

    skip_whitespace();
    if (!eof()) {
      set_error("unexpected trailing content");
      set_external_error(error);
      return std::nullopt;
    }

    return value;
  }

 private:
  bool eof() const { return position_ >= input_.size(); }

  char peek() const { return eof() ? '\0' : input_[position_]; }

  char consume() { return eof() ? '\0' : input_[position_++]; }

  bool consume_if(char expected) {
    if (peek() != expected) {
      return false;
    }

    ++position_;
    return true;
  }

  void skip_whitespace() {
    while (!eof() &&
           std::isspace(static_cast<unsigned char>(input_[position_])) != 0) {
      ++position_;
    }
  }

  std::optional<Value> parse_value() {
    skip_whitespace();
    if (eof()) {
      set_error("expected json value");
      return std::nullopt;
    }

    switch (peek()) {
      case 'n':
        return parse_literal("null", Value(nullptr));
      case 't':
        return parse_literal("true", Value(true));
      case 'f':
        return parse_literal("false", Value(false));
      case '"':
        return parse_string();
      case '[':
        return parse_array();
      case '{':
        return parse_object();
      default:
        if (peek() == '-' ||
            std::isdigit(static_cast<unsigned char>(peek())) != 0) {
          return parse_number();
        }
        set_error("unexpected character in json value");
        return std::nullopt;
    }
  }

  std::optional<Value> parse_literal(std::string_view literal, Value value) {
    if (input_.substr(position_, literal.size()) != literal) {
      set_error("invalid json literal");
      return std::nullopt;
    }

    position_ += literal.size();
    return value;
  }

  std::optional<Value> parse_string() {
    if (!consume_if('"')) {
      set_error("expected string");
      return std::nullopt;
    }

    std::string result;
    while (!eof()) {
      const char ch = consume();
      if (ch == '"') {
        return Value(std::move(result));
      }

      if (static_cast<unsigned char>(ch) < 0x20) {
        set_error("control character in string");
        return std::nullopt;
      }

      if (ch != '\\') {
        result.push_back(ch);
        continue;
      }

      if (eof()) {
        set_error("unterminated escape sequence");
        return std::nullopt;
      }

      const char escaped = consume();
      switch (escaped) {
        case '"':
        case '\\':
        case '/':
          result.push_back(escaped);
          break;
        case 'b':
          result.push_back('\b');
          break;
        case 'f':
          result.push_back('\f');
          break;
        case 'n':
          result.push_back('\n');
          break;
        case 'r':
          result.push_back('\r');
          break;
        case 't':
          result.push_back('\t');
          break;
        case 'u':
          if (!parse_unicode_escape(result)) {
            return std::nullopt;
          }
          break;
        default:
          set_error("invalid escape sequence");
          return std::nullopt;
      }
    }

    set_error("unterminated string");
    return std::nullopt;
  }

  bool parse_unicode_escape(std::string& output) {
    if (position_ + 4 > input_.size()) {
      set_error("incomplete unicode escape");
      return false;
    }

    unsigned int codepoint = 0;
    for (int i = 0; i < 4; ++i) {
      const char ch = consume();
      codepoint <<= 4;
      if (ch >= '0' && ch <= '9') {
        codepoint += static_cast<unsigned int>(ch - '0');
      } else if (ch >= 'a' && ch <= 'f') {
        codepoint += static_cast<unsigned int>(ch - 'a' + 10);
      } else if (ch >= 'A' && ch <= 'F') {
        codepoint += static_cast<unsigned int>(ch - 'A' + 10);
      } else {
        set_error("invalid unicode escape");
        return false;
      }
    }

    append_utf8(codepoint, output);
    return true;
  }

  void append_utf8(unsigned int codepoint, std::string& output) {
    if (codepoint <= 0x7F) {
      output.push_back(static_cast<char>(codepoint));
    } else if (codepoint <= 0x7FF) {
      output.push_back(static_cast<char>(0xC0 | (codepoint >> 6)));
      output.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
    } else {
      output.push_back(static_cast<char>(0xE0 | (codepoint >> 12)));
      output.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
      output.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
    }
  }

  std::optional<Value> parse_number() {
    const std::size_t start = position_;

    consume_if('-');
    if (peek() == '0') {
      consume();
    } else if (std::isdigit(static_cast<unsigned char>(peek())) != 0) {
      while (std::isdigit(static_cast<unsigned char>(peek())) != 0) {
        consume();
      }
    } else {
      set_error("invalid number");
      return std::nullopt;
    }

    bool is_floating = false;
    if (consume_if('.')) {
      is_floating = true;
      if (std::isdigit(static_cast<unsigned char>(peek())) == 0) {
        set_error("invalid number fraction");
        return std::nullopt;
      }
      while (std::isdigit(static_cast<unsigned char>(peek())) != 0) {
        consume();
      }
    }

    if (peek() == 'e' || peek() == 'E') {
      is_floating = true;
      consume();
      if (peek() == '+' || peek() == '-') {
        consume();
      }
      if (std::isdigit(static_cast<unsigned char>(peek())) == 0) {
        set_error("invalid number exponent");
        return std::nullopt;
      }
      while (std::isdigit(static_cast<unsigned char>(peek())) != 0) {
        consume();
      }
    }

    const std::string text(input_.substr(start, position_ - start));
    try {
      if (is_floating) {
        return Value(std::stod(text));
      }
      return Value(static_cast<std::int64_t>(std::stoll(text)));
    } catch (const std::exception&) {
      set_error("number out of range");
      return std::nullopt;
    }
  }

  std::optional<Value> parse_array() {
    consume_if('[');
    Value::Array result;

    skip_whitespace();
    if (consume_if(']')) {
      return Value(std::move(result));
    }

    while (true) {
      auto item = parse_value();
      if (!item.has_value()) {
        return std::nullopt;
      }
      result.push_back(std::move(*item));

      skip_whitespace();
      if (consume_if(']')) {
        return Value(std::move(result));
      }
      if (!consume_if(',')) {
        set_error("expected comma or closing bracket");
        return std::nullopt;
      }
    }
  }

  std::optional<Value> parse_object() {
    consume_if('{');
    Value::Object result;

    skip_whitespace();
    if (consume_if('}')) {
      return Value(std::move(result));
    }

    while (true) {
      skip_whitespace();
      auto key = parse_string();
      if (!key.has_value() || !key->is_string()) {
        set_error("expected object key");
        return std::nullopt;
      }

      skip_whitespace();
      if (!consume_if(':')) {
        set_error("expected colon after object key");
        return std::nullopt;
      }

      auto value = parse_value();
      if (!value.has_value()) {
        return std::nullopt;
      }
      result[key->as_string()] = std::move(*value);

      skip_whitespace();
      if (consume_if('}')) {
        return Value(std::move(result));
      }
      if (!consume_if(',')) {
        set_error("expected comma or closing brace");
        return std::nullopt;
      }
    }
  }

  void set_error(const std::string& message) {
    if (error_.empty()) {
      error_ = message + " at byte " + std::to_string(position_);
    }
  }

  void set_external_error(std::string* error) const {
    if (error != nullptr) {
      *error = error_.empty() ? "invalid json" : error_;
    }
  }

  std::string_view input_;
  std::size_t position_ = 0;
  std::string error_;
};

void stringify_impl(const Value& value, std::ostringstream& output) {
  if (value.is_null()) {
    output << "null";
  } else if (value.is_bool()) {
    output << (value.as_bool() ? "true" : "false");
  } else if (value.is_int()) {
    output << value.as_int();
  } else if (value.is_double()) {
    output << std::setprecision(15) << value.as_double();
  } else if (value.is_string()) {
    output << '"' << escape_string(value.as_string()) << '"';
  } else if (value.is_array()) {
    output << '[';
    bool first = true;
    for (const auto& item : value.as_array()) {
      if (!first) {
        output << ',';
      }
      first = false;
      stringify_impl(item, output);
    }
    output << ']';
  } else {
    output << '{';
    bool first = true;
    for (const auto& [key, item] : value.as_object()) {
      if (!first) {
        output << ',';
      }
      first = false;
      output << '"' << escape_string(key) << "\":";
      stringify_impl(item, output);
    }
    output << '}';
  }
}

}  // namespace

Value::Value() : value_(nullptr) {}

Value::Value(std::nullptr_t) : value_(nullptr) {}

Value::Value(bool value) : value_(value) {}

Value::Value(std::int64_t value) : value_(value) {}

Value::Value(double value) : value_(value) {}

Value::Value(std::string value) : value_(std::move(value)) {}

Value::Value(const char* value) : value_(std::string(value)) {}

Value::Value(Array value) : value_(std::move(value)) {}

Value::Value(Object value) : value_(std::move(value)) {}

bool Value::is_null() const { return std::holds_alternative<std::nullptr_t>(value_); }

bool Value::is_bool() const { return std::holds_alternative<bool>(value_); }

bool Value::is_int() const { return std::holds_alternative<std::int64_t>(value_); }

bool Value::is_double() const { return std::holds_alternative<double>(value_); }

bool Value::is_number() const { return is_int() || is_double(); }

bool Value::is_string() const { return std::holds_alternative<std::string>(value_); }

bool Value::is_array() const { return std::holds_alternative<Array>(value_); }

bool Value::is_object() const { return std::holds_alternative<Object>(value_); }

bool Value::as_bool() const { return std::get<bool>(value_); }

std::int64_t Value::as_int() const {
  if (is_int()) {
    return std::get<std::int64_t>(value_);
  }
  return static_cast<std::int64_t>(std::get<double>(value_));
}

double Value::as_double() const {
  if (is_double()) {
    return std::get<double>(value_);
  }
  return static_cast<double>(std::get<std::int64_t>(value_));
}

const std::string& Value::as_string() const {
  return std::get<std::string>(value_);
}

const Value::Array& Value::as_array() const { return std::get<Array>(value_); }

const Value::Object& Value::as_object() const { return std::get<Object>(value_); }

Value::Array& Value::as_array() { return std::get<Array>(value_); }

Value::Object& Value::as_object() { return std::get<Object>(value_); }

std::string stringify(const Value& value) {
  std::ostringstream output;
  stringify_impl(value, output);
  return output.str();
}

std::optional<Value> parse(std::string_view input, std::string* error) {
  Parser parser(input);
  return parser.parse(error);
}

std::string escape_string(std::string_view input) {
  std::ostringstream output;
  for (const char ch : input) {
    switch (ch) {
      case '"':
        output << "\\\"";
        break;
      case '\\':
        output << "\\\\";
        break;
      case '\b':
        output << "\\b";
        break;
      case '\f':
        output << "\\f";
        break;
      case '\n':
        output << "\\n";
        break;
      case '\r':
        output << "\\r";
        break;
      case '\t':
        output << "\\t";
        break;
      default:
        if (static_cast<unsigned char>(ch) < 0x20) {
          output << "\\u" << std::hex << std::setw(4) << std::setfill('0')
                 << static_cast<int>(static_cast<unsigned char>(ch))
                 << std::dec << std::setfill(' ');
        } else {
          output << ch;
        }
        break;
    }
  }

  return output.str();
}

}  // namespace oj::util::json
