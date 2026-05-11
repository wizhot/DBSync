/// JsonCpp Value - core JSON value representation
#ifndef JSON_VALUE_H_INCLUDED
#define JSON_VALUE_H_INCLUDED

#include "config.h"
#include <algorithm>
#include <map>
#include <vector>
#include <utility>

namespace Json {

class Value {
public:
  using Members = std::vector<String>;
  using UInt = Json::UInt;
  using Int64 = Json::Int64;
  using UInt64 = Json::UInt64;
  using LargestInt = Json::LargestInt;
  using LargestUInt = Json::LargestUInt;
  using ArrayIndex = Json::ArrayIndex;

  class const_iterator;
  class iterator;

  // Constructors
  Value(ValueType type = nullValue);
  Value(Int64 value);
  Value(UInt64 value);
  Value(double value);
  Value(const char* value);
  Value(const String& value);
  Value(bool value);
  Value(const Value& other);
  Value(Value&& other) noexcept;
  ~Value();

  // Assignment
  Value& operator=(const Value& other);
  Value& operator=(Value&& other) noexcept;

  // Type checks
  ValueType type() const;
  const char* typeAsString() const;
  bool isNull() const;
  bool isBool() const;
  bool isInt() const;
  bool isInt64() const;
  bool isUInt() const;
  bool isUInt64() const;
  bool isIntegral() const;
  bool isDouble() const;
  bool isNumeric() const;
  bool isString() const;
  bool isArray() const;
  bool isObject() const;

  // Bool access
  bool asBool() const;

  // Integer access
  Int asInt() const;
  UInt asUInt() const;
  Int64 asInt64() const;
  UInt64 asUInt64() const;
  LargestInt asLargestInt() const;
  LargestUInt asLargestUInt() const;

  // Double access
  double asDouble() const;
  float asFloat() const;

  // String access
  const String& asString() const;
  String& asString();
  const char* asCString() const;

  // Array access
  ArrayIndex size() const;
  bool empty() const;
  void clear();
  void resize(ArrayIndex newSize);
  bool operator!() const;

  Value& operator[](ArrayIndex index);
  const Value& operator[](ArrayIndex index) const;
  Value& operator[](int index);
  const Value& operator[](int index) const;
  Value& operator[](const char* key);
  const Value& operator[](const char* key) const;
  Value& operator[](const String& key);
  const Value& operator[](const String& key) const;

  Value get(ArrayIndex index, const Value& defaultValue) const;
  Value get(const char* key, const Value& defaultValue) const;
  Value get(const String& key, const Value& defaultValue) const;

  // Array operations
  Value& append(const Value& value);
  Value& append(Value&& value);
  void removeIndex(ArrayIndex index, Value* removed = nullptr);
  void removeMember(const char* key);
  void removeMember(const String& key);
  bool isMember(const char* key) const;
  bool isMember(const String& key) const;

  // Object member access
  const Members& getMemberNames() const;

  // Comparison
  int compare(const Value& other) const;
  bool operator<(const Value& other) const;
  bool operator<=(const Value& other) const;
  bool operator>(const Value& other) const;
  bool operator>=(const Value& other) const;
  bool operator==(const Value& other) const;
  bool operator!=(const Value& other) const;

  // Comment
  String toStyledString() const;

  // Iterator support
  iterator begin();
  iterator end();
  const_iterator begin() const;
  const_iterator end() const;
  const_iterator cbegin() const;
  const_iterator cend() const;

private:
  class CZString;
  using ObjectValues = std::map<CZString, Value>;
  using ArrayValues = std::vector<Value>;

  union ValueHolder {
    Int64 int_;
    UInt64 uint_;
    double real_;
    bool bool_;
    String* string_;
    ArrayValues* array_;
    ObjectValues* map_;
  };

  ValueType type_;
  ValueHolder value_;

  String& ensureString();
  ArrayValues& ensureArray();
  ObjectValues& ensureObject();
  void releasePayload();
  void dupPayload(const Value& other);
  void dupPayload(Value&& other);
};

// Iterator classes
class Value::const_iterator {
  friend class Value;
public:
  using iterator_category = std::bidirectional_iterator_tag;
  using value_type = const Value;
  using difference_type = int;
  using pointer = const Value*;
  using reference = const Value&;

  const_iterator();
  const_iterator(const Value::ObjectValues::iterator& current);
  const_iterator(Value::ArrayIndex index, const Value::ArrayValues::iterator& current);

  reference operator*() const;
  pointer operator->() const;
  const_iterator& operator++();
  const_iterator operator++(int);
  const_iterator& operator--();
  const_iterator operator--(int);
  bool operator==(const const_iterator& other) const;
  bool operator!=(const const_iterator& other) const;

  String key() const;
  UInt index() const;
  const char* memberName() const;

private:
  bool isArray() const;
  Value::ObjectValues::iterator current_;
  Value::ArrayIndex arrayIndex_;
};

class Value::iterator : public Value::const_iterator {
  friend class Value;
public:
  using value_type = Value;
  using pointer = Value*;
  using reference = Value&;

  iterator();
  iterator(const Value::ObjectValues::iterator& current);
  iterator(Value::ArrayIndex index, const Value::ArrayValues::iterator& current);

  reference operator*() const;
  pointer operator->() const;
};

} // namespace Json

#endif // JSON_VALUE_H_INCLUDED
