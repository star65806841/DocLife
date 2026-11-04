#pragma once

#include <QWidget>

namespace fluent::navigation { class StackContentHost; }

namespace doclife::ui {

    class TemplateSheetView;

    class MainContentView : public QWidget
    {
        Q_OBJECT
    public:
        explicit MainContentView(QWidget* parent = nullptr);

    private slots:
        // 接收左侧树发出的表格展示信号
        void onTemplateSheetActivated(const QString& dbPath, int sheetId, const QString& title);

    private:
        fluent::navigation::StackContentHost* m_host = nullptr;
        QWidget* m_blankPage = nullptr;
        TemplateSheetView* m_sheetView = nullptr;
    };

} // namespace doclife::ui