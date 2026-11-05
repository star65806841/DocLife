#include "aboutdialog.h"

#include <QCoreApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QTextBrowser>
#include <QVBoxLayout>

#include <components/basicinput/Button.h>

namespace doclife::ui {

    AboutDialog::AboutDialog(QWidget* parent)
        : fluent::dialogs_flyouts::Dialog(parent)
    {
        this->setMinimumSize(560, 460);
        this->setSmokeEnabled(true);
        this->setAnimationEnabled(true);
        this->setModal(true);
        this->setDim(true);

        auto* layout = new QVBoxLayout(this);
        layout->setContentsMargins(24, 20, 24, 20);
        layout->setSpacing(14);

        auto* title = new QLabel(QStringLiteral("关于"), this);
        title->setStyleSheet(QStringLiteral("font-size: 16px; font-weight: bold; color: #333;"));
        layout->addWidget(title);

        auto* browser = new QTextBrowser(this);
        browser->setOpenExternalLinks(true);
        browser->setStyleSheet(QStringLiteral(
            "QTextBrowser { border: 1px solid #E0E0E0; border-radius: 4px; background: #FFFFFF; padding: 8px; }"));
        browser->setHtml(buildContent());
        layout->addWidget(browser, 1);

        auto* commandRow = new QHBoxLayout;
        commandRow->addStretch(1);
        auto* closeButton = new fluent::basicinput::Button(QStringLiteral("关 闭"), this);
        closeButton->setMinimumWidth(100);
        closeButton->setFluentStyle(fluent::basicinput::Button::Accent);
        commandRow->addWidget(closeButton);
        layout->addLayout(commandRow);

        connect(closeButton, &fluent::basicinput::Button::clicked, this, &AboutDialog::close);
        connect(this, &QDialog::finished, this, &QObject::deleteLater);
    }

    QString AboutDialog::buildContent()
    {
        QString version = QCoreApplication::applicationVersion();
        if (version.isEmpty()) version = QStringLiteral("0.1.0");

        const QString html = QStringLiteral(R"(
<h3 style="margin-top:0;">DocLife 公路资料管理软件</h3>
<p style="color:#666;">版本 %1 &nbsp;|&nbsp; 构建 %2<br/>
基于 Qt %3 构建</p>

<p>本软件用于公路工程质检资料的分类、填报与打印管理。</p>

<h4>第三方组件声明</h4>
<table cellspacing="0" cellpadding="4" style="border-collapse:collapse;">
<tr><th align="left">组件</th><th align="left">用途</th><th align="left">许可证</th></tr>
<tr><td>Qt 6</td><td>应用框架</td><td>LGPL-3.0 / GPL-3.0 / 商业许可</td></tr>
<tr><td>Fluent-Qt</td><td>界面组件库</td><td>MIT</td></tr>
<tr><td>QXlsx</td><td>Excel 读写</td><td>MIT</td></tr>
<tr><td>QuaZip</td><td>压缩包读写</td><td>LGPL-2.1</td></tr>
<tr><td>zlib</td><td>压缩算法</td><td>zlib License</td></tr>
<tr><td>bzip2</td><td>压缩算法</td><td>BSD 风格许可</td></tr>
</table>

<p style="color:#666; font-size:12px;">
以上组件版权归各自作者所有，使用须遵守对应许可证条款。<br/>
Qt 以动态链接方式使用，相关许可证文本可在 Qt 安装目录中找到。</p>

<h4>说明</h4>
<p style="color:#666; font-size:12px;">
本软件中的工程表格模板来源于公开的工程质检用表格式，仅用于资料整理；<br/>
填报内容由使用者自行负责。</p>
)").arg(version, QString::fromLatin1(__DATE__), QString::fromLatin1(qVersion()));

        return html;
    }

} // namespace doclife::ui
