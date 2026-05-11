/// JsonCpp error types
#ifndef JSON_ERRORS_H_INCLUDED
#define JSON_ERRORS_H_INCLUDED

#include "config.h"

namespace Json {

enum ErrorCategory {
  strictMode,
  bestEffort
};

} // namespace Json

#endif // JSON_ERRORS_H_INCLUDED
