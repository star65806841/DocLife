#include "backstagepages.h"

#include <QDateTime>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSettings>
#include <QToolButton>
#include <QVBoxLayout>

namespace doclife::ui {

    namespace backstage {
        namespace {
            const char* kRecentKey = "backstage/recentFiles";
            constexpr int kRecentLimit = 10;
        }

        QStringList recentFiles()
        {
            return QSettings().value(QString::fromLatin1(kRecentKey)).toStringList();
        }

        void addRecentFile(const QString& path)
        {
            if (path.isEmpty()) return;
            const QString clean = QFileInfo(path).absoluteFilePath();

            QStringList list = recentFiles();
            list.removeAll(clean);
            list.prepend(clean);
            while (list.size() > kRecentLimit) list.removeLast();

            QSettings().setValue(QString::fromLatin1(kRecentKey), list);
        }

        void clearRecentFiles()
        {
            QSettings().remove(QString::fromLatin1(kRecentKey));
        }
    } // namespace backstage

    namespace {
        using doclife::ui::backstage::recentFiles;

        QLabel* makeTitle(const QString& text, QWidget* parent)
        {
            auto* label = new QLabel(text, parent);
            label->setStyleSheet(QStringLiteral("font-size: 28px; font-weight: 600; color: #202020;"));
            return label;
        }

        QLabel* makeSectionTitle(const QString& text, QWidget* parent)
        {
            auto* label = new QLabel(text, parent);
            label->setStyleSheet(QStringLiteral("font-size: 15px; font-weight: 600; color: #202020;"));
            return label;
        }

        QLabel* makeHint(const QString& text, QWidget* parent)
        {
            auto* label = new QLabel(text, parent);
            label->setStyleSheet(QStringLiteral("color: #606060; font-size: 13px;"));
            label->setWordWrap(true);
            return label;
        }

        // 大卡片：标题 + 说明，整块可点
        QPushButton* makeCard(const QString& title, const QString& subtitle, QWidget* parent)
        {
            auto* card = new QPushButton(title + QLatin1Char('\n') + subtitle, parent);
            card->setCursor(Qt::PointingHandCursor);
            card->setMinimumSize(220, 120);
            card->setStyleSheet(QStringLiteral(
                "QPushButton {"
                "  background: #FAFAFA; border: 1px solid #E0E0E0; border-radius: 8px;"
                "  padding: 18px; text-align: left; font-size: 14px; color: #202020;"
                "}"
                "QPushButton:hover { background: #F0F5FF; border-color: #0078D4; }"
                "QPushButton:pressed { background: #E4EEFB; }"));
            return card;
        }

        // 最近文件一行：文件名 + 目录/时间，整行可点
        QPushButton* makeRecentRow(const QFileInfo& info, QWidget* parent)
        {
            auto* row = new QPushButton(parent);
            row->setCursor(Qt::PointingHandCursor);
            row->setMinimumHeight(52);
            row->setStyleSheet(QStringLiteral(
                "QPushButton {"
                "  background: transparent; border: none; border-radius: 6px;"
                "  text-align: left; padding: 8px 12px;"
                "}"
                "QPushButton:hover { background: rgba(0,0,0,0.05); }"));

            auto* layout = new QHBoxLayout(row);
            layout->setContentsMargins(12, 6, 12, 6);
            layout->setSpacing(12);

            auto* text = new QVBoxLayout;
            text->setSpacing(2);
            auto* name = new QLabel(info.fileName(), row);
            name->setStyleSheet(QStringLiteral("font-size: 14px; color: #202020;"));
            auto* detail = new QLabel(QStringLiteral("%1 · %2")
                .arg(info.absolutePath(),
                    info.lastModified().toString(QStringLiteral("yyyy-MM-dd HH:mm"))), row);
            detail->setStyleSheet(QStringLiteral("font-size: 12px; color: #606060;"));
            text->addWidget(name);
            text->addWidget(detail);
            layout->addLayout(text, 1);
            return row;
        }

        void clearLayout(QLayout* layout)
        {
            if (!layout) return;
            while (QLayoutItem* item = layout->takeAt(0)) {
                if (QWidget* w = item->widget()) w->deleteLater();
                delete item;
            }
        }
    } // namespace

    // ---------------------------------------------------------------- 新建

    BackstageNewPage::BackstageNewPage(QWidget* parent)
        : BackstagePageBase(parent)
    {
        auto* root = new QVBoxLayout(this);
        root->setContentsMargins(32, 32, 32, 32);
        root->setSpacing(20);

        root->addWidget(makeTitle(QStringLiteral("新建"), this));

        auto* cardRow = new QHBoxLayout;
        cardRow->setSpacing(16);

        auto* blankCard = makeCard(QStringLiteral("空白工程"),
            QStringLiteral("从空白开始建立自己的树"), this);
        auto* openCard = makeCard(QStringLiteral("打开工程"),
            QStringLiteral("打开已有的 .dlf 工程文件"), this);
        cardRow->addWidget(blankCard);
        cardRow->addWidget(openCard);
        cardRow->addStretch();
        root->addLayout(cardRow);

        root->addSpacing(8);
        root->addWidget(makeSectionTitle(QStringLiteral("最近打开"), this));

        auto* recentHost = new QWidget(this);
        m_recentLayout = new QVBoxLayout(recentHost);
        m_recentLayout->setContentsMargins(0, 0, 0, 0);
        m_recentLayout->setSpacing(4);
        root->addWidget(recentHost);
        root->addStretch(1);

        connect(blankCard, &QPushButton::clicked, this, &BackstageNewPage::requestBlank);
        connect(openCard, &QPushButton::clicked, this, &BackstageNewPage::requestOpen);

        rebuildRecent();
    }

    void BackstageNewPage::onShown()
    {
        rebuildRecent();
    }

    void BackstageNewPage::rebuildRecent()
    {
        clearLayout(m_recentLayout);

        const QStringList files = recentFiles();
        if (files.isEmpty()) {
            m_recentLayout->addWidget(makeHint(QStringLiteral("还没有打开过工程。"), this));
            return;
        }

        for (const QString& path : files) {
            const QFileInfo info(path);
            if (!info.exists()) continue;
            auto* row = makeRecentRow(info, this);
            connect(row, &QPushButton::clicked, this, [this, path]() {
                emit requestOpenPath(path);
                });
            m_recentLayout->addWidget(row);
        }
    }

    // ---------------------------------------------------------------- 打开

    BackstageOpenPage::BackstageOpenPage(QWidget* parent)
        : BackstagePageBase(parent)
    {
        auto* root = new QVBoxLayout(this);
        root->setContentsMargins(32, 32, 32, 32);
        root->setSpacing(16);

        root->addWidget(makeTitle(QStringLiteral("打开"), this));

        auto* browse = new QPushButton(QStringLiteral("浏览…"), this);
        browse->setCursor(Qt::PointingHandCursor);
        browse->setFixedWidth(120);
        browse->setMinimumHeight(34);
        browse->setStyleSheet(QStringLiteral(
            "QPushButton { background: #0078D4; color: white; border: none;"
            "  border-radius: 4px; font-size: 14px; }"
            "QPushButton:hover { background: #0067C0; }"));
        root->addWidget(browse);

        root->addSpacing(8);
        root->addWidget(makeSectionTitle(QStringLiteral("历史记录"), this));

        auto* host = new QWidget(this);
        m_listLayout = new QVBoxLayout(host);
        m_listLayout->setContentsMargins(0, 0, 0, 0);
        m_listLayout->setSpacing(4);
        root->addWidget(host);
        root->addStretch(1);

        connect(browse, &QPushButton::clicked, this, &BackstageOpenPage::requestBrowse);
        rebuildList();
    }

    void BackstageOpenPage::onShown()
    {
        rebuildList();
    }

    void BackstageOpenPage::rebuildList()
    {
        clearLayout(m_listLayout);

        const QStringList files = recentFiles();
        int shown = 0;
        for (const QString& path : files) {
            const QFileInfo info(path);
            if (!info.exists()) continue;
            auto* row = makeRecentRow(info, this);
            connect(row, &QPushButton::clicked, this, [this, path]() {
                emit requestOpenPath(path);
                });
            m_listLayout->addWidget(row);
            ++shown;
        }

        if (shown == 0) {
            m_listLayout->addWidget(makeHint(QStringLiteral("暂无历史记录，点上面的「浏览…」选择工程文件。"), this));
        }
    }

    // ---------------------------------------------------------------- 设置

    BackstageSettingsPage::BackstageSettingsPage(QWidget* parent)
        : BackstagePageBase(parent)
    {
        auto* root = new QVBoxLayout(this);
        root->setContentsMargins(32, 32, 32, 32);
        root->setSpacing(16);

        root->addWidget(makeTitle(QStringLiteral("设置"), this));
        root->addWidget(makeHint(QStringLiteral("这些入口复用现有的弹窗，后续可以把内容直接内嵌到这里。"), this));
        root->addSpacing(8);

        auto makeEntry = [this, root](const QString& title, const QString& hint, auto signal) {
            auto* button = new QPushButton(title, this);
            button->setCursor(Qt::PointingHandCursor);
            button->setMinimumHeight(44);
            button->setStyleSheet(QStringLiteral(
                "QPushButton { background: #FAFAFA; border: 1px solid #E0E0E0; border-radius: 6px;"
                "  text-align: left; padding: 0 14px; font-size: 14px; color: #202020; }"
                "QPushButton:hover { background: #F0F5FF; border-color: #0078D4; }"));
            root->addWidget(button);
            root->addWidget(makeHint(hint, this));
            connect(button, &QPushButton::clicked, this, signal);
            };

        makeEntry(QStringLiteral("界面与编辑偏好"),
            QStringLiteral("选中背景、日志导出等"), &BackstageSettingsPage::requestSettings);
        makeEntry(QStringLiteral("工程信息"),
            QStringLiteral("工程名称、编号、各参建单位"), &BackstageSettingsPage::requestProjectInfo);
        makeEntry(QStringLiteral("关于 DocLife"),
            QStringLiteral("版本与第三方组件许可"), &BackstageSettingsPage::requestAbout);

        root->addStretch(1);
    }

    // ---------------------------------------------------------------- 占位

    BackstagePlaceholderPage::BackstagePlaceholderPage(const QString& title,
                                                       const QString& hint,
                                                       QWidget* parent)
        : BackstagePageBase(parent)
    {
        auto* root = new QVBoxLayout(this);
        root->setContentsMargins(32, 32, 32, 32);
        root->setSpacing(12);
        root->addWidget(makeTitle(title, this));
        root->addWidget(makeHint(hint, this));
        root->addStretch(1);
    }

} // namespace doclife::ui
