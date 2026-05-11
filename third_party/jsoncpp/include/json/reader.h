/// JsonCpp Reader - JSON deserialization
#ifndef JSON_READER_H_INCLUDED
#define JSON_READER_H_INCLUDED

#include "value.h"
#include <istream>
#include <memory>
#include <string>
#include <vector>

namespace Json {

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
  enum Features {
    none = 0,
    allowComments = 1,
    allowTrailingCommas = 2,
    strictMode = 4,
    allowSpecialFloats = 8,
    allowSingleQuotes = 16,
    allowBOM = 32,
    allowNanAndInf = 64,
    allowEscapedAny = 128
  };

  Reader(Features features = Features::none);
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
  // ... (implementation details omitted for stub)
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
  bool stackLimit_;
  bool failIfExtra_;
  bool rejectDupKeys_;
  bool allowEscapedAny_;
};

/// Parse from an input stream.
/// \deprecated Use CharReaderBuilder instead.
bool JSON_API parseFromStream(CharReaderBuilder const& builder, std::istream& sin,
                              Value* root, String* errs);

/// Parse from string.
bool JSON_API parse(const String& document, Value& root,
                    CharReaderBuilder const& builder);

} // namespace Json

#endif // JSON_READER_H_INCLUDED
