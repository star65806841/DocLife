#pragma once

#include <QWidget>
#include <QStandardItemModel>

class QStackedWidget;
class QLabel;
class QEvent;

namespace fluent::textfields { class AutoSuggestBox; }
namespace fluent::collections { class ListView; }

namespace doclife::ui {

    class CatalogSearchView : public QWidget
    {
        Q_OBJECT
    public:
        explicit CatalogSearchView(QWidget* parent = nullptr);

    protected:
        bool eventFilter(QObject* watched, QEvent* event) override;

    private:
        void onSuggestionChosen(const QVariant& item);
        void showSingleSheet(const QString& title, const QString& code);
        void showProcessList(const QString& processName);
        void onCheckListItemClicked(const QModelIndex& index);

        fluent::textfields::AutoSuggestBox* m_searchBox = nullptr;
        QStackedWidget* m_stack = nullptr;

        // 单表页
        QLabel* m_singleTitle = nullptr;
        QLabel* m_singleCode = nullptr;

        // 工序页
        QLabel* m_processTitle = nullptr;
        // ★ 替换为 Fluent 的 ListView
        fluent::collections::ListView* m_checkList = nullptr;
        QStandardItemModel* m_checkModel = nullptr;
    };

} // namespace doclife::ui