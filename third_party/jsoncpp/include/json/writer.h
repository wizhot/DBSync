/// JsonCpp Writer - JSON serialization
#ifndef JSON_WRITER_H_INCLUDED
#define JSON_WRITER_H_INCLUDED

#include "value.h"
#include <ostream>
#include <string>
#include <vector>
#include <memory>

namespace Json {

class JSON_API Writer {
public:
  virtual ~Writer();

  virtual String write(const Value& root) = 0;

  /// Write a Value in JSON format to a stream, without adding a newline.
  virtual void writeValue(const Value& value, std::ostream* out) = 0;
};

class JSON_API FastWriter : public Writer {
public:
  FastWriter();
  ~FastWriter() override;

  String write(const Value& root) override;

  void enableYAMLCompatibility();
  void dropNullPlaceholders();

private:
  void writeValue(const Value& value, std::ostream* out) override;
  std::ostream* document_;
  bool yamlCompatibilityEnabled_;
  bool dropNullPlaceholders_;
};

class JSON_API StyledWriter : public Writer {
public:
  StyledWriter();
  ~StyledWriter() override;

  String write(const Value& root) override;

private:
  void writeValue(const Value& value, std::ostream* out) override;
  void writeArrayValue(const Value& value, std::ostream* out);
  bool isMultilineArray(const Value& value);
  void pushValue(const String& value);
  void writeIndent();
  void writeWithIndent(const String& value);
  void indent();
  void unindent();
  using ChildValues = std::vector<String>;
  ChildValues childValues_;
  std::ostream* document_;
  String indentString_;
  int rightMargin_;
  int indentSize_;
};

class JSON_API StyledStreamWriter {
public:
  StyledStreamWriter(const String& indentation = "\t");
  ~StyledStreamWriter();

  void write(std::ostream& out, const Value& root);

private:
  void writeValue(const Value& value, std::ostream& out);
  void writeArrayValue(const Value& value, std::ostream& out);
  bool isMultilineArray(const Value& value);
  void pushValue(const String& value);
  void writeIndent(std::ostream& out);
  void writeWithIndent(const String& value, std::ostream& out);
  void indent();
  void unindent();
  using ChildValues = std::vector<String>;
  ChildValues childValues_;
  std::ostream* document_;
  String indentString_;
  unsigned int rightMargin_;
  String indentation_;
};

class JSON_API StreamWriter : public Writer {
public:
  ~StreamWriter() override;
  virtual int write(const Value& root, std::ostream* sout) = 0;
  String write(const Value& root) override;
};

class JSON_API StreamWriterBuilder : public Writer {
public:
  StreamWriterBuilder();
  ~StreamWriterBuilder() override;

  StreamWriter* newStreamWriter() const;

  String write(const Value& root) override;

  /// Set a configuration setting.
  void set(const String& key, const String& value);

  /// Get a configuration setting.
  String get(const String& key) const;

  static void setDefaults(Json::Value* settings);

private:
  Json::Value settings_;
};

/// Write a Value in JSON format to a stream, without adding a newline.
/// This is a convenience function.
std::ostream& operator<<(std::ostream& sout, const Value& root);

} // namespace Json

#endif // JSON_WRITER_H_INCLUDED
