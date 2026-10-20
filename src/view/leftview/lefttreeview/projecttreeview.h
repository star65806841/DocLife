#pragma once

#include <QWidget>
#include <QPoint>

class QModelIndex;

namespace fluent::collections { class TreeView; }
namespace doclife::model {
class ProjectTreeModel;
}

namespace doclife::ui {

class ProjectTreeView : public QWidget
{
    Q_OBJECT
public:
    explicit ProjectTreeView(QWidget* parent = nullptr);

private slots:
    void rebuildAll();
    void onCustomContextMenuRequested(const QPoint& pos);
    void beginRename(const QString& nodeId);

private:
    doclife::model::ProjectTreeModel*m_model = nullptr;
    fluent::collections::TreeView*   m_tree  = nullptr;
};

} // namespace doclife::ui