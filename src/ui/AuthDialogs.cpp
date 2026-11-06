#include "ui/AuthDialogs.h"

#include "ui/Icons.h"
#include "ui/Theme.h"

#include <QApplication>
#include <QClipboard>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QToolTip>
#include <QVBoxLayout>

namespace nyan::ui {

HostKeyDialog::HostKeyDialog(QWidget *parent, const QString &host, quint16 port, const QString &keyType,
                             const QString &fingerprint, const QString &expectedFingerprint)
    : MDialog(QStringLiteral("验证主机密钥"), parent)
{
    const Palette &palette = Theme::instance().palette();
    const bool mismatch = !expectedFingerprint.isEmpty() && expectedFingerprint != fingerprint;

    setTitle(mismatch ? QStringLiteral("主机密钥已改变") : QStringLiteral("验证主机密钥"));
    setSubtitle(QStringLiteral("%1:%2").arg(host).arg(port));
    setPreferredWidth(560);

    auto *notice = new QWidget;
    auto *noticeLayout = new QHBoxLayout(notice);
    noticeLayout->setContentsMargins(0, 0, 0, 0);
    noticeLayout->setSpacing(14);
    auto *icon = new QLabel;
    icon->setPixmap(Icons::pixmap(mismatch ? QStringLiteral("alert") : QStringLiteral("shield"),
                                  mismatch ? palette.danger : palette.accent, 26, 2.0));
    icon->setFixedSize(28, 28);
    noticeLayout->addWidget(icon, 0, Qt::AlignTop);

    auto *message = new QLabel(
        mismatch
            ? QStringLiteral("服务器提供的 SSH 主机密钥与本地记录不一致。服务器重装可能造成该变化，"
                             "但也可能意味着中间人攻击。请与服务器管理员核实指纹后再继续。")
            : QStringLiteral("这是首次连接该主机。请确认下方指纹与服务器管理员提供的信息一致，"
                             "以防范中间人攻击。"));
    message->setWordWrap(true);
    message->setFont(Theme::instance().uiFont(10));
    message->setTextInteractionFlags(Qt::TextSelectableByMouse);
    noticeLayout->addWidget(message, 1);
    contentLayout()->addWidget(notice);

    auto *card = new MCard;
    card->setSurfaceColor(palette.surfaceAlt);
    auto *cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(14, 10, 10, 12);
    cardLayout->setSpacing(6);

    auto *typeRow = new QHBoxLayout;
    auto *typeLabel = new QLabel(QStringLiteral("密钥类型"));
    typeLabel->setProperty("role", QStringLiteral("muted"));
    typeLabel->setFont(Theme::instance().uiFont(8.5));
    auto *typeValue = new QLabel(keyType);
    typeValue->setFont(Theme::instance().monoFont(9));
    typeRow->addWidget(typeLabel);
    typeRow->addStretch();
    typeRow->addWidget(typeValue);
    cardLayout->addLayout(typeRow);

    auto *fingerprintRow = new QHBoxLayout;
    fingerprintRow->setSpacing(8);
    auto *fingerprintLabel = new QLabel(fingerprint);
    fingerprintLabel->setFont(Theme::instance().monoFont(9));
    fingerprintLabel->setWordWrap(true);
    fingerprintLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    fingerprintRow->addWidget(fingerprintLabel, 1);
    auto *copyButton = new MIconButton(QStringLiteral("copy"), QStringLiteral("复制指纹"), card);
    copyButton->setButtonSize(28);
    connect(copyButton, &QAbstractButton::clicked, this, [fingerprint, copyButton] {
        QGuiApplication::clipboard()->setText(fingerprint);
        QToolTip::showText(copyButton->mapToGlobal(QPoint(copyButton->width() / 2, copyButton->height())),
                           QStringLiteral("已复制"), copyButton);
    });
    fingerprintRow->addWidget(copyButton, 0, Qt::AlignTop);
    cardLayout->addLayout(fingerprintRow);

    if (mismatch) {
        auto *oldLabel = new QLabel(QStringLiteral("此前记录：%1").arg(expectedFingerprint));
        oldLabel->setFont(Theme::instance().monoFont(8));
        oldLabel->setProperty("role", QStringLiteral("muted"));
        oldLabel->setWordWrap(true);
        cardLayout->addWidget(oldLabel);
    }
    contentLayout()->addWidget(card);

    auto *rememberRow = new QWidget;
    auto *rememberLayout = new QHBoxLayout(rememberRow);
    rememberLayout->setContentsMargins(0, 2, 0, 0);
    rememberLayout->setSpacing(12);
    auto *rememberText = new QVBoxLayout;
    rememberText->setSpacing(2);
    auto *rememberTitle = new QLabel(QStringLiteral("信任并记住此主机"));
    rememberTitle->setFont(Theme::instance().uiFont(9.5, QFont::Medium));
    auto *rememberHint = new QLabel(mismatch ? QStringLiteral("仅在你确认指纹无误时开启")
                                             : QStringLiteral("后续连接将自动校验该指纹"));
    rememberHint->setProperty("role", QStringLiteral("muted"));
    rememberHint->setFont(Theme::instance().uiFont(8.5));
    rememberText->addWidget(rememberTitle);
    rememberText->addWidget(rememberHint);
    rememberLayout->addLayout(rememberText, 1);

    m_remember = new MSwitch;
    m_remember->setChecked(!mismatch);
    rememberLayout->addWidget(m_remember, 0, Qt::AlignVCenter);
    contentLayout()->addWidget(rememberRow);

    auto *cancel = new MButton(QStringLiteral("取消"), MButton::Variant::Outlined);
    auto *accept = new MButton(mismatch ? QStringLiteral("仍然连接") : QStringLiteral("信任并连接"),
                               mismatch ? MButton::Variant::Danger : MButton::Variant::Filled);
    connect(cancel, &QAbstractButton::clicked, this, &QDialog::reject);
    connect(accept, &QAbstractButton::clicked, this, &QDialog::accept);
    addFooterButton(cancel);
    addFooterButton(accept);
    accept->setFocus();
}

bool HostKeyDialog::rememberKey() const
{
    return m_remember && m_remember->isChecked();
}

PasswordDialog::PasswordDialog(QWidget *parent, Mode mode, const QString &target, const QString &error,
                               bool rememberDefault)
    : MDialog(QStringLiteral("输入密码"), parent)
    , m_mode(mode)
{
    const Palette &palette = Theme::instance().palette();
    setTitle(mode == Mode::Password ? (error.isEmpty() ? QStringLiteral("输入密码")
                                                       : QStringLiteral("认证失败"))
                                    : QStringLiteral("输入私钥口令"));
    setSubtitle(target);
    setPreferredWidth(460);

    if (!error.isEmpty()) {
        auto *banner = new QWidget;
        auto *bannerLayout = new QHBoxLayout(banner);
        bannerLayout->setContentsMargins(0, 0, 0, 0);
        bannerLayout->setSpacing(10);
        auto *icon = new QLabel;
        icon->setPixmap(Icons::pixmap(QStringLiteral("alert"), palette.danger, 18, 2.0));
        icon->setFixedSize(20, 20);
        bannerLayout->addWidget(icon, 0, Qt::AlignTop);
        auto *label = new QLabel(error);
        label->setWordWrap(true);
        label->setFont(Theme::instance().uiFont(9));
        label->setStyleSheet(QStringLiteral("color:%1;").arg(palette.danger.name()));
        bannerLayout->addWidget(label, 1);
        contentLayout()->addWidget(banner);
    }

    m_field = new MTextField(mode == Mode::Password ? QStringLiteral("密码") : QStringLiteral("口令"),
                             mode == Mode::Password ? QStringLiteral("lock") : QStringLiteral("key"));
    m_field->setPasswordMode(true);
    m_field->lineEdit()->setPlaceholderText(QStringLiteral("输入后按回车连接"));
    contentLayout()->addWidget(m_field);

    auto *rememberRow = new QWidget;
    auto *rememberLayout = new QHBoxLayout(rememberRow);
    rememberLayout->setContentsMargins(0, 0, 0, 0);
    rememberLayout->setSpacing(12);
    auto *rememberLabel = new QLabel(mode == Mode::Password ? QStringLiteral("记住密码（使用 Windows DPAPI 加密）")
                                                            : QStringLiteral("记住口令（使用 Windows DPAPI 加密）"));
    rememberLabel->setFont(Theme::instance().uiFont(9));
    rememberLabel->setProperty("role", QStringLiteral("muted"));
    rememberLayout->addWidget(rememberLabel, 1);
    m_remember = new MSwitch;
    m_remember->setChecked(rememberDefault);
    rememberLayout->addWidget(m_remember);
    contentLayout()->addWidget(rememberRow);

    auto *cancel = new MButton(QStringLiteral("取消"), MButton::Variant::Outlined);
    auto *connectButton = new MButton(QStringLiteral("连接"), MButton::Variant::Filled);
    connect(cancel, &QAbstractButton::clicked, this, &QDialog::reject);
    connect(connectButton, &QAbstractButton::clicked, this, [this] {
        if (m_field->text().isEmpty()) {
            m_field->setError(true, QStringLiteral("请输入内容"));
            return;
        }
        QDialog::accept();
    });
    connect(m_field, &MTextField::returnPressed, this, [this] {
        if (!m_field->text().isEmpty())
            QDialog::accept();
    });
    addFooterButton(cancel);
    addFooterButton(connectButton);
    m_field->lineEdit()->setFocus();
}

QString PasswordDialog::secret() const
{
    return m_field ? m_field->text() : QString();
}

bool PasswordDialog::remember() const
{
    return m_remember && m_remember->isChecked();
}

} // namespace nyan::ui
