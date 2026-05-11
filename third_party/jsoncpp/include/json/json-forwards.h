/// JsonCpp forward declarations
#ifndef JSON_FORWARDS_H_INCLUDED
#define JSON_FORWARDS_H_INCLUDED

#include "config.h"

namespace Json {

// Forward declarations
class FastWriter;
class StyledWriter;
class StyledStreamWriter;
class Writer;
class StreamWriter;
class StreamWriterBuilder;
class Reader;
class CharReader;
class CharReaderBuilder;
class Value;
class Exception;
class RuntimeError;
class LogicError;

using ValueIterator = class ValueConstIterator;
using ValueConstIterator = class ValueConstIterator;

} // namespace Json

#endif // JSON_FORWARDS_H_INCLUDED
