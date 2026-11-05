#pragma once

#include <QString>

namespace doclife::model {

    /**
     * 单元格角色：决定编辑时的底色与是否可改。
     * 持久化在 sheet_cells.cell_role，取值为下面的 key。
     */
    enum class CellRole {
        Unknown,    // 旧数据/未标记：按普通单元格处理（可编辑）
        Title,      // 表名/标题：只读
        ReadOnly,   // 固定文字：只读，双击可解锁
        Project,    // 工程信息：只读，由工程信息自动填充
        Input,      // 用户填写：可编辑
        Summary,    // 汇总/评价：只读，由计算写入
        Date,       // 时间区域：双击弹日期选择器
    };

    inline QString cellRoleKey(CellRole role)
    {
        switch (role) {
        case CellRole::Title:    return QStringLiteral("title");
        case CellRole::ReadOnly: return QStringLiteral("readonly");
        case CellRole::Project:  return QStringLiteral("project");
        case CellRole::Input:    return QStringLiteral("input");
        case CellRole::Summary:  return QStringLiteral("summary");
        case CellRole::Date:     return QStringLiteral("date");
        default:                 return QString();
        }
    }

    inline CellRole cellRoleFromKey(const QString& key)
    {
        if (key == QLatin1String("title")) return CellRole::Title;
        if (key == QLatin1String("readonly")) return CellRole::ReadOnly;
        if (key == QLatin1String("project")) return CellRole::Project;
        if (key == QLatin1String("input")) return CellRole::Input;
        if (key == QLatin1String("summary")) return CellRole::Summary;
        if (key == QLatin1String("date")) return CellRole::Date;
        return CellRole::Unknown;
    }

    inline bool cellRoleEditable(CellRole role)
    {
        return role == CellRole::Input || role == CellRole::Unknown;
    }

} // namespace doclife::model
