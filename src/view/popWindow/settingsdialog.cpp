#include "settingsdialog.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QSettings>
#include <QVBoxLayout>

#include <components/basicinput/Button.h>
#include <components/basicinput/ColorPicker.h>
#include <components/basicinput/ToggleButton.h>
#include <components/navigation/NavigationView.h>
#include <components/navigation/StackContentHost.h>
#include <components/textfields/Label.h>

#include <uisignalmanager.h>
#include <logging.h>
#include <popWindow/infoItem/projectitem.h>
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
        // 尺寸与观感对齐「工程信息」弹窗
        this->setMinimumSize(650, 500);
        this->setSmokeEnabled(true);
        this->setAnimationEnabled(true);

        auto* layout = new QVBoxLayout(this);
        layout->setContentsMargins(55, 25, 45, 25);
        layout->setSpacing(18);

        auto* titleLabel = new fluent::textfields::Label(QStringLiteral("设置:"));
        titleLabel->setFluentTypography(Typography::FontRole::Subtitle);

        auto* navView = new fluent::navigation::NavigationView();
        navView->setMinimumWidth(220);
        navView->setMaximumWidth(620);
        navView->setFixedHeight(320);
        navView->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        navView->setAnimationEnabled(true);
        navView->setDisplayMode(fluent::navigation::NavigationView::DisplayMode::Left);
        navView->setExpandedPaneWidth(180);

        // ---- 内容宿主：编辑区域设置 / 诊断日志 ----
        fluent::navigation::StackContentHost* host = navView->contentHost();
        host->insertPage(0, buildEditorPage(), fluent::WidgetOwnership::Reparented);
        host->insertPage(1, buildDiagnosticsPage(), fluent::WidgetOwnership::Reparented);
        host->setTransitionEffect(fluent::navigation::StackContentHost::TransitionEffect::SlideFromBottom);

        // ---- 导航项列表作为 main chrome 接入 ----
        QVector<MainSection::Entry> entries =
        {
            {Typography::Icons::glyph(QStringLiteral("ic_fluent_paint_bucket_24_regular")),   QStringLiteral("编辑区域设置"), true},
            {Typography::Icons::glyph(QStringLiteral("ic_fluent_arrow_download_24_regular")), QStringLiteral("诊断日志"),       false},
        };

        auto* mainSection = new MainSection(entries, navView);
        navView->setMainChromeWidget(mainSection, fluent::WidgetOwnership::Borrowed);

        host->setCurrentIndex(0, 0, false);

        mainSection->onActivated = [host](int pageIndex) {
            const int direction = pageIndex >= host->currentIndex() ? 1 : -1;
            host->setCurrentIndex(pageIndex, direction, true);
        };

        // 底部命令按钮（与「工程信息」同样的顺序与样式）
        auto* okButton = new fluent::basicinput::Button(QStringLiteral("确 认"));
        okButton->setMinimumWidth(120);
        okButton->setFluentStyle(fluent::basicinput::Button::Accent);

        auto* cancelButton = new fluent::basicinput::Button(QStringLiteral("取 消"));
        cancelButton->setMinimumWidth(120);
        cancelButton->setFluentStyle(fluent::basicinput::Button::Standard);

        auto* commandRow = new QHBoxLayout;
        commandRow->setContentsMargins(0, 0, 0, 0);
        commandRow->setSpacing(8);
        commandRow->addStretch(1);
        commandRow->addWidget(okButton);
        commandRow->addWidget(cancelButton);

        layout->addWidget(titleLabel);
        layout->addWidget(navView);
        layout->addStretch(1);
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

        layout->addStretch(1);
        return page;
    }

    QWidget* SettingsDialog::buildDiagnosticsPage()
    {
        auto* page = new QWidget(this);
        auto* layout = new QVBoxLayout(page);
        layout->setContentsMargins(16, 8, 8, 8);
        layout->setSpacing(16);

        auto* heading = new QLabel(QStringLiteral("诊断日志"), page);
        heading->setStyleSheet(QStringLiteral("font-size: 14px; font-weight: 600; color: #333;"));
        layout->addWidget(heading);

        // 把日志打包成一个文件，用户发回来即可远程定位问题
        auto* diagRow = new QHBoxLayout;
        auto* diagLabel = new QLabel(QStringLiteral("导出日志"), page);
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
                AppToast::showWarning(this, QStringLiteral("导出失败：桌面目录不可写"));
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
