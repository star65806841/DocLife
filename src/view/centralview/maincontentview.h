#pragma once

#include <QWidget>

namespace fluent::navigation { class StackContentHost; }

namespace doclife::ui {

class TemplatePreviewView;

class MainContentView : public QWidget
{
    Q_OBJECT
public:
    explicit MainContentView(QWidget *parent = nullptr);

private slots:
    void onSheetActivated(const QString &xlsxPath, const QString &sheetName);

private:
    fluent::navigation::StackContentHost *m_host = nullptr;
    TemplatePreviewView *m_preview = nullptr;
    QWidget          *m_blankPage = nullptr;
};

} // namespace doclife::ui