/// JsonCpp assertions
#ifndef JSON_ASSERTIONS_H_INCLUDED
#define JSON_ASSERTIONS_H_INCLUDED

#include <cassert>
#include <stdexcept>

namespace Json {

/// Assert macro
#define JSON_ASSERT(condition) assert(condition)

/// Assert message macro
#define JSON_ASSERT_MESSAGE(condition, message) \
    do { if (!(condition)) throw std::runtime_error(message); } while(0)

/// Fail macro
#define JSON_FAIL_MESSAGE(message) throw std::runtime_error(message)

} // namespace Json

#endif // JSON_ASSERTIONS_H_INCLUDED
