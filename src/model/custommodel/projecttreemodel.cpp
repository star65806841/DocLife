#include "ProjectTreeModel.h"
#include <QIODevice>
#include <QMimeData>

namespace doclife::ui {

ProjectTreeModel::ProjectTreeModel(QObject* parent)
    : QStandardItemModel(parent)
{
}

// 除根节点外都能拖动，根节点不能拖动/放置
Qt::ItemFlags ProjectTreeModel::flags(const QModelIndex &index) const
{
    if (!index.isValid()) {
        // 顶层（不可见根）不接收放置
        Qt::ItemFlags f = QStandardItemModel::flags(index);
        f &= ~Qt::ItemIsDropEnabled;
        return f;
    }
    if (!index.isValid())
        return QStandardItemModel::flags(index);

    QString type = index.data(RoleNodeType).toString();
    if (type == "project") {
        // 根节点不可拖动/放置
        return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
    }
    // 其他节点可拖放
    return QStandardItemModel::flags(index) | Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled;
}

// 拖放逻辑
bool ProjectTreeModel::dropMimeData(const QMimeData *data, Qt::DropAction action,
                                    int row, int column, const QModelIndex &parent)
{
    // 获取拖动的节点类型
    QByteArray encoded = data->data("application/x-qstandarditemmodeldatalist");
    if (encoded.isEmpty())
        return false;

    QDataStream stream(&encoded, QIODevice::ReadOnly);
    int r, c;
    QMap<int, QVariant> roleData;
    stream >> r >> c >> roleData;
    QString draggedType = roleData.value(RoleNodeType).toString();

    QString parentType = parent.data(RoleNodeType).toString();

    // 根节点不能作为目标
    if (parentType == "project" && draggedType != "unit")
        return false;

    // sheet 只能放在 item 节点下的末尾
    if (draggedType == "sheet") {
        if (parentType != "item")
            return false;
        row = -1; // 末尾
    }

    // sheet 不能作为父节点
    if (parentType == "sheet")
        return false;

    return QStandardItemModel::dropMimeData(data, action, row, column, parent);
}

// 递归序列化
QJsonArray ProjectTreeModel::serializeTree() const
{
    QJsonArray arr;
    for (int i = 0; i < rowCount(); ++i) {
        QStandardItem* item = this->item(i);
        arr.append(serializeItem(item));
    }
    return arr;
}

QJsonObject ProjectTreeModel::serializeItem(QStandardItem* item) const
{
    QJsonObject obj;
    obj["id"] = item->data(RoleNodeId).toString();
    obj["type"] = item->data(RoleNodeType).toString();
    obj["subtitle"] = item->data(RoleSubtitle).toString();
    obj["text"] = item->text();

    QJsonArray children;
    for (int i = 0; i < item->rowCount(); ++i) {
        children.append(serializeItem(item->child(i)));
    }
    obj["children"] = children;
    return obj;
}

// 递归反序列化
void ProjectTreeModel::deserializeTree(const QJsonArray& arr)
{
    clear();
    for (const QJsonValue& v : arr) {
        QStandardItem* item = deserializeItem(v.toObject());
        if (item)
            appendRow(item);
    }
}

QStandardItem* ProjectTreeModel::deserializeItem(const QJsonObject& obj)
{
    auto* item = new QStandardItem(obj["text"].toString());
    item->setData(obj["id"].toString(), RoleNodeId);
    item->setData(obj["type"].toString(), RoleNodeType);
    item->setData(obj["subtitle"].toString(), RoleSubtitle);
    item->setEditable(false);

    QJsonArray children = obj["children"].toArray();
    for (const QJsonValue& v : children) {
        QStandardItem* child = deserializeItem(v.toObject());
        if (child)
            item->appendRow(child);
    }
    return item;
}

} // namespace doclife::ui
