#include "ui/Sidebar.h"

#include "ui/Icons.h"
#include "ui/MaterialWidgets.h"
#include "ui/Theme.h"

#include <QContextMenuEvent>
#include <QDateTime>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QScrollArea>
#include <QVBoxLayout>

#include <algorithm>
#include <functional>

namespace nyan::ui {
namespace {

class SessionAvatar : public QWidget {
public:
    SessionAvatar(const QColor &color, QWidget *parent = nullptr)
        : QWidget(parent)
        , m_color(color)
    {
        setFixedSize(38, 38);
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);
        QColor background = m_color;
        background.setAlpha(36);
        painter.setPen(QPen(QColor(m_color.red(), m_color.green(), m_color.blue(), 85), 1));
        painter.setBrush(background);
        painter.drawRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), 11, 11);
        const QPixmap iconPixmap = Icons::pixmap(QStringLiteral("server"), m_color, 20, 2.0);
        painter.drawPixmap(QRect(9, 9, 20, 20), iconPixmap);
    }

private:
    QColor m_color;
};

class SessionCard : public MCard {
public:
    SessionCard(const SessionProfile &profile, QWidget *parent = nullptr)
        : MCard(parent)
        , m_profile(profile)
    {
        setHoverable(true);
        setCursor(Qt::PointingHandCursor);
        setFixedHeight(76);
        setToolTip(QStringLiteral("%1\n%2").arg(profile.displayName(), profile.subtitle()));

        auto *layout = new QHBoxLayout(this);
        layout->setContentsMargins(12, 10, 10, 10);
        layout->setSpacing(11);

        const QColor color(profile.color.isEmpty() ? SessionProfile::defaultColorFor(profile.id)
                                                   : profile.color);
        layout->addWidget(new SessionAvatar(color, this), 0, Qt::AlignVCenter);

        auto *textLayout = new QVBoxLayout;
        textLayout->setSpacing(3);
        auto *name = new QLabel(profile.displayName(), this);
        name->setFont(Theme::instance().uiFont(9.5, QFont::DemiBold));
        auto *host = new QLabel(profile.subtitle(), this);
        host->setProperty("role", QStringLiteral("muted"));
        host->setFont(Theme::instance().uiFont(8.3));
        textLayout->addWidget(name);
        textLayout->addWidget(host);

        auto *tags = new QHBoxLayout;
        tags->setContentsMargins(0, 1, 0, 0);
        tags->setSpacing(5);
        const bool autoTag = profile.autoLogin && profile.isUsableForAutoLogin();
        if (autoTag)
            tags->addWidget(new MTag(QStringLiteral("自动登录"), Theme::instance().palette().success, this));
        if (profile.auth == AuthType::PublicKey)
            tags->addWidget(new MTag(QStringLiteral("密钥"), Theme::instance().palette().warning, this));
        else if (!autoTag)
            tags->addWidget(new MTag(QStringLiteral("密码"), color, this));
        if (profile.autoLogin && !profile.isUsableForAutoLogin())
            tags->addWidget(new MTag(QStringLiteral("待补全"), Theme::instance().palette().textMuted, this));
        tags->addStretch();
        textLayout->addLayout(tags);
        layout->addLayout(textLayout, 1);

        auto *chevron = new QLabel(this);
        chevron->setPixmap(Icons::pixmap(QStringLiteral("chevron-right"), Theme::instance().palette().textFaint,
                                         16, 2.0));
        layout->addWidget(chevron, 0, Qt::AlignVCenter);
    }

    std::function<void()> connectCallback;
    std::function<void()> editCallback;
    std::function<void()> duplicateCallback;
    std::function<void()> deleteCallback;

protected:
    void mouseReleaseEvent(QMouseEvent *event) override
    {
        if (event->button() == Qt::LeftButton && rect().contains(event->position().toPoint())) {
            if (connectCallback)
                connectCallback();
            event->accept();
            return;
        }
        MCard::mouseReleaseEvent(event);
    }

    void contextMenuEvent(QContextMenuEvent *event) override
    {
        QMenu menu(this);
        QAction *connectAction = menu.addAction(QStringLiteral("连接"));
        QAction *editAction = menu.addAction(QStringLiteral("编辑"));
        QAction *duplicateAction = menu.addAction(QStringLiteral("复制会话"));
        menu.addSeparator();
        QAction *deleteAction = menu.addAction(QStringLiteral("删除"));
        QAction *chosen = menu.exec(event->globalPos());
        if (!chosen)
            return;
        if (chosen == connectAction && connectCallback)
            connectCallback();
        else if (chosen == editAction && editCallback)
            editCallback();
        else if (chosen == duplicateAction && duplicateCallback)
            duplicateCallback();
        else if (chosen == deleteAction && deleteCallback)
            deleteCallback();
        event->accept();
    }

private:
    SessionProfile m_profile;
};

} // namespace

Sidebar::Sidebar(QWidget *parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("sidebar"));
    setAttribute(Qt::WA_StyledBackground, true);
    setFixedWidth(300);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(16, 16, 16, 16);
    root->setSpacing(12);

    auto *newButton = new MButton(QStringLiteral("新建连接"), MButton::Variant::Filled);
    newButton->setIconName(QStringLiteral("plus"));
    connect(newButton, &QAbstractButton::clicked, this, &Sidebar::newConnectionRequested);
    root->addWidget(newButton);

    m_search = new MTextField(QStringLiteral("搜索会话"), QStringLiteral("search"));
    m_search->setPlaceholder(QStringLiteral("名称、主机或用户名"));
    connect(m_search, &MTextField::textChanged, this, [this](const QString &) { applyFilter(); });
    root->addWidget(m_search);

    auto *sectionRow = new QHBoxLayout;
    sectionRow->setContentsMargins(2, 4, 2, 0);
    auto *section = new QLabel(QStringLiteral("会话"));
    section->setProperty("role", QStringLiteral("muted"));
    section->setFont(Theme::instance().uiFont(8.5, QFont::DemiBold));
    sectionRow->addWidget(section);
    sectionRow->addStretch();
    m_countLabel = new QLabel;
    m_countLabel->setProperty("role", QStringLiteral("faint"));
    m_countLabel->setFont(Theme::instance().uiFont(8));
    sectionRow->addWidget(m_countLabel);
    root->addLayout(sectionRow);

    auto *scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scroll->setStyleSheet(QStringLiteral("QScrollArea { background: transparent; }"));
    m_listContainer = new QWidget;
    m_listContainer->setStyleSheet(QStringLiteral("background: transparent;"));
    m_listLayout = new QVBoxLayout(m_listContainer);
    m_listLayout->setContentsMargins(0, 0, 0, 0);
    m_listLayout->setSpacing(8);

    m_emptyLabel = new QLabel(QStringLiteral("还没有保存的会话\n点击上方按钮创建第一个连接"));
    m_emptyLabel->setAlignment(Qt::AlignCenter);
    m_emptyLabel->setWordWrap(true);
    m_emptyLabel->setProperty("role", QStringLiteral("faint"));
    m_emptyLabel->setFont(Theme::instance().uiFont(9));
    m_emptyLabel->setMinimumHeight(120);
    m_listLayout->addWidget(m_emptyLabel);
    m_listLayout->addStretch();
    scroll->setWidget(m_listContainer);
    root->addWidget(scroll, 1);

    auto *footerCard = new MCard;
    footerCard->setFixedHeight(54);
    auto *footerLayout = new QHBoxLayout(footerCard);
    footerLayout->setContentsMargins(12, 8, 8, 8);
    footerLayout->setSpacing(8);
    m_footerDot = new QLabel;
    m_footerDot->setFixedSize(8, 8);
    m_footerDot->setStyleSheet(QStringLiteral("background:%1;border-radius:4px;")
                                   .arg(Theme::instance().palette().textFaint.name()));
    footerLayout->addWidget(m_footerDot);
    m_footerLabel = new QLabel(QStringLiteral("暂无活动会话"));
    m_footerLabel->setProperty("role", QStringLiteral("muted"));
    m_footerLabel->setFont(Theme::instance().uiFont(8.5));
    footerLayout->addWidget(m_footerLabel, 1);
    auto *settings = new MIconButton(QStringLiteral("settings"), QStringLiteral("设置"), footerCard);
    settings->setButtonSize(30);
    connect(settings, &QAbstractButton::clicked, this, &Sidebar::settingsRequested);
    footerLayout->addWidget(settings);
    root->addWidget(footerCard);

    connect(&Theme::instance(), &Theme::changed, this, [this] {
        update();
        rebuild();
    });
}

void Sidebar::paintEvent(QPaintEvent *)
{
    const Palette &palette = Theme::instance().palette();
    QPainter painter(this);
    painter.fillRect(rect(), palette.sidebar);
    painter.setPen(QPen(palette.border, 1));
    painter.drawLine(width() - 1, 0, width() - 1, height());
}

void Sidebar::setProfiles(const QVector<SessionProfile> &profiles)
{
    m_profiles = profiles;
    std::sort(m_profiles.begin(), m_profiles.end(), [](const SessionProfile &a, const SessionProfile &b) {
        const QDateTime at = a.lastUsedAt.isValid() ? a.lastUsedAt : a.createdAt;
        const QDateTime bt = b.lastUsedAt.isValid() ? b.lastUsedAt : b.createdAt;
        if (at == bt)
            return a.name.toLower() < b.name.toLower();
        return at > bt;
    });
    rebuild();
}

void Sidebar::rebuild()
{
    if (!m_listLayout)
        return;

    for (int i = m_listLayout->count() - 1; i >= 0; --i) {
        QLayoutItem *item = m_listLayout->takeAt(i);
        if (item->widget() && item->widget() != m_emptyLabel)
            item->widget()->deleteLater();
        delete item;
    }
    m_listLayout->addWidget(m_emptyLabel);
    m_listLayout->addStretch();

    for (const SessionProfile &profile : m_profiles) {
        auto *card = new SessionCard(profile, m_listContainer);
        card->connectCallback = [this, id = profile.id] { emit connectRequested(id); };
        card->editCallback = [this, id = profile.id] { emit editRequested(id); };
        card->duplicateCallback = [this, id = profile.id] { emit duplicateRequested(id); };
        card->deleteCallback = [this, id = profile.id] { emit deleteRequested(id); };
        m_listLayout->insertWidget(m_listLayout->count() - 1, card);
    }

    m_countLabel->setText(QStringLiteral("%1 个").arg(m_profiles.size()));
    applyFilter();
}

void Sidebar::applyFilter()
{
    const QString needle = m_search ? m_search->text().trimmed().toLower() : QString();
    int visible = 0;
    for (int i = 0; i < m_listLayout->count(); ++i) {
        QLayoutItem *item = m_listLayout->itemAt(i);
        if (!item || !item->widget() || item->widget() == m_emptyLabel)
            continue;
        const bool match = needle.isEmpty() || item->widget()->toolTip().toLower().contains(needle);
        item->widget()->setVisible(match);
        if (match)
            ++visible;
    }
    m_emptyLabel->setVisible(visible == 0);
    if (visible == 0 && !needle.isEmpty())
        m_emptyLabel->setText(QStringLiteral("没有匹配的会话"));
    else
        m_emptyLabel->setText(QStringLiteral("还没有保存的会话\n点击上方按钮创建第一个连接"));
}

void Sidebar::setActiveSessionCount(int count)
{
    m_activeCount = count;
    setFooterText(count > 0 ? QStringLiteral("%1 个活动会话").arg(count)
                           : QStringLiteral("暂无活动会话"));
    if (m_footerDot) {
        const QColor color = count > 0 ? Theme::instance().palette().success
                                       : Theme::instance().palette().textFaint;
        m_footerDot->setStyleSheet(
            QStringLiteral("background:%1;border-radius:4px;").arg(color.name()));
    }
}

void Sidebar::setFooterText(const QString &text)
{
    if (m_footerLabel)
        m_footerLabel->setText(text);
}

} // namespace nyan::ui
