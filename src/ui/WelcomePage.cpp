#include "ui/WelcomePage.h"

#include "ui/Icons.h"
#include "ui/MaterialWidgets.h"
#include "ui/Theme.h"

#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QIntValidator>
#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPainter>
#include <QRadialGradient>
#include <QScrollArea>
#include <QVBoxLayout>

#include <functional>

namespace nyan::ui {
namespace {

QLabel *mutedLabel(const QString &text, int pointSize = 9)
{
    auto *label = new QLabel(text);
    label->setProperty("role", QStringLiteral("muted"));
    label->setFont(Theme::instance().uiFont(pointSize));
    return label;
}

class RecentCard : public MCard {
public:
    explicit RecentCard(const SessionProfile &profile, QWidget *parent = nullptr)
        : MCard(parent)
        , m_id(profile.id)
    {
        setHoverable(true);
        setCursor(Qt::PointingHandCursor);
        setFixedHeight(68);
        auto *layout = new QHBoxLayout(this);
        layout->setContentsMargins(13, 10, 12, 10);
        layout->setSpacing(11);

        auto *avatar = new QLabel;
        avatar->setFixedSize(36, 36);
        avatar->setPixmap(Icons::logo(32, 2.0));
        layout->addWidget(avatar, 0, Qt::AlignVCenter);

        auto *textLayout = new QVBoxLayout;
        textLayout->setSpacing(3);
        auto *name = new QLabel(profile.displayName());
        name->setFont(Theme::instance().uiFont(9.5, QFont::DemiBold));
        auto *host = new QLabel(profile.subtitle());
        host->setProperty("role", QStringLiteral("muted"));
        host->setFont(Theme::instance().uiFont(8.3));
        textLayout->addWidget(name);
        textLayout->addWidget(host);
        layout->addLayout(textLayout, 1);

        auto *arrow = new QLabel;
        arrow->setPixmap(Icons::pixmap(QStringLiteral("chevron-right"),
                                       Theme::instance().palette().textFaint, 16, 2.0));
        layout->addWidget(arrow);
    }

    std::function<void(const QString &)> activatedCallback;

protected:
    void mouseReleaseEvent(QMouseEvent *event) override
    {
        if (event->button() == Qt::LeftButton && rect().contains(event->position().toPoint())) {
            if (activatedCallback)
                activatedCallback(m_id);
            event->accept();
            return;
        }
        MCard::mouseReleaseEvent(event);
    }

private:
    QString m_id;
};

} // namespace

WelcomePage::WelcomePage(QWidget *parent)
    : QWidget(parent)
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    auto *scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setStyleSheet(QStringLiteral("QScrollArea { background: transparent; }"));
    root->addWidget(scroll);

    auto *content = new QWidget;
    content->setStyleSheet(QStringLiteral("background: transparent;"));
    auto *layout = new QVBoxLayout(content);
    layout->setContentsMargins(42, 32, 42, 32);
    layout->setSpacing(22);

    // Hero -------------------------------------------------------------------
    auto *hero = new QHBoxLayout;
    hero->setSpacing(24);
    auto *heroText = new QVBoxLayout;
    heroText->setSpacing(12);
    auto *title = new QLabel(QStringLiteral("Nyan SSH"));
    title->setFont(Theme::instance().displayFont(30, QFont::DemiBold));
    heroText->addWidget(title);

    auto *chips = new QHBoxLayout;
    chips->setSpacing(7);
    chips->addWidget(new MTag(QStringLiteral("xterm 终端"), Theme::instance().palette().accent));
    chips->addWidget(new MTag(QStringLiteral("SFTP 文件"), Theme::instance().palette().success));
    chips->addWidget(new MTag(QStringLiteral("自动登录"), Theme::instance().palette().warning));
    chips->addStretch();
    heroText->addLayout(chips);

    auto *newButton = new MButton(QStringLiteral("新建连接"), MButton::Variant::Filled);
    newButton->setIconName(QStringLiteral("plus"));
    newButton->setFixedWidth(148);
    connect(newButton, &QAbstractButton::clicked, this, &WelcomePage::newConnectionRequested);
    heroText->addWidget(newButton, 0, Qt::AlignLeft);
    hero->addLayout(heroText, 1);

    auto *logo = new QLabel;
    logo->setPixmap(Icons::logo(104, 2.0));
    logo->setFixedSize(104, 104);
    hero->addWidget(logo, 0, Qt::AlignTop);
    layout->addLayout(hero);

    // Quick connect ----------------------------------------------------------
    auto *quickCard = new MCard;
    quickCard->setShadowEnabled(true);
    quickCard->setCardRadius(14);
    auto *quickLayout = new QVBoxLayout(quickCard);
    quickLayout->setContentsMargins(22, 20, 22, 20);
    quickLayout->setSpacing(12);

    auto *quickHeader = new QHBoxLayout;
    auto *quickTitle = new QLabel(QStringLiteral("快速连接"));
    quickTitle->setFont(Theme::instance().displayFont(13, QFont::DemiBold));
    quickHeader->addWidget(quickTitle);
    quickHeader->addSpacing(10);
    quickHeader->addWidget(mutedLabel(QStringLiteral("无需保存到会话列表，立即打开临时会话"), 8.5), 0,
                           Qt::AlignBottom);
    quickHeader->addStretch();
    auto *bolt = new QLabel;
    bolt->setPixmap(Icons::pixmap(QStringLiteral("sparkles"), Theme::instance().palette().accent, 18, 2.0));
    quickHeader->addWidget(bolt);
    quickLayout->addLayout(quickHeader);

    auto *fields = new QHBoxLayout;
    fields->setSpacing(12);
    m_host = new MTextField(QStringLiteral("主机地址"), QStringLiteral("globe"));
    m_host->setPlaceholder(QStringLiteral("example.com"));
    fields->addWidget(m_host, 5);
    m_port = new MTextField(QStringLiteral("端口"), QStringLiteral("server"));
    m_port->setText(QStringLiteral("22"));
    m_port->lineEdit()->setValidator(new QIntValidator(1, 65535, m_port));
    m_port->setFixedWidth(120);
    fields->addWidget(m_port);
    m_username = new MTextField(QStringLiteral("用户名"), QStringLiteral("server"));
    m_username->setPlaceholder(QStringLiteral("root"));
    fields->addWidget(m_username, 4);
    quickLayout->addLayout(fields);

    auto *passwordRow = new QHBoxLayout;
    passwordRow->setSpacing(12);
    m_password = new MTextField(QStringLiteral("密码（可留空）"), QStringLiteral("lock"));
    m_password->setPasswordMode(true);
    m_password->setPlaceholder(QStringLiteral("留空则连接时询问"));
    passwordRow->addWidget(m_password, 1);
    auto *connectButton = new MButton(QStringLiteral("连接"), MButton::Variant::Filled);
    connectButton->setIconName(QStringLiteral("power"));
    connectButton->setFixedWidth(126);
    connectButton->setFixedHeight(42);
    passwordRow->addWidget(connectButton, 0, Qt::AlignBottom);
    quickLayout->addLayout(passwordRow);

    m_errorLabel = new QLabel;
    m_errorLabel->setFont(Theme::instance().uiFont(8.5));
    m_errorLabel->setStyleSheet(QStringLiteral("color:%1;").arg(Theme::instance().palette().danger.name()));
    m_errorLabel->hide();
    quickLayout->addWidget(m_errorLabel);

    connect(connectButton, &QAbstractButton::clicked, this, &WelcomePage::submitQuickConnect);
    for (MTextField *field : {m_host, m_port, m_username, m_password})
        connect(field, &MTextField::returnPressed, this, &WelcomePage::submitQuickConnect);
    layout->addWidget(quickCard);

    // Recent -----------------------------------------------------------------
    auto *recentHeader = new QHBoxLayout;
    auto *recentTitle = new QLabel(QStringLiteral("最近连接"));
    recentTitle->setFont(Theme::instance().displayFont(12, QFont::DemiBold));
    recentHeader->addWidget(recentTitle);
    recentHeader->addStretch();
    m_statsLabel = mutedLabel(QString(), 8.5);
    recentHeader->addWidget(m_statsLabel);
    layout->addLayout(recentHeader);

    m_recentContainer = new QWidget;
    m_recentGrid = new QGridLayout(m_recentContainer);
    m_recentGrid->setContentsMargins(0, 0, 0, 0);
    m_recentGrid->setHorizontalSpacing(12);
    m_recentGrid->setVerticalSpacing(10);
    layout->addWidget(m_recentContainer);

    m_recentEmpty = new QLabel(QStringLiteral("还没有保存的会话。创建一个连接，让下一次登录只需一次点击。"));
    m_recentEmpty->setProperty("role", QStringLiteral("faint"));
    m_recentEmpty->setFont(Theme::instance().uiFont(9.5));
    m_recentEmpty->setMinimumHeight(68);
    m_recentEmpty->setAlignment(Qt::AlignCenter);
    layout->addWidget(m_recentEmpty);

    layout->addStretch();
    scroll->setWidget(content);

    connect(&Theme::instance(), &Theme::changed, this, [this] {
        update();
        rebuildRecent();
    });
}

void WelcomePage::paintEvent(QPaintEvent *)
{
    const Palette &palette = Theme::instance().palette();
    QPainter painter(this);
    painter.fillRect(rect(), palette.window);

    QRadialGradient topRight(QPointF(width() * 0.88, -40), width() * 0.66);
    QColor accentGlow = palette.accent;
    accentGlow.setAlpha(34);
    topRight.setColorAt(0.0, accentGlow);
    accentGlow.setAlpha(0);
    topRight.setColorAt(1.0, accentGlow);
    painter.fillRect(rect(), topRight);

    QRadialGradient bottomLeft(QPointF(-60, height() * 0.95), width() * 0.5);
    QColor purple = QColor(QStringLiteral("#A855F7"));
    purple.setAlpha(18);
    bottomLeft.setColorAt(0.0, purple);
    purple.setAlpha(0);
    bottomLeft.setColorAt(1.0, purple);
    painter.fillRect(rect(), bottomLeft);

    painter.setPen(QPen(QColor(255, 255, 255, 8), 1));
    for (int x = 20; x < width(); x += 34) {
        for (int y = 12; y < height(); y += 34)
            painter.drawPoint(x, y);
    }
}

void WelcomePage::setProfiles(const QVector<SessionProfile> &profiles)
{
    m_profiles = profiles;
    rebuildRecent();
}

void WelcomePage::setStats(int savedSessions, int trustedHosts)
{
    if (m_statsLabel)
        m_statsLabel->setText(QStringLiteral("%1 个会话 · 已信任 %2 台主机").arg(savedSessions).arg(trustedHosts));
}

void WelcomePage::focusQuickConnect()
{
    if (m_host)
        m_host->lineEdit()->setFocus();
}

void WelcomePage::rebuildRecent()
{
    if (!m_recentGrid)
        return;
    while (QLayoutItem *item = m_recentGrid->takeAt(0)) {
        if (item->widget())
            item->widget()->deleteLater();
        delete item;
    }

    const int shown = qMin(4, m_profiles.size());
    for (int i = 0; i < shown; ++i) {
        auto *card = new RecentCard(m_profiles.at(i), m_recentContainer);
        card->activatedCallback = [this](const QString &id) { emit connectRequested(id); };
        m_recentGrid->addWidget(card, i / 2, i % 2);
    }
    m_recentGrid->setColumnStretch(0, 1);
    m_recentGrid->setColumnStretch(1, 1);
    m_recentEmpty->setVisible(shown == 0);
    m_recentContainer->setVisible(shown > 0);
}

void WelcomePage::submitQuickConnect()
{
    m_host->setError(false);
    m_username->setError(false);
    m_port->setError(false);
    m_errorLabel->hide();

    const QString host = m_host->text().trimmed();
    const QString username = m_username->text().trimmed();
    if (host.isEmpty()) {
        m_host->setError(true, QStringLiteral("请输入主机地址"));
        m_errorLabel->setText(QStringLiteral("请至少填写主机地址与用户名"));
        m_errorLabel->show();
        m_host->lineEdit()->setFocus();
        return;
    }
    if (username.isEmpty()) {
        m_username->setError(true, QStringLiteral("请输入用户名"));
        m_errorLabel->setText(QStringLiteral("请至少填写主机地址与用户名"));
        m_errorLabel->show();
        m_username->lineEdit()->setFocus();
        return;
    }
    bool portOk = false;
    const int port = m_port->text().toInt(&portOk);
    if (!portOk || port < 1 || port > 65535) {
        m_port->setError(true, QStringLiteral("端口无效"));
        m_errorLabel->setText(QStringLiteral("端口必须位于 1-65535 之间"));
        m_errorLabel->show();
        return;
    }

    SessionProfile profile;
    profile.name = QStringLiteral("%1@%2").arg(username, host);
    profile.host = host;
    profile.port = static_cast<quint16>(port);
    profile.username = username;
    profile.auth = AuthType::Password;
    profile.setPassword(m_password->text(), false);
    profile.autoLogin = false;
    profile.color = SessionProfile::defaultColorFor(host + username);
    emit quickConnectRequested(profile);
}

} // namespace nyan::ui
