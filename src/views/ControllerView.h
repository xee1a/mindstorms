#pragma once

#include "drive/Gamepad.h"

#include <QList>
#include <QTimer>
#include <QWidget>
#include <functional>

class QComboBox;
class QGridLayout;
class QLabel;

namespace ui {
class PadView;
}

/**
 * Shows what the gamepad sends, live: a drawing of the pad in the middle and every input on both sides,
 * named as in the Gamepad blocks. It controls nothing; programs decide what each input does.
 */
class ControllerView : public QWidget {
    Q_OBJECT
public:
    explicit ControllerView(QWidget *parent = nullptr);

protected:
    void showEvent(QShowEvent *e) override;
    void hideEvent(QHideEvent *e) override;

private:
    /** One line in an input list: a chip with the input's name and its current value. */
    struct InputRow {
        /** The input's name in the Gamepad blocks. */
        QString name;
        QLabel *chip;
        QLabel *value;
        std::function<bool(const drive::PadSnapshot &)> active;
        std::function<QString(const drive::PadSnapshot &)> describe;
        bool lit = false;
        QString text;
    };

    void addInput(QGridLayout *grid, bool chipOnRight, const QString &name, std::function<bool(const drive::PadSnapshot &)> active,
                  std::function<QString(const drive::PadSnapshot &)> describe);
    void tick();
    void onPadConnection(bool connected);
    /** Draws the chosen pad and adds its own button names (✕, LB…) next to the block names. */
    void applyLayout();

    QTimer m_tick;
    QList<InputRow> m_rows;
    bool m_digitalShown = false;

    ui::PadView *m_pad = nullptr;
    QLabel *m_padName = nullptr;
    QLabel *m_digitalHint = nullptr;
    QComboBox *m_padPick = nullptr;
    QComboBox *m_layoutPick = nullptr;
};
