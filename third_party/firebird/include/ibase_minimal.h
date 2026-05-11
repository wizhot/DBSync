/**
 * @file ibase_minimal.h
 * @brief Firebird API minimal header for DbSync project
 * @details Contains only the type definitions and function declarations
 *          required by the DbSync project's FirebirdManager module.
 *          Based on the Firebird ibase.h public API.
 */
#ifndef IBASE_MINIMAL_H
#define IBASE_MINIMAL_H

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== Basic Types ==================== */

#ifdef _WIN32
  #include <windows.h>
  typedef long ISC_STATUS;
  typedef long ISC_LONG;
  typedef unsigned long ISC_ULONG;
#else
  #include <stddef.h>
  typedef int ISC_STATUS;
  typedef int ISC_LONG;
  typedef unsigned int ISC_ULONG;
#endif

typedef ISC_LONG ISC_INT64;
typedef ISC_ULONG ISC_UINT64;

/* Handle types */
typedef void* isc_db_handle;
typedef void* isc_tr_handle;
typedef void* isc_stmt_handle;
typedef void* isc_blob_handle;

#define isc_db_handle isc_db_handle
#define isc_tr_handle isc_tr_handle
#define isc_stmt_handle isc_stmt_handle

/* ==================== SQLDA Structures ==================== */

#define SQLDA_VERSION1 1

/* SQL dialect */
#define SQL_DIALECT_V6 3

/* DSQL option */
#define DSQL_close      1
#define DSQL_drop       2
#define DSQL_execute    3
#define DSQL_execute2   4
#define DSQL_fetch      5
#define DSQL_prepare    6
#define DSQL_describe   7
#define DSQL_execute_immediate 8
#define DSQL_set_cursor_name 9
#define DSQL_info_sql 10

/* XSQLVAR length macro */
#define XSQLDA_LENGTH(n) (sizeof(XSQLDA) + (n - 1) * sizeof(XSQLVAR))

/* SQLVAR short alias */
typedef struct {
    short sqltype;       /* datatype of field */
    short sqlscale;      /* scale factor */
    short sqlsubtype;    /* subtype */
    short sqllen;        /* length of data */
    char* sqldata;       /* pointer to data */
    short* sqlind;       /* pointer to indicator */
    short sqlname_length;/* length of sqlname field */
    char sqlname[32];    /* field name */
    short relname_length;/* length of relname field */
    char relname[32];    /* table name */
    short ownname_length;/* length of ownname field */
    char ownname[32];    /* owner name */
    short aliasname_length;
    char aliasname[32];
} XSQLVAR;

/* XSQLDA structure */
typedef struct {
    short version;       /* version of this XSQLDA */
    short sqld;          /* number of fields returned */
    short sqln;          /* number of fields allocated */
    char sqldaid[8];     /* XSQLDA identifier string */
    ISC_LONG sqldabc;    /* length of SQLDA */
    XSQLVAR* sqlvar;     /* pointer to first XSQLVAR */
} XSQLDA;

/* ==================== SQL Data Types ==================== */

#define SQL_TEXT        452
#define SQL_VARYING     448
#define SQL_SHORT       500
#define SQL_LONG        496
#define SQL_INT64       516
#define SQL_FLOAT       482
#define SQL_DOUBLE      480
#define SQL_TIMESTAMP   510
#define SQL_TYPE_DATE   570
#define SQL_TYPE_TIME   572
#define SQL_BLOB        520
#define SQL_ARRAY       540
#define SQL_QUAD        544
#define SQL_D_FLOAT     484
#define SQL_BOOLEAN     546

/* ==================== DPB (Database Parameter Block) Constants ==================== */

#define isc_dpb_version1         1
#define isc_dpb_user_name        28
#define isc_dpb_password         29
#define isc_dpb_lc_ctype         57
#define isc_dpb_page_size        5
#define isc_dpb_num_buffers      6
#define isc_dpb_force_write      24
#define isc_dpb_no_reserve       25
#define isc_dpb_dbkey_scope      26
#define isc_dpb_connect_timeout  62
#define isc_dpb_dummy_packet_interval 63
#define isc_dpb_sql_role_name    60

/* ==================== TPB (Transaction Parameter Block) Constants ==================== */

#define isc_tpb_version1         1
#define isc_tpb_consistency      1
#define isc_tpb_concurrency      2
#define isc_tpb_shared           3
#define isc_tpb_protected        4
#define isc_tpb_exclusive        5
#define isc_tpb_wait             6
#define isc_tpb_nowait           7
#define isc_tpb_read             8
#define isc_tpb_write            9
#define isc_tpb_read_committed   10
#define isc_tpb_auto_commit      11
#define isc_tpb_rec_version      11
#define isc_tpb_no_rec_version   12
#define isc_tpb_restart_requests 13
#define isc_tpb_no_auto_undo     14

/* ==================== ISC API Function Declarations ==================== */

/* Database operations */
ISC_STATUS isc_attach_database(ISC_STATUS* status_vector,
                               short db_name_length,
                               const char* db_name,
                               isc_db_handle* db_handle,
                               short dpb_length,
                               const char* dpb);

ISC_STATUS isc_detach_database(ISC_STATUS* status_vector,
                               isc_db_handle* db_handle);

ISC_STATUS isc_database_info(ISC_STATUS* status_vector,
                             isc_db_handle* db_handle,
                             short item_length,
                             const char* items,
                             short buffer_length,
                             char* buffer);

/* Transaction operations */
ISC_STATUS isc_start_transaction(ISC_STATUS* status_vector,
                                 isc_tr_handle* trans_handle,
                                 short db_handle_count,
                                 isc_db_handle* db_handle,
                                 short tpb_length,
                                 const char* tpb);

ISC_STATUS isc_commit_transaction(ISC_STATUS* status_vector,
                                  isc_tr_handle* trans_handle);

ISC_STATUS isc_commit_retaining(ISC_STATUS* status_vector,
                                isc_tr_handle* trans_handle);

ISC_STATUS isc_rollback_transaction(ISC_STATUS* status_vector,
                                    isc_tr_handle* trans_handle);

/* DSQL operations */
ISC_STATUS isc_dsql_allocate_statement(ISC_STATUS* status_vector,
                                       isc_db_handle* db_handle,
                                       isc_stmt_handle* stmt_handle);

ISC_STATUS isc_dsql_prepare(ISC_STATUS* status_vector,
                            isc_tr_handle* trans_handle,
                            isc_stmt_handle* stmt_handle,
                            unsigned short length,
                            const char* statement,
                            unsigned short dialect,
                            XSQLDA* xsqlda);

ISC_STATUS isc_dsql_execute(ISC_STATUS* status_vector,
                            isc_tr_handle* trans_handle,
                            isc_stmt_handle* stmt_handle,
                            unsigned short dialect,
                            XSQLDA* xsqlda);

ISC_STATUS isc_dsql_execute_immediate(ISC_STATUS* status_vector,
                                      isc_db_handle* db_handle,
                                      isc_tr_handle* trans_handle,
                                      unsigned short length,
                                      const char* statement,
                                      unsigned short dialect,
                                      XSQLDA* xsqlda);

ISC_STATUS isc_dsql_fetch(ISC_STATUS* status_vector,
                          isc_stmt_handle* stmt_handle,
                          unsigned short dialect,
                          XSQLDA* xsqlda);

ISC_STATUS isc_dsql_free_statement(ISC_STATUS* status_vector,
                                   isc_stmt_handle* stmt_handle,
                                    unsigned short option);

ISC_STATUS isc_dsql_describe(ISC_STATUS* status_vector,
                             isc_stmt_handle* stmt_handle,
                             unsigned short dialect,
                             XSQLDA* xsqlda);

ISC_STATUS isc_dsql_describe_bind(ISC_STATUS* status_vector,
                                  isc_stmt_handle* stmt_handle,
                                  unsigned short dialect,
                                  XSQLDA* xsqlda);

ISC_STATUS isc_dsql_set_cursor_name(ISC_STATUS* status_vector,
                                    isc_stmt_handle* stmt_handle,
                                    const char* cursor_name,
                                    short length);

/* Error handling */
int isc_interprete(char* buffer, ISC_STATUS** status_vector);
const char* isc_sqlcode(ISC_STATUS* status_vector);
long isc_sqlcode_s(ISC_STATUS* status_vector);

/* Event functions */
ISC_STATUS isc_que_events(ISC_STATUS* status_vector,
                          isc_db_handle* db_handle,
                          ISC_ULONG* event_id,
                          short length,
                          const char* event_table,
                          void (*ast)(void*, short, const char*),
                          void* arg);

ISC_STATUS isc_cancel_events(ISC_STATUS* status_vector,
                             isc_db_handle* db_handle,
                             ISC_ULONG* event_id);

ISC_STATUS isc_event_counts(ISC_ULONG* result_vector,
                            short length,
                            const char* event_buffer,
                            const char* event_buffer2);

/* Blob operations */
ISC_STATUS isc_open_blob(ISC_STATUS* status_vector,
                         isc_db_handle* db_handle,
                         isc_tr_handle* trans_handle,
                         isc_blob_handle* blob_handle,
                         ISC_QUAD* blob_id);

ISC_STATUS isc_close_blob(ISC_STATUS* status_vector,
                          isc_blob_handle* blob_handle);

ISC_STATUS isc_get_segment(ISC_STATUS* status_vector,
                           isc_blob_handle* blob_handle,
                           unsigned short* length,
                           short buffer_length,
                           char* buffer);

ISC_STATUS isc_put_segment(ISC_STATUS* status_vector,
                           isc_blob_handle* blob_handle,
                           unsigned short length,
                           const char* buffer);

/* Misc */
ISC_STATUS isc_drop_database(ISC_STATUS* status_vector,
                             isc_db_handle* db_handle);

void isc_print_status(ISC_STATUS* status_vector);
void isc_print_sqlerror(long code, ISC_STATUS* status_vector);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* IBASE_MINIMAL_H */
