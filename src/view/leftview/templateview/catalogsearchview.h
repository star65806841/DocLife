#pragma once

#include <QWidget>
#include <QStandardItemModel>

class QListView;
class QStackedWidget;
class QLabel;
class QEvent;

namespace fluent::textfields { class AutoSuggestBox; }

namespace doclife::ui {

    class CatalogSearchView : public QWidget
    {
        Q_OBJECT
    public:
        explicit CatalogSearchView(QWidget* parent = nullptr);

    protected:
        // 重写事件过滤器，用于清空点击文字
        bool eventFilter(QObject* watched, QEvent* event) override;

    private:
        void onSuggestionChosen(const QVariant& item);
        void showSingleSheet(const QString& title, const QString& code);
        void showProcessList(const QString& processName);
        void onCheckListItemClicked(const QModelIndex& index);

        fluent::textfields::AutoSuggestBox* m_searchBox = nullptr;
        QStackedWidget* m_stack = nullptr;

        // 单表页 (无需按钮，仅做提示)
        QLabel* m_singleTitle = nullptr;
        QLabel* m_singleCode = nullptr;

        // 工序页
        QLabel* m_processTitle = nullptr;
        QListView* m_checkList = nullptr;
        QStandardItemModel* m_checkModel = nullptr;
    };

} // namespace doclife::ui