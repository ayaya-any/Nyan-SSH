#pragma once

#include "core/SessionProfile.h"

#include <QWidget>
#include <QVector>

class QGridLayout;
class QLabel;

namespace nyan::ui {

class MTextField;

class WelcomePage : public QWidget {
    Q_OBJECT
public:
    explicit WelcomePage(QWidget *parent = nullptr);

    void setProfiles(const QVector<SessionProfile> &profiles);
    void setStats(int savedSessions, int trustedHosts);
    void focusQuickConnect();

signals:
    void newConnectionRequested();
    void connectRequested(const QString &id);
    void quickConnectRequested(const SessionProfile &profile);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    void rebuildRecent();
    void submitQuickConnect();

    QVector<SessionProfile> m_profiles;
    MTextField *m_host = nullptr;
    MTextField *m_port = nullptr;
    MTextField *m_username = nullptr;
    MTextField *m_password = nullptr;
    QLabel *m_errorLabel = nullptr;
    QGridLayout *m_recentGrid = nullptr;
    QWidget *m_recentContainer = nullptr;
    QLabel *m_recentEmpty = nullptr;
    QLabel *m_statsLabel = nullptr;
};

} // namespace nyan::ui
