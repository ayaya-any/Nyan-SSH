#include "ui/ConnectionDialog.h"

#include "ui/Icons.h"
#include "ui/Theme.h"

#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QIntValidator>
#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPainter>
#include <QTimer>
#include <QUuid>
#include <QVBoxLayout>
#include <QWidget>

#include <algorithm>
#include <functional>

namespace nyan::ui {
namespace {

QLabel *sectionLabel(const QString &text)
{
    auto *label = new QLabel(text);
    label->setProperty("role", QStringLiteral("muted"));
    label->setFont(Theme::instance().uiFont(8.5, QFont::Medium));
    return label;
}

QWidget *optionRow(const QString &title, const QString &hint, MSwitch *toggle)
{
    auto *row = new QWidget;
    auto *layout = new QHBoxLayout(row);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(10);
    auto *textLayout = new QVBoxLayout;
    textLayout->setSpacing(2);
    auto *titleLabel = new QLabel(title);
    titleLabel->setFont(Theme::instance().uiFont(9, QFont::Medium));
    auto *hintLabel = new QLabel(hint);
    hintLabel->setFont(Theme::instance().uiFont(8.5));
    hintLabel->setProperty("role", QStringLiteral("muted"));
    textLayout->addWidget(titleLabel);
    textLayout->addWidget(hintLabel);
    layout->addLayout(textLayout, 1);
    layout->addWidget(toggle, 0, Qt::AlignVCenter);
    return row;
}

} // namespace

class ColorDots : public QWidget {
public:
    explicit ColorDots(QWidget *parent = nullptr)
        : QWidget(parent)
        , m_colors(SessionProfile::accentPalette())
    {
        setFixedHeight(28);
        setMinimumWidth(kMargin * 2 + m_colors.size() * kSize
                        + qMax(0, m_colors.size() - 1) * kSpacing);
        setCursor(Qt::PointingHandCursor);
        setToolTip(QStringLiteral("选择会话标识颜色"));
    }

    void setColor(const QString &color)
    {
        m_color = color;
        update();
    }

    QString color() const { return m_color; }
    std::function<void(const QString &)> colorChanged;

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);
        int x = kMargin;
        for (const QString &name : m_colors) {
            const QColor color(name);
            const QRectF circle(x, (height() - kSize) / 2.0, kSize, kSize);
            painter.setPen(Qt::NoPen);
            painter.setBrush(color);
            painter.drawEllipse(circle);
            if (name.compare(m_color, Qt::CaseInsensitive) == 0) {
                painter.setBrush(Qt::NoBrush);
                painter.setPen(QPen(Theme::instance().palette().text, 2));
                // The ring extends 3px outside the dot; the 5px margin keeps
                // the first and last rings inside the widget.
                painter.drawEllipse(circle.adjusted(-3, -3, 3, 3));
            }
            x += kSize + kSpacing;
        }
    }

    void mousePressEvent(QMouseEvent *event) override
    {
        const qreal relativeX = event->position().x() - kMargin;
        if (relativeX < 0)
            return;
        const int index = static_cast<int>(relativeX) / (kSize + kSpacing);
        if (index >= 0 && index < m_colors.size()) {
            m_color = m_colors.at(index);
            update();
            if (colorChanged)
                colorChanged(m_color);
        }
    }

private:
    static constexpr int kSize = 16;
    static constexpr int kSpacing = 10;
    static constexpr int kMargin = 5;
    QStringList m_colors;
    QString m_color;
};

ConnectionDialog::ConnectionDialog(QWidget *parent, Mode mode, const SessionProfile *existing)
    : MDialog(mode == Mode::Create ? QStringLiteral("新建连接") : QStringLiteral("编辑连接"), parent)
    , m_mode(mode)
{
    if (existing)
        m_profile = *existing;
    else
        m_profile.rememberPassword = true;
    if (m_profile.color.isEmpty())
        m_profile.color = SessionProfile::defaultColorFor(m_profile.id);

    setPreferredWidth(580);
    setSubtitle(mode == Mode::Create ? QStringLiteral("保存会话并一键登录，凭据由 Windows DPAPI 加密")
                                     : QStringLiteral("修改连接信息与登录偏好"));

    auto *row1 = new QHBoxLayout;
    row1->setSpacing(12);
    m_name = new MTextField(QStringLiteral("会话名称"), QStringLiteral("terminal"));
    m_name->setPlaceholder(QStringLiteral("例如：生产服务器"));
    row1->addWidget(m_name, 1);
    m_username = new MTextField(QStringLiteral("用户名"), QStringLiteral("server"));
    m_username->setPlaceholder(QStringLiteral("nyan"));
    row1->addWidget(m_username, 1);
    contentLayout()->addLayout(row1);

    auto *row2 = new QHBoxLayout;
    row2->setSpacing(12);
    m_host = new MTextField(QStringLiteral("主机地址"), QStringLiteral("globe"));
    m_host->setPlaceholder(QStringLiteral("example.com 或 10.0.0.12"));
    row2->addWidget(m_host, 1);
    m_port = new MTextField(QStringLiteral("端口"), QStringLiteral("server"));
    m_port->setPlaceholder(QStringLiteral("22"));
    m_port->lineEdit()->setValidator(new QIntValidator(1, 65535, m_port));
    m_port->setFixedWidth(112);
    row2->addWidget(m_port);
    contentLayout()->addLayout(row2);

    auto *authRow = new QHBoxLayout;
    authRow->setSpacing(12);
    authRow->addWidget(sectionLabel(QStringLiteral("认证方式")));
    m_auth = new MSegmentedControl({QStringLiteral("密码"), QStringLiteral("密钥")});
    m_auth->setFixedWidth(200);
    authRow->addWidget(m_auth);
    authRow->addStretch();
    authRow->addWidget(sectionLabel(QStringLiteral("标识颜色")));
    m_colorDots = new ColorDots;
    m_colorDots->setColor(m_profile.color);
    m_colorDots->colorChanged = [this](const QString &color) { m_profile.color = color; };
    authRow->addWidget(m_colorDots);
    contentLayout()->addLayout(authRow);

    // Password section -------------------------------------------------------
    m_passwordSection = new QWidget;
    auto *passwordLayout = new QVBoxLayout(m_passwordSection);
    passwordLayout->setContentsMargins(0, 0, 0, 0);
    passwordLayout->setSpacing(12);
    m_password = new MTextField(QStringLiteral("密码"), QStringLiteral("lock"));
    m_password->setPasswordMode(true);
    m_password->setPlaceholder(QStringLiteral("保存后下次自动登录"));
    passwordLayout->addWidget(m_password);
    contentLayout()->addWidget(m_passwordSection);

    // Key section ------------------------------------------------------------
    m_keySection = new QWidget;
    auto *keyLayout = new QVBoxLayout(m_keySection);
    keyLayout->setContentsMargins(0, 0, 0, 0);
    keyLayout->setSpacing(12);
    auto *keyRow = new QHBoxLayout;
    keyRow->setSpacing(10);
    m_keyPath = new MTextField(QStringLiteral("私钥文件"), QStringLiteral("key"));
    m_keyPath->setPlaceholder(QStringLiteral("id_ed25519 / id_rsa"));
    keyRow->addWidget(m_keyPath, 1);
    m_browseKey = new MButton(QStringLiteral("浏览"), MButton::Variant::Outlined);
    m_browseKey->setIconName(QStringLiteral("folder-open"));
    m_browseKey->setFixedHeight(38);
    connect(m_browseKey, &QAbstractButton::clicked, this, &ConnectionDialog::browseForKey);
    keyRow->addWidget(m_browseKey, 0, Qt::AlignBottom);
    keyLayout->addLayout(keyRow);
    m_passphrase = new MTextField(QStringLiteral("私钥口令（可选）"), QStringLiteral("shield"));
    m_passphrase->setPasswordMode(true);
    keyLayout->addWidget(m_passphrase);
    contentLayout()->addWidget(m_keySection);

    // Login preferences ------------------------------------------------------
    auto *optionsRow = new QHBoxLayout;
    optionsRow->setSpacing(24);
    m_remember = new MSwitch;
    auto *rememberRow = optionRow(QStringLiteral("记住凭据"),
                                  QStringLiteral("使用 Windows DPAPI 加密保存"),
                                  m_remember);
    optionsRow->addWidget(rememberRow, 1);

    m_autoLoginRow = new QWidget;
    auto *autoLayout = new QHBoxLayout(m_autoLoginRow);
    autoLayout->setContentsMargins(0, 0, 0, 0);
    auto *autoText = new QVBoxLayout;
    autoText->setSpacing(2);
    auto *autoTitle = new QLabel(QStringLiteral("自动登录"));
    autoTitle->setFont(Theme::instance().uiFont(9, QFont::Medium));
    auto *autoHint = new QLabel(QStringLiteral("启动应用时自动连接"));
    autoHint->setProperty("role", QStringLiteral("muted"));
    autoHint->setFont(Theme::instance().uiFont(8.5));
    autoText->addWidget(autoTitle);
    autoText->addWidget(autoHint);
    autoLayout->addLayout(autoText, 1);
    m_autoLogin = new MSwitch;
    autoLayout->addWidget(m_autoLogin);
    optionsRow->addWidget(m_autoLoginRow, 1);
    contentLayout()->addLayout(optionsRow);

    m_saveRow = new QWidget;
    auto *saveLayout = new QHBoxLayout(m_saveRow);
    saveLayout->setContentsMargins(0, 0, 0, 0);
    auto *saveText = new QVBoxLayout;
    saveText->setSpacing(2);
    auto *saveTitle = new QLabel(QStringLiteral("保存到会话列表"));
    saveTitle->setFont(Theme::instance().uiFont(9, QFont::Medium));
    auto *saveHint = new QLabel(QStringLiteral("以后可以从侧边栏快速连接"));
    saveHint->setProperty("role", QStringLiteral("muted"));
    saveHint->setFont(Theme::instance().uiFont(8.5));
    saveText->addWidget(saveTitle);
    saveText->addWidget(saveHint);
    saveLayout->addLayout(saveText, 1);
    m_saveToLibrary = new MSwitch;
    m_saveToLibrary->setChecked(true);
    saveLayout->addWidget(m_saveToLibrary);
    contentLayout()->addWidget(m_saveRow);
    if (m_mode == Mode::Edit)
        m_saveRow->hide();

    m_errorLabel = new QLabel;
    m_errorLabel->setWordWrap(true);
    m_errorLabel->setFont(Theme::instance().uiFont(9));
    m_errorLabel->setStyleSheet(
        QStringLiteral("color:%1;").arg(Theme::instance().palette().danger.name()));
    m_errorLabel->hide();
    contentLayout()->addWidget(m_errorLabel);

    connect(m_auth, &MSegmentedControl::currentChanged, this, [this](int) {
        updateAuthSection();
        updateAutoLoginState();
    });
    connect(m_remember, &QAbstractButton::toggled, this, [this](bool) { updateAutoLoginState(); });
    connect(m_saveToLibrary, &QAbstractButton::toggled, this, [this](bool) { updateAutoLoginState(); });
    connect(m_keyPath, &MTextField::textChanged, this, [this](const QString &) { updateAutoLoginState(); });

    auto *cancel = new MButton(QStringLiteral("取消"), MButton::Variant::Outlined);
    connect(cancel, &QAbstractButton::clicked, this, &QDialog::reject);
    addFooterButton(cancel);

    if (m_mode == Mode::Edit) {
        auto *save = new MButton(QStringLiteral("保存"), MButton::Variant::Outlined);
        connect(save, &QAbstractButton::clicked, this, [this] { tryAccept(false); });
        addFooterButton(save);

        auto *saveConnect = new MButton(QStringLiteral("保存并连接"), MButton::Variant::Filled);
        saveConnect->setIconName(QStringLiteral("power"));
        connect(saveConnect, &QAbstractButton::clicked, this, [this] { tryAccept(true); });
        addFooterButton(saveConnect);
        saveConnect->setFocus();
    } else {
        auto *connectButton = new MButton(QStringLiteral("连接"), MButton::Variant::Filled);
        connectButton->setIconName(QStringLiteral("log-in"));
        connect(connectButton, &QAbstractButton::clicked, this, [this] { tryAccept(true); });
        addFooterButton(connectButton);
        connectButton->setFocus();
    }

    populate();
    updateAuthSection();
    updateAutoLoginState();
}

void ConnectionDialog::populate()
{
    m_name->setText(m_profile.name);
    m_host->setText(m_profile.host);
    m_port->setText(QString::number(m_profile.port));
    m_username->setText(m_profile.username);
    if (m_profile.auth == AuthType::PublicKey) {
        m_auth->setCurrentIndex(1, false);
        m_keyPath->setText(m_profile.privateKeyPath);
        m_passphrase->setText(m_profile.passphrase());
    } else {
        m_auth->setCurrentIndex(0, false);
        m_password->setText(m_profile.password());
    }
    m_remember->setChecked(m_profile.rememberPassword);
    m_autoLogin->setChecked(m_profile.autoLogin);
    if (m_colorDots)
        m_colorDots->setColor(m_profile.color);
}

void ConnectionDialog::updateAuthSection()
{
    const bool keyAuth = m_auth->currentIndex() == 1;
    m_passwordSection->setVisible(!keyAuth);
    m_keySection->setVisible(keyAuth);
    if (m_mode == Mode::Edit) {
        m_save = true;
    } else {
        m_save = m_saveToLibrary->isChecked();
    }
    layout()->invalidate();
    layout()->activate();
    if (isVisible()) {
        // Visibility changes reach the layout on the next event-loop turn.
        // Defer the resize so hiding the key section makes the dialog smaller
        // again instead of sizing for both sections.
        QTimer::singleShot(0, this, [this] {
            if (!isVisible())
                return;
            if (QLayout *dialogLayout = layout()) {
                dialogLayout->invalidate();
                dialogLayout->activate();
            }
            adjustSize();
        });
    }
}

void ConnectionDialog::updateAutoLoginState()
{
    const bool keyAuth = m_auth->currentIndex() == 1;
    const bool saveEnabled = m_mode == Mode::Edit || m_saveToLibrary->isChecked();
    const bool canAutoLogin = saveEnabled
                              && (keyAuth ? !m_keyPath->text().trimmed().isEmpty()
                                          : m_remember->isChecked());
    m_autoLogin->setEnabled(canAutoLogin);
    m_autoLoginRow->setEnabled(canAutoLogin);
    if (!canAutoLogin && m_autoLogin->isChecked())
        m_autoLogin->setChecked(false);
}

void ConnectionDialog::browseForKey()
{
    const QString start = m_keyPath->text().trimmed().isEmpty()
                              ? QDir::homePath() + QStringLiteral("/.ssh")
                              : QFileInfo(m_keyPath->text().trimmed()).absolutePath();
    const QString path = QFileDialog::getOpenFileName(this, QStringLiteral("选择私钥文件"), start);
    if (!path.isEmpty()) {
        m_keyPath->setText(path);
        updateAutoLoginState();
    }
}

bool ConnectionDialog::validate()
{
    bool ok = true;
    m_host->setError(false);
    m_username->setError(false);
    m_port->setError(false);
    m_keyPath->setError(false);

    if (m_host->text().trimmed().isEmpty()) {
        m_host->setError(true, QStringLiteral("请输入主机地址"));
        ok = false;
    }
    if (m_username->text().trimmed().isEmpty()) {
        m_username->setError(true, QStringLiteral("请输入用户名"));
        ok = false;
    }
    bool portOk = false;
    const int port = m_port->text().toInt(&portOk);
    if (!portOk || port < 1 || port > 65535) {
        m_port->setError(true, QStringLiteral("端口无效"));
        ok = false;
    }
    if (m_auth->currentIndex() == 1) {
        const QString key = m_keyPath->text().trimmed();
        if (key.isEmpty()) {
            m_keyPath->setError(true, QStringLiteral("请选择私钥文件"));
            ok = false;
        } else if (!QFileInfo::exists(key)) {
            m_keyPath->setError(true, QStringLiteral("私钥文件不存在"));
            ok = false;
        }
    }

    if (!ok) {
        m_errorLabel->setText(QStringLiteral("请检查标红的字段"));
        m_errorLabel->show();
        return false;
    }
    m_errorLabel->hide();
    return true;
}

void ConnectionDialog::tryAccept(bool connectAfter)
{
    if (!validate())
        return;

    SessionProfile profile = m_profile;
    profile.name = m_name->text().trimmed();
    profile.host = m_host->text().trimmed();
    profile.username = m_username->text().trimmed();
    profile.port = static_cast<quint16>(m_port->text().toInt());
    profile.color = m_colorDots ? m_colorDots->color() : profile.color;
    if (profile.name.isEmpty())
        profile.name = QStringLiteral("%1@%2").arg(profile.username, profile.host);

    const bool keyAuth = m_auth->currentIndex() == 1;
    profile.auth = keyAuth ? AuthType::PublicKey : AuthType::Password;
    if (keyAuth) {
        profile.privateKeyPath = m_keyPath->text().trimmed();
        profile.setPassword(QString(), false);
        profile.setPassphrase(m_passphrase->text(), m_remember->isChecked());
    } else {
        profile.privateKeyPath.clear();
        profile.setPassphrase(QString(), false);
        profile.setPassword(m_password->text(), m_remember->isChecked());
    }
    profile.rememberPassword = m_remember->isChecked();

    m_save = (m_mode == Mode::Edit) || m_saveToLibrary->isChecked();
    profile.autoLogin = m_autoLogin->isChecked() && m_save;
    m_profile = profile;
    m_connectAfterSave = connectAfter;
    QDialog::accept();
}

} // namespace nyan::ui
