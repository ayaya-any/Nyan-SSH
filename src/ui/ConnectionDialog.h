#pragma once

#include "core/SessionProfile.h"
#include "ui/MaterialWidgets.h"

#include <QDateTime>

namespace nyan::ui {

class ConnectionDialog : public MDialog {
    Q_OBJECT
public:
    enum class Mode {
        Create,
        Edit,
    };

    ConnectionDialog(QWidget *parent, Mode mode, const SessionProfile *existing = nullptr);

    SessionProfile profile() const { return m_profile; }
    bool saveRequested() const { return m_save; }
    bool connectAfterSave() const { return m_connectAfterSave; }

private:
    void populate();
    void updateAuthSection();
    void updateAutoLoginState();
    void browseForKey();
    void tryAccept(bool connectAfter);
    bool validate();

    Mode m_mode = Mode::Create;
    SessionProfile m_profile;

    MTextField *m_name = nullptr;
    MTextField *m_username = nullptr;
    MTextField *m_host = nullptr;
    MTextField *m_port = nullptr;
    MTextField *m_password = nullptr;
    MTextField *m_keyPath = nullptr;
    MTextField *m_passphrase = nullptr;
    MButton *m_browseKey = nullptr;
    MSegmentedControl *m_auth = nullptr;
    MSwitch *m_remember = nullptr;
    MSwitch *m_autoLogin = nullptr;
    MSwitch *m_saveToLibrary = nullptr;
    QWidget *m_passwordSection = nullptr;
    QWidget *m_keySection = nullptr;
    QLabel *m_errorLabel = nullptr;
    QWidget *m_saveRow = nullptr;
    QWidget *m_autoLoginRow = nullptr;
    class ColorDots *m_colorDots = nullptr;

    bool m_save = true;
    bool m_connectAfterSave = false;
};

} // namespace nyan::ui
