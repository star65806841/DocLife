#pragma once

#include <QString>
#include <QStringList>
#include <QHash>
#include <QRegularExpression>
#include <QVector>

namespace doclife::model {

    /**
     * 模板单元格里的字段占位符：#key。
     * 占位符原样保存在模板库里，打开表格时用当前工程/节点信息替换显示。
     */
    inline QStringList fieldTokenKeys()
    {
        return {
            QStringLiteral("projectName"),        // 工程名称
            QStringLiteral("projectCode"),        // 工程编号
            QStringLiteral("ownerUnit"),          // 建设单位
            QStringLiteral("constructionUnit"),   // 施工单位
            QStringLiteral("designUnit"),         // 设计单位
            QStringLiteral("supervisionUnit"),    // 监理单位
            QStringLiteral("unitName"),           // 最近的单位工程名称
            QStringLiteral("subName"),            // 最近的分部工程名称
            QStringLiteral("itemName"),           // 最近的分项工程名称
            QStringLiteral("sheetName"),          // 当前表格节点名称
            QStringLiteral("date"),               // 打开当天的日期
        };
    }

    // 字段说明，供配置文档/调试使用
    inline QStringList fieldTokenLabels()
    {
        return {
            QStringLiteral("工程名称"),
            QStringLiteral("工程编号"),
            QStringLiteral("建设单位"),
            QStringLiteral("施工单位"),
            QStringLiteral("设计单位"),
            QStringLiteral("监理单位"),
            QStringLiteral("单位工程名称"),
            QStringLiteral("分部工程名称"),
            QStringLiteral("分项工程名称"),
            QStringLiteral("表格名称"),
            QStringLiteral("当天日期"),
        };
    }

    inline const QRegularExpression& fieldTokenPattern()
    {
        static const QRegularExpression pattern(QStringLiteral("#([A-Za-z][A-Za-z0-9]*)"));
        return pattern;
    }

    // 文本里是否含有已知的字段占位符
    inline bool containsFieldToken(const QString& text)
    {
        if (text.isEmpty() || !text.contains(QLatin1Char('#'))) return false;
        const QStringList known = fieldTokenKeys();
        auto it = fieldTokenPattern().globalMatch(text);
        while (it.hasNext()) {
            if (known.contains(it.next().captured(1))) return true;
        }
        return false;
    }

    // 把 #key 替换成对应值；未知 key 保持原样，缺值替换为空串
    inline QString resolveFieldTokens(const QString& text, const QHash<QString, QString>& values)
    {
        if (text.isEmpty() || !text.contains(QLatin1Char('#'))) return text;

        struct Match { int start; int length; QString key; };
        QVector<Match> matches;

        auto it = fieldTokenPattern().globalMatch(text);
        while (it.hasNext()) {
            const auto match = it.next();
            const QString key = match.captured(1);
            if (!values.contains(key)) continue;   // 未知占位符原样保留
            matches.append({ int(match.capturedStart(0)), int(match.capturedLength(0)), key });
        }

        QString result = text;
        for (int i = matches.size() - 1; i >= 0; --i) {
            result.replace(matches[i].start, matches[i].length, values.value(matches[i].key));
        }
        return result;
    }

} // namespace doclife::model
