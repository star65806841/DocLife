#pragma once

#include <QList>
#include <components/dialogs_flyouts/Dialog.h>
#include "business/projectcontext.h"

namespace fluent::textfields { class AutoSuggestBox; }
namespace fluent::basicinput { class Button; }
namespace fluent::collections { class TreeView; } // ★ 引入 Fluent TreeView
class QStandardItemModel;
class QListView;
class QTabWidget;

namespace doclife::ui {

    class AddSheetDialog : public fluent::dialogs_flyouts::Dialog
    {
        Q_OBJECT
    public:
        explicit AddSheetDialog(QWidget* parent = nullptr);
        ~AddSheetDialog();

        QList<doclife::core::SheetNodeData> selectedSheets() const;

    private:
        void updateOkButtonState();

        // 搜索相关
        void doFilter(const QString& text);
        void onSuggestionChosen(const QVariant& item);
        void showProcessList(const QString& processName);
        void showSingleSheet(const QString& title, const QString& code);

        // ---- 页签容器 ----
        QTabWidget* m_tabWidget = nullptr;

        // ---- 页 1：搜索与勾选 ----
        fluent::textfields::AutoSuggestBox* m_searchBox = nullptr;
        QListView* m_processListView = nullptr;
        QStandardItemModel* m_processListModel = nullptr;

        // ---- 页 2：自由选表（Fluent 树形） ----
        fluent::collections::TreeView* m_treeView = nullptr; // ★ 替换为 Fluent 树

        // ---- 底部按钮 ----
        fluent::basicinput::Button* m_okBtn = nullptr;

        QStringList m_allSuggestions;
    };

} // namespace doclife::ui