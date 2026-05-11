/**
 * @file MappingManager.cpp
 * @brief 映射管理器实现
 * @details 实现映射配置的加载、查询、字段映射和值转换功能。
 *          映射配置文件格式参见 config/mapping.json。
 */

#include "MappingManager.h"
#include <fstream>
#include <iostream>

namespace dbsync {

// ==================== 单例访问 ====================

MappingManager& MappingManager::GetInstance()
{
    static MappingManager instance;
    return instance;
}

// ==================== 加载映射配置 ====================

bool MappingManager::LoadMapping(const std::string& mappingFilePath)
{
    std::lock_guard<std::mutex> lock(mutex_);

    // 读取 JSON 文件
    std::ifstream ifs(mappingFilePath);
    if (!ifs.is_open()) {
        std::cerr << "[MappingManager] 无法打开映射配置文件: " << mappingFilePath << std::endl;
        return false;
    }

    // 解析 JSON
    Json::Value root;
    Json::CharReaderBuilder builder;
    std::string errs;
    if (!Json::parseFromStream(builder, ifs, &root, &errs)) {
        std::cerr << "[MappingManager] 解析映射配置文件失败: " << errs << std::endl;
        return false;
    }

    // 清空已有映射
    tableMappings_.clear();

    // 检查是否存在 table_mappings 数组
    if (!root.isMember("table_mappings") || !root["table_mappings"].isArray()) {
        std::cerr << "[MappingManager] 映射配置文件中缺少 table_mappings 数组" << std::endl;
        return false;
    }

    const Json::Value& mappings = root["table_mappings"];

    // 遍历每个表映射配置
    for (const auto& item : mappings) {
        TableMapping tm;

        // 解析基本信息
        tm.sourceTable = item.get("source_table", "").asString();
        tm.targetTable = item.get("target_table", "").asString();
        tm.sourceDb    = item.get("source_db", "").asString();
        tm.targetDb    = item.get("target_db", "").asString();

        // 解析主键映射
        if (item.isMember("primary_key")) {
            const Json::Value& pk = item["primary_key"];
            tm.sourcePrimaryKey = pk.get("source", "").asString();
            tm.targetPrimaryKey = pk.get("target", "").asString();
        }

        // 解析字段映射列表
        if (item.isMember("field_mappings") && item["field_mappings"].isArray()) {
            for (const auto& fm : item["field_mappings"]) {
                FieldMapping fieldMapping;
                fieldMapping.sourceField = fm.get("source", "").asString();
                fieldMapping.targetField = fm.get("target", "").asString();
                tm.fieldMappings.push_back(fieldMapping);
            }
        }

        // 解析值转换规则列表
        if (item.isMember("value_transforms") && item["value_transforms"].isArray()) {
            for (const auto& vt : item["value_transforms"]) {
                ValueTransform transform;
                transform.sourceField   = vt.get("source", "").asString();
                transform.targetField   = vt.get("target", "").asString();
                transform.transformType = vt.get("transform", "none").asString();
                tm.valueTransforms.push_back(transform);
            }
        }

        tableMappings_.push_back(tm);
    }

    std::cout << "[MappingManager] 成功加载 " << tableMappings_.size()
              << " 条表映射配置" << std::endl;
    return true;
}

// ==================== 查询映射 ====================

const TableMapping* MappingManager::FindMappingBySource(
    const std::string& sourceTable,
    const std::string& sourceDb) const
{
    for (const auto& tm : tableMappings_) {
        if (tm.sourceTable == sourceTable && tm.sourceDb == sourceDb) {
            return &tm;
        }
    }
    return nullptr;
}

const TableMapping* MappingManager::FindMappingByTarget(
    const std::string& targetTable,
    const std::string& targetDb) const
{
    for (const auto& tm : tableMappings_) {
        if (tm.targetTable == targetTable && tm.targetDb == targetDb) {
            return &tm;
        }
    }
    return nullptr;
}

bool MappingManager::HasMapping(const std::string& tableName) const
{
    for (const auto& tm : tableMappings_) {
        if (tm.sourceTable == tableName || tm.targetTable == tableName) {
            return true;
        }
    }
    return false;
}

// ==================== 获取映射字段列表 ====================

std::vector<std::string> MappingManager::GetMappedSourceFields(const std::string& sourceTable) const
{
    std::vector<std::string> fields;
    for (const auto& tm : tableMappings_) {
        if (tm.sourceTable == sourceTable) {
            for (const auto& fm : tm.fieldMappings) {
                fields.push_back(fm.sourceField);
            }
            break;
        }
    }
    return fields;
}

std::vector<std::string> MappingManager::GetMappedTargetFields(const std::string& targetTable) const
{
    std::vector<std::string> fields;
    for (const auto& tm : tableMappings_) {
        if (tm.targetTable == targetTable) {
            for (const auto& fm : tm.fieldMappings) {
                fields.push_back(fm.targetField);
            }
            break;
        }
    }
    return fields;
}

// ==================== 值转换 ====================

std::string MappingManager::TransformValue(const std::string& value, const std::string& transformType) const
{
    if (transformType == "invert") {
        // 0↔1 反转，用于 deleted↔is_active 等场景
        if (value == "0") {
            return "1";
        } else if (value == "1") {
            return "0";
        }
        // 非标准值不做转换，原样返回
        return value;
    }

    if (transformType == "timestamp_format") {
        // 时间戳格式转换（预留，可根据需要扩展）
        // TODO: 实现具体的时间戳格式转换逻辑
        return value;
    }

    // "none" 或未知类型，不转换
    return value;
}

// ==================== 记录转换 ====================

std::map<std::string, std::string> MappingManager::TransformRecord(
    const std::map<std::string, std::string>& sourceData,
    const TableMapping& mapping,
    bool sourceToTarget) const
{
    std::map<std::string, std::string> result;

    if (sourceToTarget) {
        // 源→目标方向：使用 sourceField 作为键查找，写入 targetField
        for (const auto& fm : mapping.fieldMappings) {
            auto it = sourceData.find(fm.sourceField);
            if (it != sourceData.end()) {
                result[fm.targetField] = it->second;
            }
        }

        // 应用值转换规则
        for (const auto& vt : mapping.valueTransforms) {
            auto it = sourceData.find(vt.sourceField);
            if (it != sourceData.end()) {
                result[vt.targetField] = TransformValue(it->second, vt.transformType);
            }
        }
    } else {
        // 目标→源方向（反向映射）：使用 targetField 作为键查找，写入 sourceField
        for (const auto& fm : mapping.fieldMappings) {
            auto it = sourceData.find(fm.targetField);
            if (it != sourceData.end()) {
                result[fm.sourceField] = it->second;
            }
        }

        // 应用值转换规则（反向）
        for (const auto& vt : mapping.valueTransforms) {
            auto it = sourceData.find(vt.targetField);
            if (it != sourceData.end()) {
                result[vt.sourceField] = TransformValue(it->second, vt.transformType);
            }
        }
    }

    return result;
}

} // namespace dbsync
