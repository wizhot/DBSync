/// JsonCpp assertions
#ifndef JSON_ASSERTIONS_H_INCLUDED
#define JSON_ASSERTIONS_H_INCLUDED

#include <cstdlib>
#include <sstream>

#ifndef JSON_ASSERT
#define JSON_ASSERT(condition)                                                 \
  do {                                                                         \
    if (!(condition)) {                                                        \
      std::ostringstream oss;                                                  \
      oss << "Assertion failed: " #condition " in " << __FILE__                \
          << ":" << __LINE__;                                                  \
      std::cerr << oss.str() << std::endl;                                    \
      std::abort();                                                            \
    }                                                                          \
  } while (0)
#endif

#ifndef JSON_ASSERT_MESSAGE
#define JSON_ASSERT_MESSAGE(condition, message)                                \
  do {                                                                         \
    if (!(condition)) {                                                        \
      std::ostringstream oss;                                                  \
      oss << "Assertion failed: " #condition " in " << __FILE__                \
          << ":" << __LINE__ << " - " << message;                              \
      std::cerr << oss.str() << std::endl;                                    \
      std::abort();                                                            \
    }                                                                          \
  } while (0)
#endif

#ifndef JSON_FAIL_MESSAGE
#define JSON_FAIL_MESSAGE(message)                                             \
  do {                                                                         \
    std::ostringstream oss;                                                    \
    oss << message << " in " << __FILE__ << ":" << __LINE__;                   \
    std::cerr << oss.str() << std::endl;                                      \
    std::abort();                                                              \
  } while (0)
#endif

#endif // JSON_ASSERTIONS_H_INCLUDED
