#pragma once

#include "drive/Gamepad.h"

#include <QAbstractButton>
#include <QFrame>
#include <QLabel>
#include <QPushButton>
#include <QTimer>

namespace ui {

QLabel *label(const QString &text, const char *role = nullptr, QWidget *parent = nullptr);
QFrame *card(QWidget *parent = nullptr);
QFrame *inset(QWidget *parent = nullptr);
/** kind: "primary", "ghost", "danger" or nullptr. icon: resource name without extension. */
QPushButton *button(const QString &text, const char *kind = nullptr, const char *icon = nullptr, QWidget *parent = nullptr);
QIcon icon(const char *name, const QColor &color = QColor(0xc9, 0xd4, 0xe3));

/** iOS-style on/off switch with a label. */
class ToggleSwitch : public QAbstractButton {
    Q_OBJECT
public:
    explicit ToggleSwitch(const QString &text, QWidget *parent = nullptr);
    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent *) override;
};

/** Round power dial for one motor: -100..100. */
class MotorGauge : public QWidget {
    Q_OBJECT
public:
    explicit MotorGauge(QWidget *parent = nullptr);
    void setPower(int power);
    void setCaption(const QString &caption);
    QSize sizeHint() const override { return {140, 128}; }

protected:
    void paintEvent(QPaintEvent *) override;

private:
    int m_power = 0;
    QString m_caption;
};

/** Live drawing of an Xbox-style controller. */
class ControllerWidget : public QWidget {
    Q_OBJECT
public:
    explicit ControllerWidget(QWidget *parent = nullptr);
    void setState(const drive::ControlInput &input, const std::array<bool, int(drive::PadButton::Count)> &buttons, bool live);
    QSize sizeHint() const override { return {340, 210}; }
    bool hasHeightForWidth() const override { return true; }
    int heightForWidth(int w) const override { return w * 210 / 340; }

protected:
    void paintEvent(QPaintEvent *) override;

private:
    drive::ControlInput m_input;
    std::array<bool, int(drive::PadButton::Count)> m_buttons{};
    bool m_live = false;
};

class BatteryWidget : public QWidget {
    Q_OBJECT
public:
    explicit BatteryWidget(QWidget *parent = nullptr);
    void setLevel(int percent);
    QSize sizeHint() const override { return {72, 24}; }

protected:
    void paintEvent(QPaintEvent *) override;

private:
    int m_percent = 0;
};

/** Photo of the NXT brick; dimmed and greyed out while no brick is connected. */
class BrickIllustration : public QWidget {
    Q_OBJECT
public:
    explicit BrickIllustration(QWidget *parent = nullptr);
    void setOnline(bool on);
    QSize sizeHint() const override { return {300, 250}; }

protected:
    void paintEvent(QPaintEvent *) override;

private:
    QImage m_photo;
    QImage m_dimmed;
    QImage m_scaled;
    QSize m_scaledFor;
    bool m_scaledOn = false;
    bool m_on = false;
};

/** A message that slides in at the bottom of its parent and fades away. */
class Toast : public QLabel {
    Q_OBJECT
public:
    explicit Toast(QWidget *parent);
    void showMessage(const QString &message, const char *kind = "info");

protected:
    bool eventFilter(QObject *o, QEvent *e) override;

private:
    void reposition();
    QTimer m_hide;
};

} // namespace ui
