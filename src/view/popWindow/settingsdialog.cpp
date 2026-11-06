#include "settingsdialog.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QSettings>
#include <QStackedWidget>
#include <QVBoxLayout>

#include <components/basicinput/Button.h>
#include <components/basicinput/ColorPicker.h>
#include <components/basicinput/ToggleButton.h>

#include <uisignalmanager.h>
#include <logging.h>
#include <popWindow/toast/apptoast.h>

namespace doclife::ui {

    namespace {
        const char* kSelectionEnabledKey = "editor/selectionEnabled";
        const char* kSelectionColorKey = "editor/selectionColor";
    }

    EditorSettings loadEditorSettings()
    {
        QSettings settings;
        EditorSettings value;
        value.selectionEnabled = settings.value(QString::fromLatin1(kSelectionEnabledKey), true).toBool();

        const QColor color = settings.value(QString::fromLatin1(kSelectionColorKey)).value<QColor>();
        if (color.isValid()) value.selectionColor = color;
        return value;
    }

    void saveEditorSettings(const EditorSettings& settings)
    {
        QSettings store;
        store.setValue(QString::fromLatin1(kSelectionEnabledKey), settings.selectionEnabled);
        store.setValue(QString::fromLatin1(kSelectionColorKey), settings.selectionColor);
    }

    QString editorTableStyleSheet(const EditorSettings& settings)
    {
        // 选中高亮不再走样式表：样式表一旦给 item:selected 定 background-color，
        // Qt 就不再绘制单元格自身的填充色，用户刚填完色的格子（必然处于选中态）会"看不见颜色"。
        // 这里显式置透明，半透明高亮由 CellBorderDelegate 自己叠加绘制。
        const QString selection = QStringLiteral("background-color: transparent;");

        return QStringLiteral(R"(
            QTableWidget {
                background-color: #FFFFFF;
                border: none;
            }
            QTableWidget::item {
                padding: 0px;
                margin: 0px;
                border: none;
            }
            QTableWidget::item:selected {
                %1
            }
            QHeaderView::section {
                background-color: #F8F8F8;
                border: 1px solid #D0D0D0;
                font-weight: bold;
                padding: 4px;
            }
            QHeaderView::section:hover {
                background-color: #E5F1FB;
            }
        )").arg(selection);
    }

    SettingsDialog::SettingsDialog(QWidget* parent)
        : fluent::dialogs_flyouts::Dialog(parent)
    {
        this->setMinimumSize(620, 460);
        this->setSmokeEnabled(true);
        this->setAnimationEnabled(true);
        this->setModal(true);
        this->setDim(true);

        auto* layout = new QVBoxLayout(this);
        layout->setContentsMargins(24, 20, 24, 20);
        layout->setSpacing(14);

        auto* title = new QLabel(QStringLiteral("设置"), this);
        title->setStyleSheet(QStringLiteral("font-size: 16px; font-weight: bold; color: #333;"));
        layout->addWidget(title);

        auto* body = new QHBoxLayout;
        body->setSpacing(12);

        m_nav = new QListWidget(this);
        m_nav->setFixedWidth(160);
        m_nav->setStyleSheet(QStringLiteral(R"(
            QListWidget { border: 1px solid #E0E0E0; border-radius: 4px; background: #FAFAFA; }
            QListWidget::item { padding: 10px 12px; }
            QListWidget::item:selected { background: #E5F1FB; color: #0078D4; }
        )"));
        m_nav->addItem(QStringLiteral("编辑区域设置"));
        m_nav->setCurrentRow(0);
        body->addWidget(m_nav);

        m_pages = new QStackedWidget(this);
        m_pages->addWidget(buildEditorPage());
        body->addWidget(m_pages, 1);

        layout->addLayout(body, 1);

        auto* commandRow = new QHBoxLayout;
        commandRow->addStretch(1);

        auto* cancelButton = new fluent::basicinput::Button(QStringLiteral("取 消"), this);
        cancelButton->setMinimumWidth(100);
        cancelButton->setFluentStyle(fluent::basicinput::Button::Standard);

        auto* okButton = new fluent::basicinput::Button(QStringLiteral("确 认"), this);
        okButton->setMinimumWidth(100);
        okButton->setFluentStyle(fluent::basicinput::Button::Accent);

        commandRow->addWidget(okButton);
        commandRow->addWidget(cancelButton);
        layout->addLayout(commandRow);

        connect(cancelButton, &fluent::basicinput::Button::clicked, this, &QDialog::close);
        connect(okButton, &fluent::basicinput::Button::clicked, this, &SettingsDialog::applySettings);

        EditorSettings settings = loadEditorSettings();
        m_selectionToggle->setChecked(settings.selectionEnabled);
        m_colorPicker->setColor(settings.selectionColor);
        updateColorEnabled();

        connect(m_selectionToggle, &fluent::basicinput::ToggleButton::toggled,
            this, [this](bool) { updateColorEnabled(); });
        connect(this, &QDialog::finished, this, &QObject::deleteLater);
    }

    QWidget* SettingsDialog::buildEditorPage()
    {
        auto* page = new QWidget(this);
        auto* layout = new QVBoxLayout(page);
        layout->setContentsMargins(16, 8, 8, 8);
        layout->setSpacing(16);

        auto* heading = new QLabel(QStringLiteral("编辑区域"), page);
        heading->setStyleSheet(QStringLiteral("font-size: 14px; font-weight: 600; color: #333;"));
        layout->addWidget(heading);

        auto* toggleRow = new QHBoxLayout;
        auto* toggleLabel = new QLabel(QStringLiteral("选中背景"), page);
        m_selectionToggle = new fluent::basicinput::ToggleButton(QStringLiteral("启用"), page);
        toggleRow->addWidget(toggleLabel);
        toggleRow->addStretch(1);
        toggleRow->addWidget(m_selectionToggle);
        layout->addLayout(toggleRow);

        auto* colorRow = new QHBoxLayout;
        m_colorLabel = new QLabel(QStringLiteral("背景色"), page);
        m_colorPicker = new fluent::basicinput::ColorPicker(page);
        colorRow->addWidget(m_colorLabel);
        colorRow->addStretch(1);
        colorRow->addWidget(m_colorPicker);
        layout->addLayout(colorRow);

        auto* hint = new QLabel(
            QStringLiteral("关闭后单元格选中时不再显示背景色；\n自定义颜色时，选中文字默认显示为白色。"), page);
        hint->setStyleSheet(QStringLiteral("color: #777; font-size: 12px;"));
        hint->setWordWrap(true);
        layout->addWidget(hint);

        // ---- 诊断：把日志打包成一个文件，用户发回来即可定位问题 ----
        auto* diagRow = new QHBoxLayout;
        auto* diagLabel = new QLabel(QStringLiteral("诊断日志"), page);
        auto* diagButton = new fluent::basicinput::Button(QStringLiteral("导出到桌面"), page);
        diagRow->addWidget(diagLabel);
        diagRow->addStretch(1);
        diagRow->addWidget(diagButton);
        layout->addLayout(diagRow);

        auto* diagHint = new QLabel(
            QStringLiteral("导出后是一个文本文件，包含程序版本、系统信息和最近的运行日志，"
                           "可以直接发给技术支持。\n日志目录：%1").arg(doclife::logging::logDirectory()), page);
        diagHint->setStyleSheet(QStringLiteral("color: #777; font-size: 12px;"));
        diagHint->setWordWrap(true);
        layout->addWidget(diagHint);

        connect(diagButton, &fluent::basicinput::Button::clicked, this, [this]() {
            const QString path = doclife::logging::exportDiagnostics();
            if (path.isEmpty()) {
                AppToast::showSuccess(this, QStringLiteral("导出失败：桌面目录不可写"));
                return;
            }
            AppToast::showSuccess(this, QStringLiteral("已导出：%1").arg(path));
            });

        layout->addStretch(1);
        return page;
    }

    void SettingsDialog::updateColorEnabled()
    {
        const bool enabled = m_selectionToggle->isChecked();
        m_colorLabel->setEnabled(enabled);
        m_colorPicker->setEnabled(enabled);
    }

    void SettingsDialog::applySettings()
    {
        EditorSettings settings;
        settings.selectionEnabled = m_selectionToggle->isChecked();
        settings.selectionColor = m_colorPicker->color();
        saveEditorSettings(settings);

        emit UISignalManager::instance()->editorSettingsChanged();
        accept();
    }

} // namespace doclife::ui
