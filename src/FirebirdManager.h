#pragma once

#include "Common.h"

namespace dbsync {

// Firebird数据库连接管理类
class FirebirdManager {
public:
    FirebirdManager();
    ~FirebirdManager();
    
    // 连接管理
    bool Connect(const DatabaseConfig& config);
    void Disconnect();
    bool IsConnected() const;
    bool Reconnect();
    
    // 基本查询操作
    bool Execute(const std::string& sql);
    bool Query(const std::string& sql, std::vector<std::map<std::string, std::string>>& results);
    
    // 事务管理
    bool StartTransaction();
    bool Commit();
    bool Rollback();
    
    // 数据库元数据
    bool GetTables(std::vector<std::string>& tables);
    bool GetTableColumns(const std::string& tableName, std::vector<std::string>& columns);
    bool GetPrimaryKey(const std::string& tableName, std::string& primaryKey);
    bool GetTableData(const std::string& tableName, std::vector<std::map<std::string, std::string>>& data);
    
    // 变更追踪相关
    bool SetupChangeTracking();
    bool CreateChangeLogTable();
    bool CreateSyncTriggers();
    bool GetPendingChanges(std::vector<ChangeRecord>& changes);
    bool MarkChangeAsSynced(const std::string& changeId);
    bool ClearSyncedChanges();
    
    // 数据操作
    bool InsertRecord(const std::string& tableName, const std::map<std::string, std::string>& data);
    bool UpdateRecord(const std::string& tableName, const std::string& primaryKey, 
                      const std::string& primaryKeyValue, const std::map<std::string, std::string>& data);
    bool DeleteRecord(const std::string& tableName, const std::string& primaryKey, 
                      const std::string& primaryKeyValue);
    
    // 获取最后错误信息
    std::string GetLastError() const { return lastError_; }
    
private:
    // ISC API 辅助函数
    bool CheckError(ISC_STATUS* status);
    std::string GetIscErrorMessage(ISC_STATUS* status);
    std::string EscapeString(const std::string& str);
    
    // 构建连接字符串
    std::string BuildConnectionString(const DatabaseConfig& config);
    
    ISC_STATUS statusVector_[20];
    isc_db_handle dbHandle_;
    isc_tr_handle trHandle_;
    isc_stmt_handle stmtHandle_;
    
    DatabaseConfig config_;
    std::string lastError_;
    bool connected_;
    std::mutex mutex_;
};

} // namespace dbsync
