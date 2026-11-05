#pragma once

#include <QChar>
#include <QString>
#include <QStringList>
#include <QStringView>

namespace doclife::model {

    /**
     * 检验批表格里的「方格选项」。
     *
     * 模板里的方格统一写成 □（U+25A1）。实测宋体、黑体都没有 ☑（U+2611）和 ✓（U+2713），
     * 只有 Segoe UI Symbol 有，Qt 缺字时会自动回退到它，显示效果就是一个带勾的方框。
     * 如果不想依赖字体回退，把 boxCheckedChar() 换成 ■（U+25A0）即可：
     * 宋体自带、字宽与 □ 完全一致，零排版风险，只是外观是实心块而不是勾。
     */

    inline QChar boxEmptyChar() { return QChar(0x25A1); }     // □ 未勾选
    inline QChar boxCheckedChar() { return QChar(0x2611); }   // ☑ 已勾选

    // 方格后面紧跟这些词时保持不勾：检验批的「□合格  □不合格」只有合格那个该打勾
    inline QStringList boxFailureKeywords()
    {
        return { QStringLiteral("不合格") };
    }

    // 第 boxIndex 个字符是一个方格，它后面（跳过空格）是不是「不合格」这类否定项
    inline bool boxMarksFailure(const QString& text, int boxIndex)
    {
        int i = boxIndex + 1;
        while (i < text.size() && text.at(i).isSpace()) ++i;

        const QStringView rest = QStringView(text).mid(i);
        for (const QString& keyword : boxFailureKeywords()) {
            if (rest.startsWith(keyword)) return true;
        }
        return false;
    }

    inline bool containsEmptyBox(const QString& text)
    {
        return text.contains(boxEmptyChar());
    }

    inline bool containsCheckedBox(const QString& text)
    {
        return text.contains(boxCheckedChar());
    }

    /**
     * 勾选：把 □ 换成 ☑，但「□不合格」这类否定项前的方格保持 □。
     * 取消：把 ☑ 换回 □（只认这个字符，不猜别的勾选写法）。
     * 其余文字、空格、换行原样保留，字宽不变，不会引起重新换行。
     */
    inline QString applyBoxMarks(const QString& text, bool checked)
    {
        const QChar empty = boxEmptyChar();
        const QChar marked = boxCheckedChar();

        if (text.isEmpty()) return text;
        if (checked ? !text.contains(empty) : !text.contains(marked)) return text;

        QString out;
        out.reserve(text.size());

        for (int i = 0; i < text.size(); ++i) {
            const QChar ch = text.at(i);
            if (checked) {
                out.append(ch == empty && !boxMarksFailure(text, i) ? marked : ch);
            }
            else {
                out.append(ch == marked ? empty : ch);
            }
        }
        return out;
    }

} // namespace doclife::model
