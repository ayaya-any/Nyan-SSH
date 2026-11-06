#pragma once

#include "core/SessionProfile.h"

#include <QWidget>
#include <QVector>

class QLabel;
class QVBoxLayout;

namespace nyan::ui {

class MTextField;

class Sidebar : public QWidget {
    Q_OBJECT
public:
    explicit Sidebar(QWidget *parent = nullptr);

    void setProfiles(const QVector<SessionProfile> &profiles);
    void setActiveSessionCount(int count);
    void setFooterText(const QString &text);

signals:
    void newConnectionRequested();
    void connectRequested(const QString &id);
    void editRequested(const QString &id);
    void duplicateRequested(const QString &id);
    void deleteRequested(const QString &id);
    void settingsRequested();

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    void rebuild();
    void applyFilter();

    QVector<SessionProfile> m_profiles;
    MTextField *m_search = nullptr;
    QVBoxLayout *m_listLayout = nullptr;
    QWidget *m_listContainer = nullptr;
    QLabel *m_emptyLabel = nullptr;
    QLabel *m_countLabel = nullptr;
    QLabel *m_footerLabel = nullptr;
    QLabel *m_footerDot = nullptr;
    int m_activeCount = 0;
};

} // namespace nyan::ui
