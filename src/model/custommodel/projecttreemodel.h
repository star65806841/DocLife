#ifndef PROJECTTREEMODEL_H
#define PROJECTTREEMODEL_H

#include <QStandardItemModel>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonDocument>

namespace doclife::ui {

enum {
    RoleNodeType = Qt::UserRole + 1,
    RoleNodeId   = Qt::UserRole + 2,
    RoleSubtitle = Qt::UserRole + 3
};

class ProjectTreeModel : public QStandardItemModel
{
    Q_OBJECT
public:
    explicit ProjectTreeModel(QObject* parent = nullptr);

    // 拖放相关
    Qt::ItemFlags flags(const QModelIndex &index) const override;
    bool dropMimeData(const QMimeData *data, Qt::DropAction action,
                      int row, int column, const QModelIndex &parent) override;

    // 序列化/反序列化
    QJsonArray serializeTree() const;
    void deserializeTree(const QJsonArray& arr);

private:
    QJsonObject serializeItem(QStandardItem* item) const;
    QStandardItem* deserializeItem(const QJsonObject& obj);
};

} // namespace doclife::ui

#endif // PROJECTTREEMODEL_H
