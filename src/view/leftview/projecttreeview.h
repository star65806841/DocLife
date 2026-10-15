#pragma once

#include <QWidget>
#include <QStandardItem>
#include "projectnode.h"

class QStandardItemModel;

namespace fluent::collections { class TreeView; }

namespace doclife::ui {

class ProjectTreeView : public QWidget {
    Q_OBJECT
public:
    explicit ProjectTreeView(QWidget* parent = nullptr);

private slots:
    void rebuildAll();                       // 项目打开/创建/关闭时全量重建

private:
    QStandardItem* makeItem(const doclife::model::ProjectNode& node);
    static void    applyFlags(QStandardItem* item, const QString& type);

    QStandardItemModel*            m_model = nullptr;
    fluent::collections::TreeView* m_tree  = nullptr;
};

} // namespace doclife::ui