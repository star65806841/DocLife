#pragma once

#include <QWidget>
#include <QModelIndex>

namespace fluent::collections { class TreeView; }

namespace doclife::ui {

    // 公共分发：json/pdf → pdf 预览；xlsx → excel 预览
    void activateTemplateFile(const QString& filePath, const QString& suffix);

    class CatalogView : public QWidget
    {
        Q_OBJECT
    public:
        explicit CatalogView(QWidget* parent = nullptr);

    private:
        void onItemActivated(const QModelIndex& index);

        fluent::collections::TreeView* m_tree = nullptr;
    };

} // namespace doclife::ui