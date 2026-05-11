/// JsonCpp Reader - JSON deserialization
#ifndef JSON_READER_H_INCLUDED
#define JSON_READER_H_INCLUDED

#include "value.h"
#include <istream>
#include <memory>
#include <string>
#include <vector>

namespace Json {

// Error structure for structured error reporting
struct StructError {
  String message;
  int line;
  int column;
};

// Token type for internal parsing
struct Token {
  enum Kind {
    tokenEndOfStream,
    tokenObjectBegin,
    tokenObjectEnd,
    tokenArrayBegin,
    tokenArrayEnd,
    tokenString,
    tokenNumber,
    tokenTrue,
    tokenFalse,
    tokenNull,
    tokenArraySeparator,
    tokenMemberSeparator,
    tokenComment,
    tokenError
  };
  Kind kind;
  const char* begin;
  const char* end;
};

class JSON_API CharReader {
public:
  virtual ~CharReader();

  /// Read a Value from a JSON document.
  /// \param beginDoc Pointer to the start of the JSON document.
  /// \param endDoc Pointer to the end of the JSON document.
  /// \param root [out] Contains the root value of the document if it was successfully parsed.
  /// \param errs [out] Formatted error messages if parsing fails.
  /// \return \c true if the document was successfully parsed, \c false if an error occurred.
  virtual bool parse(const char* beginDoc, const char* endDoc, Value* root,
                     String* errs) = 0;
};

class JSON_API Reader {
public:
  Reader();
  ~Reader();

  bool parse(const String& document, Value& root, bool collectComments = true);
  bool parse(const char* beginDoc, const char* endDoc, Value& root,
             bool collectComments = true);
  bool parse(std::istream& is, Value& root, bool collectComments = true);

  String getFormattedErrorMessages() const;
  std::vector<StructError> getStructuredErrors() const;

private:
  bool readValue(Value& root);
  bool readToken(Token& token);
};

class JSON_API CharReaderBuilder {
public:
  CharReaderBuilder();
  ~CharReaderBuilder();

  std::unique_ptr<CharReader> newCharReader() const;

  void set(const String& key, bool value);
  void set(const String& key, const String& value);
  void set(const String& key, int value);

  /// Get a configuration setting.
  bool get(const String& key, bool* value) const;
  bool get(const String& key, String* value) const;
  bool get(const String& key, int* value) const;

  static void setDefaults(Json::Value* settings);

  /// Configuration
  bool strictMode_;
  bool allowComments_;
  bool allowTrailingCommas_;
  bool allowSpecialFloats_;
  bool allowSingleQuotes_;
  bool allowBOM_;
  bool allowNanAndInf_;
  int stackLimit_;
  bool failIfExtra_;
  bool rejectDupKeys_;
  bool allowEscapedAny_;
};

/// Parse from an input stream.
bool JSON_API parseFromStream(CharReaderBuilder const& builder, std::istream& sin,
                              Value* root, String* errs);

/// Parse from string.
bool JSON_API parse(const String& document, Value& root,
                    CharReaderBuilder const& builder);

} // namespace Json

#endif // JSON_READER_H_INCLUDED
