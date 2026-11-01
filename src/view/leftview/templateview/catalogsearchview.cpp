#include "catalogsearchview.h"
#include "templatecatalog.h"
#include "uisignalmanager.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QListView>
#include <QStackedWidget>
#include <QLabel>
#include <QMessageBox>
#include <QDebug>
#include <QEvent>
#include <QMouseEvent>
#include <FluentQt/TextFields.h>

using namespace fluent::textfields;

namespace doclife::ui {

    // 内部用来保存全量数据，方便过滤
    static QStringList s_allSuggestions;

    CatalogSearchView::CatalogSearchView(QWidget* parent)
        : QWidget(parent)
    {
        auto* layout = new QVBoxLayout(this);
        layout->setContentsMargins(12, 12, 12, 12);
        layout->setSpacing(12);

        TemplateCatalog::instance()->load();

        // 1. 搜索框
        m_searchBox = new AutoSuggestBox(this);
        m_searchBox->setPlaceholderText("搜索表格名称，或输入工序（如：明洞浇筑）...");

        // 安装事件过滤器，用于监听鼠标点击
        m_searchBox->installEventFilter(this);

        // 构建全量联想列表
        s_allSuggestions.clear();
        for (const auto& f : TemplateCatalog::instance()->leafFiles()) {
            s_allSuggestions << QStringLiteral("[表格] %1 | %2").arg(f.sheetTitle, f.sheetName);
        }
        QStringList processes = TemplateCatalog::instance()->getAllProcessNames();
        for (const QString& proc : processes) {
            s_allSuggestions << QStringLiteral("[工序] %1").arg(proc);
        }

        m_searchBox->setSuggestions(s_allSuggestions);
        layout->addWidget(m_searchBox);

        // 动态过滤与排序逻辑
        auto doFilter = [this](const QString& inputText) {
            QString input = inputText.trimmed();
            if (input.isEmpty()) {
                m_searchBox->setSuggestions(s_allSuggestions);
                return;
            }

            QStringList startsWithList;
            QStringList containsList;

            for (const QString& s : s_allSuggestions) {
                QString realText = s.mid(s.indexOf(" ") + 1);
                if (realText.contains(input, Qt::CaseInsensitive)) {
                    if (realText.startsWith(input, Qt::CaseInsensitive)) {
                        startsWithList << s;
                    }
                    else {
                        containsList << s;
                    }
                }
            }

            startsWithList.append(containsList);
            if (startsWithList.size() > 50) startsWithList = startsWithList.mid(0, 50);
            m_searchBox->setSuggestions(startsWithList);
            };

        // 监听用户手动输入
        connect(m_searchBox, &AutoSuggestBox::textChangedWithReason, this,
            [doFilter](const QString& text, AutoSuggestBox::TextChangeReason reason) {
                if (reason != AutoSuggestBox::TextChangeReason::UserInput) return;
                doFilter(text);
            });

        // 2. 堆栈页面
        m_stack = new QStackedWidget(this);
        layout->addWidget(m_stack, 1);

        // --- 页面 0：提示页 ---
        auto* page0 = new QWidget();
        auto* hint = new QLabel("请输入关键字。\n\n搜索结果不仅支持预览单张表格，\n还支持直接搜索工序，一键推荐整套表格。", page0);
        hint->setStyleSheet("color: #888888; font-size: 14px; line-height: 24px;");
        hint->setAlignment(Qt::AlignCenter);
        auto* p0Layout = new QVBoxLayout(page0);
        p0Layout->addWidget(hint);
        m_stack->addWidget(page0);

        // --- 页面 1：单表信息页 ---
        auto* page1 = new QWidget();
        auto* p1Layout = new QVBoxLayout(page1);
        p1Layout->setContentsMargins(0, 20, 0, 0);

        m_singleTitle = new QLabel("表格名称");
        m_singleTitle->setStyleSheet("font-size: 18px; font-weight: bold; color: #0078D4;");
        m_singleTitle->setWordWrap(true);
        m_singleCode = new QLabel("表格编号");
        m_singleCode->setStyleSheet("font-size: 14px; color: #666;");

        p1Layout->addWidget(m_singleTitle);
        p1Layout->addSpacing(8);
        p1Layout->addWidget(m_singleCode);
        p1Layout->addSpacing(20);
        auto* autoHint = new QLabel("✓ 已在右侧打开预览");
        autoHint->setStyleSheet("color: #28A745; font-weight: bold;");
        p1Layout->addWidget(autoHint);

        p1Layout->addStretch();
        m_stack->addWidget(page1);

        // --- 页面 2：工序成套推荐页 ---
        auto* page2 = new QWidget();
        auto* p2Layout = new QVBoxLayout(page2);
        p2Layout->setContentsMargins(0, 20, 0, 0);

        m_processTitle = new QLabel("工序推荐清单");
        m_processTitle->setStyleSheet("font-size: 18px; font-weight: bold; color: #333;");

        m_checkList = new QListView();
        m_checkList->setStyleSheet("QListView { border: 1px solid #E0E0E0; border-radius: 4px; padding: 4px; }");
        m_checkModel = new QStandardItemModel(this);
        m_checkList->setModel(m_checkModel);

        // ★ 防护 1：从视图层面彻底禁止用户双击或按键触发编辑
        m_checkList->setEditTriggers(QAbstractItemView::NoEditTriggers);

        connect(m_checkList, &QListView::clicked, this, &CatalogSearchView::onCheckListItemClicked);

        p2Layout->addWidget(m_processTitle);
        p2Layout->addSpacing(8);
        p2Layout->addWidget(m_checkList, 1);
        m_stack->addWidget(page2);

        // 3. 下拉选中信号
        connect(m_searchBox, &AutoSuggestBox::suggestionChosen, this, [this](const QVariant& item) {
            const QString text = item.toString();
            if (text.startsWith("[表格] ")) {
                QStringList parts = text.mid(5).split(" | ");
                if (parts.size() == 2) {
                    showSingleSheet(parts[0], parts[1]);
                }
            }
            else if (text.startsWith("[工序] ")) {
                showProcessList(text.mid(5));
            }
            });
    }

    // 重写事件过滤器：点击输入框，剥离前缀并触发推荐
    bool CatalogSearchView::eventFilter(QObject* watched, QEvent* event)
    {
        if (watched == m_searchBox && event->type() == QEvent::MouseButtonPress) {
            QString currentText = m_searchBox->text();
            if (currentText.startsWith("[表格] ")) {
                QString realText = currentText.mid(5).split(" | ")[0];
                m_searchBox->setText(realText);

                QStringList startsWithList;
                QStringList containsList;
                for (const QString& s : s_allSuggestions) {
                    QString rt = s.mid(s.indexOf(" ") + 1);
                    if (rt.contains(realText, Qt::CaseInsensitive)) {
                        if (rt.startsWith(realText, Qt::CaseInsensitive)) startsWithList << s;
                        else containsList << s;
                    }
                }
                startsWithList.append(containsList);
                m_searchBox->setSuggestions(startsWithList);
                m_stack->setCurrentIndex(0);
            }
            else if (currentText.startsWith("[工序] ")) {
                QString realText = currentText.mid(5);
                m_searchBox->setText(realText);
                m_stack->setCurrentIndex(0);
            }
        }
        return QWidget::eventFilter(watched, event);
    }

    void CatalogSearchView::showSingleSheet(const QString& title, const QString& code)
    {
        m_singleTitle->setText(title);
        m_singleCode->setText(QStringLiteral("表格编号：%1").arg(code));
        m_stack->setCurrentIndex(1);

        const TemplateFileInfo* info = TemplateCatalog::instance()->findSheetByNameAndCode(title, code);
        if (info) {
            UISignalManager::instance()->templateSheetActivated(info->dbPath, info->sheetId, info->displayName());
        }
    }

    void CatalogSearchView::showProcessList(const QString& processName)
    {
        m_processTitle->setText(QStringLiteral("工序：%1 · 推荐表格清单").arg(processName));
        m_checkModel->clear();

        const auto rules = TemplateCatalog::instance()->getRulesByProcess(processName);
        for (const auto& r : rules) {
            auto* item = new QStandardItem(QStringLiteral("%1 (%2)").arg(r.sheetTitle, r.sheetCode));
            item->setCheckable(true);
            item->setCheckState(Qt::Checked);
            item->setData(r.sheetId, Qt::UserRole);

            // ★ 防护 2：从数据模型层面禁止该单元格被编辑
            item->setEditable(false);

            if (r.sheetId < 0) {
                item->setEnabled(false);
                item->setText(item->text() + " [系统暂缺此模板]");
                item->setForeground(QBrush(QColor("#999999")));
            }
            m_checkModel->appendRow(item);
        }
        m_stack->setCurrentIndex(2);
    }

    void CatalogSearchView::onCheckListItemClicked(const QModelIndex& index)
    {
        if (!index.isValid()) return;

        int sheetId = index.data(Qt::UserRole).toInt();
        if (sheetId < 0) return;

        for (const auto& f : TemplateCatalog::instance()->leafFiles()) {
            if (f.sheetId == sheetId) {
                UISignalManager::instance()->templateSheetActivated(f.dbPath, f.sheetId, f.displayName());
                break;
            }
        }
    }

} // namespace doclife::ui