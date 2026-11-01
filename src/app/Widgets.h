#pragma once

#include "drive/Gamepad.h"
#include "nxt/Brick.h"

#include <QAbstractButton>
#include <QFrame>
#include <QImage>
#include <QLabel>
#include <QPushButton>
#include <QTimer>

namespace ui {

QLabel *label(const QString &text, const char *role = nullptr, QWidget *parent = nullptr);
QFrame *card(QWidget *parent = nullptr);
QFrame *inset(QWidget *parent = nullptr);
/** kind: "primary", "ghost", "danger" or nullptr. icon: resource name without extension. */
QPushButton *button(const QString &text, const char *kind = nullptr, const char *icon = nullptr, QWidget *parent = nullptr);
/** Tinted SVG icon; an invalid colour means the theme's icon colour. */
QIcon icon(const char *name, const QColor &tint = QColor());

/** iOS-style on/off switch with a label. */
class ToggleSwitch : public QAbstractButton {
    Q_OBJECT
public:
    explicit ToggleSwitch(const QString &text, QWidget *parent = nullptr);
    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent *) override;
};

/** A battery that fills and changes colour with the charge: green, yellow, red. No numbers. */
class BatteryWidget : public QWidget {
    Q_OBJECT
public:
    explicit BatteryWidget(QWidget *parent = nullptr);
    void setLevel(nxt::BatteryLevel level);
    QSize sizeHint() const override { return {44, 22}; }
    /** The same drawing as a small icon, for buttons and chips. */
    static QIcon icon(nxt::BatteryLevel level);
    static QColor colour(nxt::BatteryLevel level);

protected:
    void paintEvent(QPaintEvent *) override;

private:
    nxt::BatteryLevel m_level = nxt::BatteryLevel::Full;
};

/** Simple drawing of the NXT brick; its screen shows the name and battery while connected. */
class BrickArt : public QWidget {
    Q_OBJECT
public:
    explicit BrickArt(QWidget *parent = nullptr);
    void setState(bool online, const QString &name, std::optional<nxt::BatteryLevel> battery);
    QSize sizeHint() const override { return {200, 314}; }

protected:
    void paintEvent(QPaintEvent *) override;

private:
    QImage m_image;
    bool m_online = false;
    QString m_name;
    std::optional<nxt::BatteryLevel> m_battery;
};

/** Drawing of a gamepad (the layout of common USB pads) with live highlights on pressed buttons and moved sticks. */
class PadView : public QWidget {
    Q_OBJECT
public:
    /** How the pad is drawn: a picture of the MANGAN 200, or a generic PlayStation or Xbox drawing. */
    enum class Layout { Mangan200, PlayStation, Xbox };

    explicit PadView(QWidget *parent = nullptr);
    void setOnline(bool on);
    void setLayout(Layout layout);
    void setState(const drive::ControlInput &input, const std::array<bool, int(drive::PadButton::Count)> &buttons);
    QSize sizeHint() const override { return {520, 412}; }

protected:
    void paintEvent(QPaintEvent *) override;

private:
    QSizeF artSize() const;
    void paintDrawn(QPainter &p) const;
    void paintHighlights(QPainter &p) const;

    Layout m_layout = Layout::Mangan200;
    QImage m_mangan;
    /** m_mangan smoothly scaled to the size it was last drawn at (a plain scaled draw looks jagged). */
    mutable QImage m_manganScaled;
    bool m_on = false;
    drive::ControlInput m_input;
    std::array<bool, int(drive::PadButton::Count)> m_buttons{};
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
