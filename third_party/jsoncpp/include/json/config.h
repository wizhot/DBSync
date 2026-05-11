/// JsonCpp configuration
#ifndef JSON_CONFIG_H_INCLUDED
#define JSON_CONFIG_H_INCLUDED

#include <cstddef>
#include <cstdint>
#include <istream>
#include <ostream>
#include <sstream>
#include <string>
#include <stdexcept>

// JSON value types
namespace Json {

enum ValueType {
  nullValue = 0,
  intValue,
  uintValue,
  realValue,
  stringValue,
  booleanValue,
  arrayValue,
  objectValue
};

// String type
using String = std::string;
using UInt64 = uint64_t;
using Int64 = int64_t;
using UInt = unsigned int;
using LargestInt = int64_t;
using LargestUInt = uint64_t;
using ArrayIndex = unsigned int;

// Exception types
class Exception : public std::exception {
public:
  Exception(String msg) : msg_(std::move(msg)) {}
  const char* what() const noexcept override { return msg_.c_str(); }
protected:
  String msg_;
};

class RuntimeError : public Exception {
public:
  RuntimeError(String msg) : Exception(std::move(msg)) {}
};

class LogicError : public Exception {
public:
  LogicError(String msg) : Exception(std::move(msg)) {}
};

} // namespace Json

// JSON_API macro
#ifndef JSON_API
#define JSON_API
#endif

#endif // JSON_CONFIG_H_INCLUDED
