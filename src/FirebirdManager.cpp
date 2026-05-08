#include "FirebirdManager.h"
#include "Logger.h"
#include <cstring>

namespace dbsync {

FirebirdManager::FirebirdManager() 
    : dbHandle_(0), trHandle_(0), stmtHandle_(0), connected_(false) {
    memset(statusVector_, 0, sizeof(statusVector_));
}

FirebirdManager::~FirebirdManager() {
    Disconnect();
}

bool FirebirdManager::Connect(const DatabaseConfig& config) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (connected_) {
        Disconnect();
    }
    
    config_ = config;
    
    // 构建连接字符串
    std::string connStr = BuildConnectionString(config);
    
    // 连接数据库
    char dpb_buffer[256];
    char* dpb = dpb_buffer;
    
    *dpb++ = isc_dpb_version1;
    *dpb++ = isc_dpb_user_name;
    *dpb++ = static_cast<char>(config.username.length());
    memcpy(dpb, config.username.c_str(), config.username.length());
    dpb += config.username.length();
    
    *dpb++ = isc_dpb_password;
    *dpb++ = static_cast<char>(config.password.length());
    memcpy(dpb, config.password.c_str(), config.password.length());
    dpb += config.password.length();
    
    *dpb++ = isc_dpb_lc_ctype;
    *dpb++ = static_cast<char>(config.charset.length());
    memcpy(dpb, config.charset.c_str(), config.charset.length());
    dpb += config.charset.length();
    
    short dpb_length = static_cast<short>(dpb - dpb_buffer);
    
    if (isc_attach_database(statusVector_, 0, connStr.c_str(), &dbHandle_, 
                           dpb_length, dpb_buffer)) {
        lastError_ = GetIscErrorMessage(statusVector_);
        LOG_ERROR("Failed to connect to database: " + lastError_);
        return false;
    }
    
    connected_ = true;
    LOG_INFO("Connected to Firebird database: " + config.database);
    return true;
}

void FirebirdManager::Disconnect() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (stmtHandle_) {
        isc_dsql_free_statement(statusVector_, &stmtHandle_, DSQL_drop);
        stmtHandle_ = 0;
    }
    
    if (trHandle_) {
        isc_commit_transaction(statusVector_, &trHandle_);
        trHandle_ = 0;
    }
    
    if (dbHandle_) {
        isc_detach_database(statusVector_, &dbHandle_);
        dbHandle_ = 0;
    }
    
    connected_ = false;
    LOG_INFO("Disconnected from database");
}

bool FirebirdManager::IsConnected() const {
    return connected_;
}

bool FirebirdManager::Reconnect() {
    Disconnect();
    return Connect(config_);
}

bool FirebirdManager::Execute(const std::string& sql) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!connected_) {
        lastError_ = "Not connected to database";
        return false;
    }
    
    // 开始事务
    if (!StartTransaction()) {
        return false;
    }
    
    // 分配语句
    if (isc_dsql_allocate_statement(statusVector_, &dbHandle_, &stmtHandle_)) {
        lastError_ = GetIscErrorMessage(statusVector_);
        Rollback();
        return false;
    }
    
    // 执行SQL
    if (isc_dsql_execute_immediate(statusVector_, &dbHandle_, &trHandle_, 0, 
                                   sql.c_str(), SQL_DIALECT_V6, nullptr)) {
        lastError_ = GetIscErrorMessage(statusVector_);
        isc_dsql_free_statement(statusVector_, &stmtHandle_, DSQL_drop);
        stmtHandle_ = 0;
        Rollback();
        return false;
    }
    
    // 释放语句
    isc_dsql_free_statement(statusVector_, &stmtHandle_, DSQL_drop);
    stmtHandle_ = 0;
    
    // 提交事务
    if (!Commit()) {
        return false;
    }
    
    return true;
}

bool FirebirdManager::Query(const std::string& sql, 
                            std::vector<std::map<std::string, std::string>>& results) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!connected_) {
        lastError_ = "Not connected to database";
        return false;
    }
    
    results.clear();
    
    // 开始事务
    if (!StartTransaction()) {
        return false;
    }
    
    // 分配语句
    if (isc_dsql_allocate_statement(statusVector_, &dbHandle_, &stmtHandle_)) {
        lastError_ = GetIscErrorMessage(statusVector_);
        Rollback();
        return false;
    }
    
    // 准备语句
    XSQLDA* xsqlda = (XSQLDA*)malloc(XSQLDA_LENGTH(10));
    xsqlda->version = SQLDA_VERSION1;
    xsqlda->sqln = 10;
    
    if (isc_dsql_prepare(statusVector_, &trHandle_, &stmtHandle_, 0, 
                         sql.c_str(), SQL_DIALECT_V6, xsqlda)) {
        lastError_ = GetIscErrorMessage(statusVector_);
        free(xsqlda);
        isc_dsql_free_statement(statusVector_, &stmtHandle_, DSQL_drop);
        stmtHandle_ = 0;
        Rollback();
        return false;
    }
    
    // 描述字段
    if (isc_dsql_describe(statusVector_, &stmtHandle_, SQL_DIALECT_V6, xsqlda)) {
        lastError_ = GetIscErrorMessage(statusVector_);
        free(xsqlda);
        isc_dsql_free_statement(statusVector_, &stmtHandle_, DSQL_drop);
        stmtHandle_ = 0;
        Rollback();
        return false;
    }
    
    // 为字段分配内存
    if (xsqlda->sqld > xsqlda->sqln) {
        int n = xsqlda->sqld;
        free(xsqlda);
        xsqlda = (XSQLDA*)malloc(XSQLDA_LENGTH(n));
        xsqlda->version = SQLDA_VERSION1;
        xsqlda->sqln = n;
        
        if (isc_dsql_describe(statusVector_, &stmtHandle_, SQL_DIALECT_V6, xsqlda)) {
            lastError_ = GetIscErrorMessage(statusVector_);
            free(xsqlda);
            isc_dsql_free_statement(statusVector_, &stmtHandle_, DSQL_drop);
            stmtHandle_ = 0;
            Rollback();
            return false;
        }
    }
    
    // 为每个字段分配缓冲区
    for (int i = 0; i < xsqlda->sqld; i++) {
        XSQLVAR* var = &xsqlda->sqlvar[i];
        var->sqldata = (char*)malloc(var->sqllen + 2);
        var->sqlind = (short*)malloc(sizeof(short));
    }
    
    // 执行查询
    if (isc_dsql_execute(statusVector_, &trHandle_, &stmtHandle_, SQL_DIALECT_V6, nullptr)) {
        lastError_ = GetIscErrorMessage(statusVector_);
        for (int i = 0; i < xsqlda->sqld; i++) {
            free(xsqlda->sqlvar[i].sqldata);
            free(xsqlda->sqlvar[i].sqlind);
        }
        free(xsqlda);
        isc_dsql_free_statement(statusVector_, &stmtHandle_, DSQL_drop);
        stmtHandle_ = 0;
        Rollback();
        return false;
    }
    
    // 获取结果
    ISC_STATUS fetchStatus;
    while ((fetchStatus = isc_dsql_fetch(statusVector_, &stmtHandle_, SQL_DIALECT_V6, xsqlda)) == 0) {
        std::map<std::string, std::string> row;
        for (int i = 0; i < xsqlda->sqld; i++) {
            XSQLVAR* var = &xsqlda->sqlvar[i];
            std::string colName = var->sqlname;
            
            if (*var->sqlind == -1) {
                row[colName] = "NULL";
            } else {
                switch (var->sqltype & ~1) {
                    case SQL_TEXT:
                    case SQL_VARYING:
                        row[colName] = std::string(var->sqldata, var->sqllen);
                        break;
                    case SQL_SHORT:
                        row[colName] = std::to_string(*(short*)var->sqldata);
                        break;
                    case SQL_LONG:
                        row[colName] = std::to_string(*(long*)var->sqldata);
                        break;
                    case SQL_INT64:
                        row[colName] = std::to_string(*(ISC_INT64*)var->sqldata);
                        break;
                    case SQL_FLOAT:
                        row[colName] = std::to_string(*(float*)var->sqldata);
                        break;
                    case SQL_DOUBLE:
                        row[colName] = std::to_string(*(double*)var->sqldata);
                        break;
                    case SQL_TIMESTAMP:
                    case SQL_TYPE_DATE:
                    case SQL_TYPE_TIME:
                        row[colName] = std::string(var->sqldata, var->sqllen);
                        break;
                    default:
                        row[colName] = std::string(var->sqldata, var->sqllen);
                        break;
                }
            }
        }
        results.push_back(row);
    }
    
    // 清理
    for (int i = 0; i < xsqlda->sqld; i++) {
        free(xsqlda->sqlvar[i].sqldata);
        free(xsqlda->sqlvar[i].sqlind);
    }
    free(xsqlda);
    
    isc_dsql_free_statement(statusVector_, &stmtHandle_, DSQL_drop);
    stmtHandle_ = 0;
    
    if (!Commit()) {
        return false;
    }
    
    return true;
}

bool FirebirdManager::StartTransaction() {
    char tpb[] = {isc_tpb_version3, isc_tpb_write, isc_tpb_wait, isc_tpb_read_committed, isc_tpb_no_rec_version};
    if (isc_start_transaction(statusVector_, &trHandle_, 1, &dbHandle_, sizeof(tpb), tpb)) {
        lastError_ = GetIscErrorMessage(statusVector_);
        return false;
    }
    return true;
}

bool FirebirdManager::Commit() {
    if (trHandle_) {
        if (isc_commit_transaction(statusVector_, &trHandle_)) {
            lastError_ = GetIscErrorMessage(statusVector_);
            trHandle_ = 0;
            return false;
        }
        trHandle_ = 0;
    }
    return true;
}

bool FirebirdManager::Rollback() {
    if (trHandle_) {
        isc_rollback_transaction(statusVector_, &trHandle_);
        trHandle_ = 0;
    }
    return true;
}

bool FirebirdManager::GetTables(std::vector<std::string>& tables) {
    tables.clear();
    std::string sql = "SELECT RDB$RELATION_NAME FROM RDB$RELATIONS "
                      "WHERE RDB$SYSTEM_FLAG = 0 AND RDB$RELATION_TYPE = 0 "
                      "ORDER BY RDB$RELATION_NAME";
    
    std::vector<std::map<std::string, std::string>> results;
    if (!Query(sql, results)) {
        return false;
    }
    
    for (const auto& row : results) {
        auto it = row.find("RDB$RELATION_NAME");
        if (it != row.end()) {
            std::string tableName = it->second;
            // 去除尾部空格
            tableName.erase(tableName.find_last_not_of(" ") + 1);
            tables.push_back(tableName);
        }
    }
    
    return true;
}

bool FirebirdManager::GetTableColumns(const std::string& tableName, 
                                       std::vector<std::string>& columns) {
    columns.clear();
    std::string sql = "SELECT RDB$FIELD_NAME FROM RDB$RELATION_FIELDS "
                      "WHERE RDB$RELATION_NAME = '" + tableName + "' "
                      "ORDER BY RDB$FIELD_POSITION";
    
    std::vector<std::map<std::string, std::string>> results;
    if (!Query(sql, results)) {
        return false;
    }
    
    for (const auto& row : results) {
        auto it = row.find("RDB$FIELD_NAME");
        if (it != row.end()) {
            std::string colName = it->second;
            colName.erase(colName.find_last_not_of(" ") + 1);
            columns.push_back(colName);
        }
    }
    
    return true;
}

bool FirebirdManager::GetPrimaryKey(const std::string& tableName, std::string& primaryKey) {
    std::string sql = "SELECT RDB$FIELD_NAME FROM RDB$INDEX_SEGMENTS "
                      "WHERE RDB$INDEX_NAME IN (" 
                      "SELECT RDB$INDEX_NAME FROM RDB$INDICES "
                      "WHERE RDB$RELATION_NAME = '" + tableName + "' "
                      "AND RDB$UNIQUE_FLAG = 1 AND RDB$INDEX_NAME LIKE 'RDB$PRIMARY%')";
    
    std::vector<std::map<std::string, std::string>> results;
    if (!Query(sql, results) || results.empty()) {
        return false;
    }
    
    auto it = results[0].find("RDB$FIELD_NAME");
    if (it != results[0].end()) {
        primaryKey = it->second;
        primaryKey.erase(primaryKey.find_last_not_of(" ") + 1);
        return true;
    }
    
    return false;
}

bool FirebirdManager::GetTableData(const std::string& tableName, 
                                    std::vector<std::map<std::string, std::string>>& data) {
    std::string sql = "SELECT * FROM " + tableName;
    return Query(sql, data);
}

bool FirebirdManager::SetupChangeTracking() {
    // 创建变更日志表
    if (!CreateChangeLogTable()) {
        return false;
    }
    
    // 创建同步触发器
    if (!CreateSyncTriggers()) {
        return false;
    }
    
    return true;
}

bool FirebirdManager::CreateChangeLogTable() {
    std::string sql = 
        "CREATE TABLE IF NOT EXISTS DBSYNC_CHANGE_LOG ("
        "    CHANGE_ID VARCHAR(36) NOT NULL PRIMARY KEY,"
        "    TABLE_NAME VARCHAR(128) NOT NULL,"
        "    CHANGE_TYPE VARCHAR(10) NOT NULL,"
        "    PRIMARY_KEY VARCHAR(128),"
        "    PRIMARY_KEY_VALUE VARCHAR(255),"
        "    DATA BLOB SUB_TYPE TEXT,"
        "    TIMESTAMP TIMESTAMP DEFAULT CURRENT_TIMESTAMP,"
        "    SOURCE_NODE VARCHAR(36),"
        "    STATUS VARCHAR(20) DEFAULT 'PENDING',"
        "    RETRY_COUNT INTEGER DEFAULT 0"
        ")";
    
    return Execute(sql);
}

bool FirebirdManager::CreateSyncTriggers() {
    std::vector<std::string> tables;
    if (!GetTables(tables)) {
        return false;
    }
    
    for (const auto& table : tables) {
        // 跳过系统表
        if (table == "DBSYNC_CHANGE_LOG") continue;
        
        std::string primaryKey;
        if (!GetPrimaryKey(table, primaryKey)) {
            LOG_WARNING("Table " + table + " has no primary key, skipping trigger creation");
            continue;
        }
        
        // 创建INSERT触发器
        std::string insertTrigger = 
            "CREATE OR ALTER TRIGGER DBSYNC_" + table + "_INSERT FOR " + table + " "
            "ACTIVE AFTER INSERT POSITION 32767 AS "
            "BEGIN "
            "    INSERT INTO DBSYNC_CHANGE_LOG (CHANGE_ID, TABLE_NAME, CHANGE_TYPE, PRIMARY_KEY, PRIMARY_KEY_VALUE, SOURCE_NODE) "
            "    VALUES (GEN_UUID(), '" + table + "', 'INSERT', '" + primaryKey + "', NEW." + primaryKey + ", NULL); "
            "END";
        
        if (!Execute(insertTrigger)) {
            LOG_ERROR("Failed to create INSERT trigger for table " + table + ": " + lastError_);
        }
        
        // 创建UPDATE触发器
        std::string updateTrigger = 
            "CREATE OR ALTER TRIGGER DBSYNC_" + table + "_UPDATE FOR " + table + " "
            "ACTIVE AFTER UPDATE POSITION 32767 AS "
            "BEGIN "
            "    INSERT INTO DBSYNC_CHANGE_LOG (CHANGE_ID, TABLE_NAME, CHANGE_TYPE, PRIMARY_KEY, PRIMARY_KEY_VALUE, SOURCE_NODE) "
            "    VALUES (GEN_UUID(), '" + table + "', 'UPDATE', '" + primaryKey + "', NEW." + primaryKey + ", NULL); "
            "END";
        
        if (!Execute(updateTrigger)) {
            LOG_ERROR("Failed to create UPDATE trigger for table " + table + ": " + lastError_);
        }
        
        // 创建DELETE触发器
        std::string deleteTrigger = 
            "CREATE OR ALTER TRIGGER DBSYNC_" + table + "_DELETE FOR " + table + " "
            "ACTIVE AFTER DELETE POSITION 32767 AS "
            "BEGIN "
            "    INSERT INTO DBSYNC_CHANGE_LOG (CHANGE_ID, TABLE_NAME, CHANGE_TYPE, PRIMARY_KEY, PRIMARY_KEY_VALUE, SOURCE_NODE) "
            "    VALUES (GEN_UUID(), '" + table + "', 'DELETE', '" + primaryKey + "', OLD." + primaryKey + ", NULL); "
            "END";
        
        if (!Execute(deleteTrigger)) {
            LOG_ERROR("Failed to create DELETE trigger for table " + table + ": " + lastError_);
        }
    }
    
    return true;
}

bool FirebirdManager::GetPendingChanges(std::vector<ChangeRecord>& changes) {
    changes.clear();
    std::string sql = 
        "SELECT CHANGE_ID, TABLE_NAME, CHANGE_TYPE, PRIMARY_KEY, PRIMARY_KEY_VALUE, "
        "DATA, TIMESTAMP, SOURCE_NODE, STATUS, RETRY_COUNT "
        "FROM DBSYNC_CHANGE_LOG "
        "WHERE STATUS = 'PENDING' OR (STATUS = 'FAILED' AND RETRY_COUNT < 3) "
        "ORDER BY TIMESTAMP";
    
    std::vector<std::map<std::string, std::string>> results;
    if (!Query(sql, results)) {
        return false;
    }
    
    for (const auto& row : results) {
        ChangeRecord record;
        record.id = row.at("CHANGE_ID");
        record.tableName = row.at("TABLE_NAME");
        
        std::string changeType = row.at("CHANGE_TYPE");
        if (changeType == "INSERT") record.changeType = ChangeType::INSERT;
        else if (changeType == "UPDATE") record.changeType = ChangeType::UPDATE;
        else if (changeType == "DELETE") record.changeType = ChangeType::DELETE;
        
        record.primaryKey = row.at("PRIMARY_KEY");
        record.primaryKeyValue = row.at("PRIMARY_KEY_VALUE");
        record.data = row.count("DATA") ? row.at("DATA") : "";
        record.timestamp = row.at("TIMESTAMP");
        record.sourceNode = row.count("SOURCE_NODE") ? row.at("SOURCE_NODE") : "";
        
        std::string status = row.at("STATUS");
        if (status == "PENDING") record.status = SyncStatus::PENDING;
        else if (status == "SYNCING") record.status = SyncStatus::SYNCING;
        else if (status == "SUCCESS") record.status = SyncStatus::SUCCESS;
        else if (status == "FAILED") record.status = SyncStatus::FAILED;
        else if (status == "CONFLICT") record.status = SyncStatus::CONFLICT;
        
        record.retryCount = std::stoi(row.at("RETRY_COUNT"));
        changes.push_back(record);
    }
    
    return true;
}

bool FirebirdManager::MarkChangeAsSynced(const std::string& changeId) {
    std::string sql = "UPDATE DBSYNC_CHANGE_LOG SET STATUS = 'SUCCESS' WHERE CHANGE_ID = '" + changeId + "'";
    return Execute(sql);
}

bool FirebirdManager::ClearSyncedChanges() {
    std::string sql = "DELETE FROM DBSYNC_CHANGE_LOG WHERE STATUS = 'SUCCESS'";
    return Execute(sql);
}

bool FirebirdManager::InsertRecord(const std::string& tableName, 
                                    const std::map<std::string, std::string>& data) {
    std::string columns, values;
    for (const auto& pair : data) {
        if (!columns.empty()) {
            columns += ", ";
            values += ", ";
        }
        columns += pair.first;
        values += "'" + EscapeString(pair.second) + "'";
    }
    
    std::string sql = "INSERT INTO " + tableName + " (" + columns + ") VALUES (" + values + ")";
    return Execute(sql);
}

bool FirebirdManager::UpdateRecord(const std::string& tableName, 
                                    const std::string& primaryKey,
                                    const std::string& primaryKeyValue,
                                    const std::map<std::string, std::string>& data) {
    std::string setClause;
    for (const auto& pair : data) {
        if (pair.first == primaryKey) continue; // 不更新主键
        if (!setClause.empty()) {
            setClause += ", ";
        }
        setClause += pair.first + " = '" + EscapeString(pair.second) + "'";
    }
    
    std::string sql = "UPDATE " + tableName + " SET " + setClause + 
                      " WHERE " + primaryKey + " = '" + EscapeString(primaryKeyValue) + "'";
    return Execute(sql);
}

bool FirebirdManager::DeleteRecord(const std::string& tableName, 
                                    const std::string& primaryKey,
                                    const std::string& primaryKeyValue) {
    std::string sql = "DELETE FROM " + tableName + 
                      " WHERE " + primaryKey + " = '" + EscapeString(primaryKeyValue) + "'";
    return Execute(sql);
}

std::string FirebirdManager::BuildConnectionString(const DatabaseConfig& config) {
    std::string connStr;
    
    // 嵌入式模式：直接使用数据库文件路径
    if (config.embedded) {
        connStr = config.database;
        LOG_DEBUG("Using embedded mode, database path: " + connStr);
    }
    // 服务器模式
    else if (config.host == "localhost" || config.host == "127.0.0.1") {
        connStr = config.database;
    } else {
        connStr = config.host + "/" + std::to_string(config.port) + ":" + config.database;
    }
    
    return connStr;
}

std::string FirebirdManager::GetIscErrorMessage(ISC_STATUS* status) {
    std::string message;
    char msgBuffer[512];
    
    while (isc_interprete(msgBuffer, &status)) {
        if (!message.empty()) {
            message += "; ";
        }
        message += msgBuffer;
    }
    
    return message.empty() ? "Unknown error" : message;
}

std::string FirebirdManager::EscapeString(const std::string& str) {
    std::string result;
    result.reserve(str.length() * 2);
    for (char c : str) {
        if (c == '\'') {
            result += "''";
        } else {
            result += c;
        }
    }
    return result;
}

} // namespace dbsync
