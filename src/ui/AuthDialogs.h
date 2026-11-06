#pragma once

#include "ui/MaterialWidgets.h"

namespace nyan::ui {

class HostKeyDialog : public MDialog {
    Q_OBJECT
public:
    HostKeyDialog(QWidget *parent, const QString &host, quint16 port, const QString &keyType,
                  const QString &fingerprint, const QString &expectedFingerprint);

    bool rememberKey() const;

private:
    MSwitch *m_remember = nullptr;
};

class PasswordDialog : public MDialog {
    Q_OBJECT
public:
    enum class Mode {
        Password,
        Passphrase,
    };

    PasswordDialog(QWidget *parent, Mode mode, const QString &target, const QString &error = QString(),
                   bool rememberDefault = true);

    QString secret() const;
    bool remember() const;

private:
    MTextField *m_field = nullptr;
    MSwitch *m_remember = nullptr;
    Mode m_mode = Mode::Password;
};

} // namespace nyan::ui
