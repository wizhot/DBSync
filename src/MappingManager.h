/**
 * @file MappingManager.h
 * @brief 映射管理器头文件
 * @details 管理 SQLite 和 Firebird 之间的表字段映射规则，
 *          支持字段名映射和值转换（如 0↔1 反转）。
 *          映射配置从 config/mapping.json 加载。
 */

#pragma once

#include "Common.h"
#include <json/json.h>

namespace dbsync {

/**
 * @brief 单个字段映射
 * @details 描述源字段与目标字段之间的对应关系
 */
struct FieldMapping {
    std::string sourceField;   ///< 源字段名
    std::string targetField;   ///< 目标字段名
};

/**
 * @brief 值转换规则
 * @details 描述源字段值到目标字段值的转换方式
 */
struct ValueTransform {
    std::string sourceField;       ///< 源字段名
    std::string targetField;       ///< 目标字段名
    std::string transformType;     ///< 转换类型: "invert"(0↔1), "timestamp_format", "none"
};

/**
 * @brief 单表映射配置
 * @details 描述一张表从源数据库到目标数据库的完整映射关系
 */
struct TableMapping {
    std::string sourceTable;           ///< 源表名
    std::string targetTable;           ///< 目标表名
    std::string sourceDb;              ///< 源数据库类型: "sqlite" 或 "firebird"
    std::string targetDb;              ///< 目标数据库类型: "sqlite" 或 "firebird"
    std::string sourcePrimaryKey;      ///< 源表主键字段名
    std::string targetPrimaryKey;      ///< 目标表主键字段名
    std::vector<FieldMapping> fieldMappings;       ///< 字段映射列表
    std::vector<ValueTransform> valueTransforms;   ///< 值转换规则列表
};

/**
 * @class MappingManager
 * @brief 映射管理器（单例模式）
 * @details 负责：
 *          - 从 JSON 配置文件加载映射规则
 *          - 按源表名或目标表名查找映射
 *          - 执行字段名映射和值转换
 *          - 线程安全（内部使用 mutex）
 */
class MappingManager {
public:
    /**
     * @brief 获取单例实例
     * @return 映射管理器的引用
     */
    static MappingManager& GetInstance();

    /**
     * @brief 加载映射配置
     * @param mappingFilePath 映射配置文件路径（JSON 格式）
     * @return 成功返回 true，失败返回 false
     */
    bool LoadMapping(const std::string& mappingFilePath);

    /**
     * @brief 获取所有表映射配置
     * @return 表映射配置列表的常引用
     */
    const std::vector<TableMapping>& GetTableMappings() const { return tableMappings_; }

    /**
     * @brief 根据源表名和源数据库类型查找映射
     * @param sourceTable 源表名
     * @param sourceDb 源数据库类型（"sqlite" 或 "firebird"）
     * @return 匹配的映射指针，未找到返回 nullptr
     */
    const TableMapping* FindMappingBySource(const std::string& sourceTable, const std::string& sourceDb) const;

    /**
     * @brief 根据目标表名和目标数据库类型查找映射
     * @param targetTable 目标表名
     * @param targetDb 目标数据库类型（"sqlite" 或 "firebird"）
     * @return 匹配的映射指针，未找到返回 nullptr
     */
    const TableMapping* FindMappingByTarget(const std::string& targetTable, const std::string& targetDb) const;

    /**
     * @brief 检查表是否有映射配置
     * @param tableName 表名（同时匹配源表和目标表）
     * @return 存在映射返回 true
     */
    bool HasMapping(const std::string& tableName) const;

    /**
     * @brief 获取源表的映射字段列表
     * @param sourceTable 源表名
     * @return 源字段名列表
     */
    std::vector<std::string> GetMappedSourceFields(const std::string& sourceTable) const;

    /**
     * @brief 获取目标表的映射字段列表
     * @param targetTable 目标表名
     * @return 目标字段名列表
     */
    std::vector<std::string> GetMappedTargetFields(const std::string& targetTable) const;

    /**
     * @brief 根据转换类型对单个值进行转换
     * @param value 原始值
     * @param transformType 转换类型: "invert", "timestamp_format", "none"
     * @return 转换后的值
     */
    std::string TransformValue(const std::string& value, const std::string& transformType) const;

    /**
     * @brief 根据映射规则将源数据转换为目标数据
     * @param sourceData 源数据（字段名→值的映射）
     * @param mapping 表映射配置
     * @param sourceToTarget 转换方向: true=源→目标, false=目标→源
     * @return 转换后的数据（目标字段名→值的映射）
     */
    std::map<std::string, std::string> TransformRecord(
        const std::map<std::string, std::string>& sourceData,
        const TableMapping& mapping,
        bool sourceToTarget = true) const;

private:
    MappingManager() = default;
    ~MappingManager() = default;
    MappingManager(const MappingManager&) = delete;
    MappingManager& operator=(const MappingManager&) = delete;

    std::vector<TableMapping> tableMappings_;  ///< 所有表映射配置
    std::mutex mutex_;                          ///< 线程安全互斥锁
};

} // namespace dbsync
