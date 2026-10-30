#pragma once

#include <QWidget>

class QTableWidget;
class QLabel;

namespace doclife::ui {

    class TemplateSheetView : public QWidget
    {
        Q_OBJECT
    public:
        explicit TemplateSheetView(QWidget* parent = nullptr);

        // 从 db 加载某个 sheet 的完整渲染
        void loadSheet(const QString& dbPath, int sheetId, const QString& title);

    private:
        QLabel* m_titleLabel = nullptr;
        QTableWidget* m_table = nullptr;
    };

} // namespace doclife::ui