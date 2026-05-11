/*
** 2024-01-01
** SQLite amalgamation header (minimal stub for DbSync project compilation)
**
** NOTE: This is a minimal stub header containing only the API declarations
** needed by the DbSync project. For full SQLite API, download the complete
** amalgamation from https://www.sqlite.org/
*/
#ifndef SQLITE3_H
#define SQLITE3_H

#include <stdarg.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== Basic Types ==================== */
typedef struct sqlite3 sqlite3;
typedef struct sqlite3_stmt sqlite3_stmt;

typedef void (*sqlite3_destructor_type)(void*);
#define SQLITE_STATIC      ((sqlite3_destructor_type)0)
#define SQLITE_TRANSIENT   ((sqlite3_destructor_type)-1)

/* ==================== Result Codes ==================== */
#define SQLITE_OK           0   /* Successful result */
#define SQLITE_ERROR        1   /* SQL error or missing database */
#define SQLITE_BUSY         5   /* The database file is locked */
#define SQLITE_NOMEM        7   /* A malloc() failed */
#define SQLITE_ABORT        4   /* Callback routine requested an abort */
#define SQLITE_MISUSE       21  /* Library used incorrectly */
#define SQLITE_ROW          100 /* sqlite3_step() has another row ready */
#define SQLITE_DONE         101 /* sqlite3_step() has finished executing */

/* ==================== Extended Result Codes ==================== */
#define SQLITE_CANTOPEN     14  /* Unable to open the database file */

/* ==================== Configuration Options ==================== */
#define SQLITE_HAS_CODEC    0

/* ==================== Function Prototypes ==================== */

/* Open/Close */
int sqlite3_open(const char *filename, sqlite3 **ppDb);
int sqlite3_open16(const void *filename, sqlite3 **ppDb);
int sqlite3_open_v2(const char *filename, sqlite3 **ppDb, int flags, const char *zVfs);
int sqlite3_close(sqlite3 *db);
int sqlite3_close_v2(sqlite3 *db);

/* Execute SQL */
int sqlite3_exec(
  sqlite3 *db,                                  /* An open database */
  const char *sql,                               /* SQL to be evaluated */
  int (*callback)(void*,int,char**,char**),      /* Callback function */
  void *arg,                                     /* 1st argument to callback */
  char **errmsg                                  /* Error msg written here */
);
void sqlite3_free(void *ptr);

/* Error messages */
const char *sqlite3_errmsg(sqlite3 *db);
int sqlite3_errcode(sqlite3 *db);
int sqlite3_extended_errcode(sqlite3 *db);

/* Prepare/Execute statements */
int sqlite3_prepare_v2(
  sqlite3 *db,            /* Database handle */
  const char *zSql,       /* SQL statement, UTF-8 encoded */
  int nByte,              /* Maximum length of zSql in bytes. */
  sqlite3_stmt **ppStmt,  /* OUT: Statement handle */
  const char **pzTail     /* OUT: Pointer to unused portion of zSql */
);
int sqlite3_prepare(
  sqlite3 *db,            /* Database handle */
  const char *zSql,       /* SQL statement, UTF-8 encoded */
  int nByte,              /* Maximum length of zSql in bytes. */
  sqlite3_stmt **ppStmt,  /* OUT: Statement handle */
  const char **pzTail     /* OUT: Pointer to unused portion of zSql */
);
int sqlite3_finalize(sqlite3_stmt *pStmt);
int sqlite3_step(sqlite3_stmt *pStmt);
int sqlite3_reset(sqlite3_stmt *pStmt);

/* Bind parameters */
int sqlite3_bind_blob(sqlite3_stmt*, int, const void*, int n, void(*)(void*));
int sqlite3_bind_double(sqlite3_stmt*, int, double);
int sqlite3_bind_int(sqlite3_stmt*, int, int);
int sqlite3_bind_int64(sqlite3_stmt*, int, long long int);
int sqlite3_bind_null(sqlite3_stmt*, int);
int sqlite3_bind_text(sqlite3_stmt*, int, const char*, int n, void(*)(void*));
int sqlite3_bind_text16(sqlite3_stmt*, int, const void*, int, void(*)(void*));
int sqlite3_bind_value(sqlite3_stmt*, int, const sqlite3_value*);
int sqlite3_bind_zeroblob(sqlite3_stmt*, int, int n);
int sqlite3_bind_parameter_count(sqlite3_stmt*);
int sqlite3_bind_parameter_name(sqlite3_stmt*, int);
int sqlite3_bind_parameter_index(sqlite3_stmt*, const char *zName);
int sqlite3_clear_bindings(sqlite3_stmt*);

/* Column access */
int sqlite3_column_count(sqlite3_stmt *pStmt);
const char *sqlite3_column_name(sqlite3_stmt*, int N);
const char *sqlite3_column_decltype(sqlite3_stmt*, int);
const unsigned char *sqlite3_column_text(sqlite3_stmt*, int iCol);
const void *sqlite3_column_text16(sqlite3_stmt*, int iCol);
double sqlite3_column_double(sqlite3_stmt*, int iCol);
int sqlite3_column_int(sqlite3_stmt*, int iCol);
long long int sqlite3_column_int64(sqlite3_stmt*, int iCol);
const void *sqlite3_column_blob(sqlite3_stmt*, int iCol);
int sqlite3_column_bytes(sqlite3_stmt*, int iCol);
int sqlite3_column_bytes16(sqlite3_stmt*, int iCol);
int sqlite3_column_type(sqlite3_stmt*, int iCol);

/* Result info */
sqlite3_int64 sqlite3_last_insert_rowid(sqlite3 *db);
int sqlite3_changes(sqlite3 *db);
int sqlite3_total_changes(sqlite3 *db);

/* Busy/Timeout */
int sqlite3_busy_timeout(sqlite3 *db, int ms);
int sqlite3_busy_handler(sqlite3 *db, int(*)(void*,int), void*);

/* Encryption (SQLCipher extension) */
int sqlite3_key(sqlite3 *db, const void *pKey, int nKey);
int sqlite3_key_v2(sqlite3 *db, const char *zDbName, const void *pKey, int nKey);
int sqlite3_rekey(sqlite3 *db, const void *pKey, int nKey);
int sqlite3_rekey_v2(sqlite3 *db, const char *zDbName, const void *pKey, int nKey);

/* sqlite3_value */
typedef struct sqlite3_value sqlite3_value;
const void *sqlite3_value_blob(sqlite3_value*);
double sqlite3_value_double(sqlite3_value*);
int sqlite3_value_int(sqlite3_value*);
long long int sqlite3_value_int64(sqlite3_value*);
const unsigned char *sqlite3_value_text(sqlite3_value*);
const void *sqlite3_value_text16(sqlite3_value*);
int sqlite3_value_type(sqlite3_value*);
int sqlite3_value_bytes(sqlite3_value*);
int sqlite3_value_bytes16(sqlite3_value*);

/* Utility */
int sqlite3_libversion_number(void);
const char *sqlite3_libversion(void);
const char *sqlite3_sourceid(void);
int sqlite3_threadsafe(void);
int64_t sqlite3_memory_used(void);
int64_t sqlite3_memory_highwater(int resetFlag);

/* String functions */
char *sqlite3_mprintf(const char*,...);
char *sqlite3_vmprintf(const char*, va_list);
char *sqlite3_snprintf(int,char*,const char*,...);
char *sqlite3_vsnprintf(int,char*,const char*, va_list);

/* Misc */
int sqlite3_initialize(void);
int sqlite3_shutdown(void);
int sqlite3_os_end(void);
int sqlite3_config(int,...);
int sqlite3_db_config(sqlite3*, int op, ...);

#ifdef __cplusplus
}  /* end of extern "C" */
#endif

#endif /* SQLITE3_H */
