#pragma once

#include "drive/Gamepad.h"
#include "drive/Mixer.h"
#include "nxt/Brick.h"

#include <QList>
#include <QSet>
#include <QTimer>
#include <QWidget>
#include <functional>

class QComboBox;
class QGridLayout;
class QLabel;
class QPushButton;

namespace ui {
class PadView;
}

/**
 * Live driving: a photo of the controller in the middle, and every input with
 * what it does on both sides. Drive setup itself lives in Settings → Drive.
 */
class DriveView : public QWidget {
    Q_OBJECT
public:
    explicit DriveView(QWidget *parent = nullptr);

signals:
    void message(const QString &text, const char *kind);

protected:
    void showEvent(QShowEvent *e) override;
    void hideEvent(QHideEvent *e) override;
    bool eventFilter(QObject *o, QEvent *e) override;

private:
    /** One line in an input list: a chip naming the input and what it does. */
    struct InputRow {
        QLabel *chip;
        QLabel *action;
        std::function<bool(const drive::PadSnapshot &)> active;
        std::function<QString()> describe;
        bool lit = false;
    };

    void addInput(QGridLayout *grid, bool chipOnRight, const QString &name, std::function<bool(const drive::PadSnapshot &)> active,
                  std::function<QString()> describe);
    void refreshDescriptions();
    void setConfig(const drive::DriveConfig &cfg);
    void setArmed(bool on);
    void emergencyStop();
    void tick();
    void onPadButton(drive::PadButton b);
    void onPadConnection(bool connected);
    void onBrick(const nxt::BrickState &s);
    void announce(const QString &text);
    QString motorName(std::optional<int> port) const;
    drive::ControlInput keyboardInput() const;

    drive::DriveConfig m_cfg;
    bool m_armed = false;
    /** Whether the pad label currently shows the digital-mode hint. */
    bool m_digitalShown = false;
    bool m_precision = false;
    QSet<int> m_keys;
    QTimer m_tick;
    QList<InputRow> m_rows;

    ui::PadView *m_pad = nullptr;
    QLabel *m_padName = nullptr;
    QComboBox *m_padPick = nullptr;
    QLabel *m_brickNote = nullptr;
    QLabel *m_modeNote = nullptr;
    QPushButton *m_arm = nullptr;
};
