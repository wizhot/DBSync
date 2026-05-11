/*
** 2024-01-01
** SQLite extension header (minimal stub for DbSync project compilation)
**
** NOTE: This is a minimal stub header. For full extension API,
** download from https://www.sqlite.org/
*/
#ifndef SQLITE3EXT_H
#define SQLITE3EXT_H

#include "sqlite3.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
** Extension loading API
*/
typedef struct sqlite3_api_routines sqlite3_api_routines;

int sqlite3_auto_extension(void(*xEntryPoint)(void));
int sqlite3_cancel_auto_extension(void(*xEntryPoint)(void));
int sqlite3_load_extension(sqlite3 *db, const char *zFilename,
                          const char *zProc, char **pzErrMsg);
int sqlite3_enable_load_extension(sqlite3 *db, int onoff);
void *sqlite3_extension_init(sqlite3 *db, char **pzErrMsg,
                            const sqlite3_api_routines *pApi);

#ifdef SQLITE_CORE
  typedef struct sqlite3_api_routines sqlite3_api_routines;
#endif

#ifdef __cplusplus
}  /* end of extern "C" */
#endif

#endif /* SQLITE3EXT_H */
