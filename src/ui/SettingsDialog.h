#pragma once

#include "core/AppSettings.h"
#include "ui/MaterialWidgets.h"

class QComboBox;
class QLabel;
class QSlider;
class QSpinBox;

namespace nyan::ui {

class SettingsDialog : public MDialog {
    Q_OBJECT
public:
    SettingsDialog(QWidget *parent, const AppSettings &current);

    AppSettings settings() const { return m_settings; }

private:
    AppSettings m_settings;
    MSegmentedControl *m_theme = nullptr;
    QSlider *m_opacity = nullptr;
    QLabel *m_opacityValue = nullptr;
    QLabel *m_backgroundPath = nullptr;
    QString m_backgroundImage;
    QComboBox *m_fontFamily = nullptr;
    QSpinBox *m_fontSize = nullptr;
    QSpinBox *m_scrollback = nullptr;
    MSwitch *m_copyOnSelect = nullptr;
    MSwitch *m_confirmDelete = nullptr;
    MSwitch *m_showHidden = nullptr;
    MSwitch *m_autoOpenPanel = nullptr;
    MSwitch *m_confirmExit = nullptr;
};

} // namespace nyan::ui
