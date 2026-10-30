#pragma once

#include <QWidget>

class QLineEdit;
class QListView;
class QStandardItemModel;
class QSortFilterProxyModel;

namespace doclife::ui {

    class CatalogSearchView : public QWidget
    {
        Q_OBJECT
    public:
        explicit CatalogSearchView(QWidget* parent = nullptr);

    private:
        void rebuild();
        void onItemActivated(const QModelIndex& index);

        QLineEdit* m_edit = nullptr;
        QListView* m_view = nullptr;
        QStandardItemModel* m_model = nullptr;
        QSortFilterProxyModel* m_proxy = nullptr;
    };

} // namespace doclife::ui