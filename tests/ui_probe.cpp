// Visual development harness for the Material widget kit.  Not part of the
// shipped app; run with --screenshot <path> to capture the window.

#include "term/TerminalWidget.h"
#include "ui/Icons.h"
#include "ui/MaterialWidgets.h"
#include "ui/Theme.h"

#include <QApplication>
#include <QFile>
#include <QHBoxLayout>
#include <QLabel>
#include <QScrollArea>
#include <QTimer>
#include <QVBoxLayout>

using namespace nyan::ui;

namespace {

QLabel *label(const QString &text, int pointSize = 10, QFont::Weight weight = QFont::Normal)
{
    auto *widget = new QLabel(text);
    widget->setFont(Theme::instance().uiFont(pointSize, weight));
    return widget;
}

QWidget *sessionCard(const QString &name, const QString &host, const QColor &color, bool autoLogin)
{
    auto *card = new MCard();
    card->setHoverable(true);
    card->setFixedHeight(74);
    auto *layout = new QHBoxLayout(card);
    layout->setContentsMargins(14, 10, 12, 10);
    layout->setSpacing(12);

    auto *avatar = new QLabel;
    avatar->setFixedSize(38, 38);
    avatar->setPixmap(Icons::logo(34, 2.0));
    layout->addWidget(avatar);

    auto *textLayout = new QVBoxLayout;
    textLayout->setSpacing(3);
    auto *title = label(name, 10, QFont::DemiBold);
    auto *subtitle = label(host, 8.5);
    subtitle->setProperty("role", QStringLiteral("muted"));
    textLayout->addWidget(title);
    textLayout->addWidget(subtitle);

    auto *tags = new QHBoxLayout;
    tags->setSpacing(5);
    tags->addWidget(new MTag(autoLogin ? QStringLiteral("自动登录") : QStringLiteral("密码"), color));
    tags->addStretch();
    textLayout->addLayout(tags);
    layout->addLayout(textLayout, 1);

    auto *icon = new MIconButton(QStringLiteral("chevron-right"), QStringLiteral("连接"));
    icon->setButtonSize(30);
    layout->addWidget(icon);
    return card;
}

} // namespace

int main(int argc, char **argv)
{
    Q_INIT_RESOURCE(resources);
    QApplication app(argc, argv);
    QApplication::setStyle(QStringLiteral("Fusion"));
    QApplication::setApplicationName(QStringLiteral("Nyan SSH UI Probe"));
    Theme::instance().setMode(nyan::ThemeMode::Dark);

    QWidget window;
    window.setWindowTitle(QStringLiteral("Nyan SSH — UI Probe"));
    window.resize(1240, 800);
    auto *root = new QHBoxLayout(&window);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    auto *sidebar = new QWidget;
    sidebar->setObjectName(QStringLiteral("sidebar"));
    sidebar->setFixedWidth(300);
    auto *sidebarLayout = new QVBoxLayout(sidebar);
    sidebarLayout->setContentsMargins(18, 20, 18, 18);
    sidebarLayout->setSpacing(14);

    auto *brand = new QHBoxLayout;
    auto *logo = new QLabel;
    logo->setPixmap(Icons::logo(42, 2.0));
    logo->setFixedSize(42, 42);
    brand->addWidget(logo);
    auto *brandText = new QVBoxLayout;
    brandText->setSpacing(0);
    auto *brandTitle = label(QStringLiteral("Nyan SSH"), 15, QFont::DemiBold);
    brandTitle->setFont(Theme::instance().displayFont(15, QFont::DemiBold));
    auto *brandSub = label(QStringLiteral("轻盈的终端工作台"), 8);
    brandSub->setProperty("role", QStringLiteral("muted"));
    brandText->addWidget(brandTitle);
    brandText->addWidget(brandSub);
    brand->addLayout(brandText, 1);
    sidebarLayout->addLayout(brand);

    auto *newButton = new MButton(QStringLiteral("新建连接"), MButton::Variant::Filled);
    newButton->setIconName(QStringLiteral("plus"));
    sidebarLayout->addWidget(newButton);

    auto *search = new MTextField(QStringLiteral("搜索会话"), QStringLiteral("search"));
    sidebarLayout->addWidget(search);

    auto *sectionLabel = label(QStringLiteral("会话"), 9, QFont::DemiBold);
    sectionLabel->setProperty("role", QStringLiteral("muted"));
    sidebarLayout->addWidget(sectionLabel);

    auto *sessions = new QVBoxLayout;
    sessions->setSpacing(8);
    sessions->addWidget(sessionCard(QStringLiteral("生产服务器"), QStringLiteral("nyan@10.0.0.12:22"),
                                    QColor(QStringLiteral("#4C8DFF")), true));
    sessions->addWidget(sessionCard(QStringLiteral("本地实验台"), QStringLiteral("nyan@127.0.0.1:2222"),
                                    QColor(QStringLiteral("#2DD4BF")), false));
    sessions->addWidget(sessionCard(QStringLiteral("边缘节点"), QStringLiteral("ops@edge.nyan.dev:22"),
                                    QColor(QStringLiteral("#F472B6")), false));
    sidebarLayout->addLayout(sessions);
    sidebarLayout->addStretch();

    auto *status = new MCard;
    status->setFixedHeight(58);
    auto *statusLayout = new QHBoxLayout(status);
    statusLayout->setContentsMargins(12, 8, 12, 8);
    auto *statusDot = new QLabel;
    statusDot->setFixedSize(8, 8);
    statusDot->setStyleSheet(QStringLiteral("background:#4EC9B0;border-radius:4px;"));
    statusLayout->addWidget(statusDot);
    statusLayout->addWidget(label(QStringLiteral("2 个活动会话"), 9), 1);
    auto *settings = new MIconButton(QStringLiteral("settings"), QStringLiteral("设置"));
    statusLayout->addWidget(settings);
    sidebarLayout->addWidget(status);
    root->addWidget(sidebar);

    auto *mainPanel = new QWidget;
    auto *mainLayout = new QVBoxLayout(mainPanel);
    mainLayout->setContentsMargins(18, 14, 18, 14);
    mainLayout->setSpacing(12);

    auto *topBar = new QHBoxLayout;
    auto *connectionPill = new MCard;
    connectionPill->setFixedHeight(38);
    auto *pillLayout = new QHBoxLayout(connectionPill);
    pillLayout->setContentsMargins(12, 0, 12, 0);
    auto *dot = new QLabel;
    dot->setFixedSize(8, 8);
    dot->setStyleSheet(QStringLiteral("background:#4EC9B0;border-radius:4px;"));
    pillLayout->addWidget(dot);
    pillLayout->addWidget(label(QStringLiteral("已连接 · nyan@127.0.0.1:2222"), 9));
    topBar->addWidget(connectionPill);
    topBar->addStretch();
    for (const QString &icon : {QStringLiteral("folder-open"), QStringLiteral("refresh"),
                                QStringLiteral("panel-right"), QStringLiteral("more")}) {
        auto *button = new MIconButton(icon, icon);
        button->setButtonSize(34);
        topBar->addWidget(button);
    }
    auto *disconnect = new MButton(QStringLiteral("断开"), MButton::Variant::Outlined);
    disconnect->setCompact(true);
    topBar->addWidget(disconnect);
    mainLayout->addLayout(topBar);

    auto *terminalCard = new MCard;
    terminalCard->setCardRadius(12);
    auto *terminalLayout = new QVBoxLayout(terminalCard);
    terminalLayout->setContentsMargins(0, 0, 0, 0);
    auto *terminal = new nyan::term::TerminalWidget;
    terminal->setFontFamily(QStringLiteral("Cascadia Mono"));
    terminal->setFontSize(11);
    terminalLayout->addWidget(terminal);
    mainLayout->addWidget(terminalCard, 1);

    terminal->feed(QByteArrayLiteral(
        "\x1b[38;2;124;160;255m  ╭──────────────────────────────────────────────────────╮\r\n"
        "  │  Nyan SSH · 本地实验台                              │\r\n"
        "  ╰──────────────────────────────────────────────────────╯\x1b[0m\r\n"
        "\r\n"
        "\x1b[38;2;78;201;176mnyan@nyan-server\x1b[0m:\x1b[38;2;108;184;255m~\x1b[0m$ ls -la\r\n"
        "total 24\r\n"
        "drwxrwxrwx 1 nyan nyan 4096 Nov  4 16:20 \x1b[1;34m.\x1b[0m\r\n"
        "drwxrwxrwx 1 nyan nyan 4096 Nov  4 16:20 \x1b[1;34m..\x1b[0m\r\n"
        "drwxrwxrwx 1 nyan nyan 4096 Nov  4 16:20 \x1b[1;34mprojects\x1b[0m\r\n"
        "drwxrwxrwx 1 nyan nyan 4096 Nov  4 16:20 \x1b[1;34m图片\x1b[0m\r\n"
        "-rw-rw-rw- 1 nyan nyan   63 Nov  4 16:20 README.md\r\n"
        "-rw-rw-rw- 1 nyan nyan   26 Nov  4 16:20 notes.txt\r\n"
        "\x1b[38;2;78;201;176mnyan@nyan-server\x1b[0m:\x1b[38;2;108;184;255m~\x1b[0m$ \r\n"));

    auto *bottom = new QHBoxLayout;
    auto *segment = new MSegmentedControl({QStringLiteral("终端"), QStringLiteral("文件"), QStringLiteral("日志")});
    segment->setFixedWidth(300);
    bottom->addWidget(segment);
    bottom->addStretch();
    auto *toggleLabel = label(QStringLiteral("自动重连"), 9);
    bottom->addWidget(toggleLabel);
    auto *toggle = new MSwitch;
    toggle->setChecked(true);
    bottom->addWidget(toggle);
    auto *spinner = new MSpinner;
    spinner->start();
    bottom->addWidget(spinner);
    mainLayout->addLayout(bottom);

    root->addWidget(mainPanel, 1);

    window.show();
    terminal->setFocus();

    const QString screenshotPath = [&] {
        const QStringList args = app.arguments();
        const int index = args.indexOf(QStringLiteral("--screenshot"));
        return index >= 0 && index + 1 < args.size() ? args.at(index + 1) : QString();
    }();

    if (!screenshotPath.isEmpty()) {
        QTimer::singleShot(1200, &app, [&] {
            window.grab().save(screenshotPath);
            app.quit();
        });
    }

    return app.exec();
}
