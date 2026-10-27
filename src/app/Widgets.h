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

/** A transparent photo scaled to fit, greyed out and faded while "offline". */
class PhotoWidget : public QWidget {
    Q_OBJECT
public:
    PhotoWidget(const QString &resource, QSize hint, QWidget *parent = nullptr);
    void setOnline(bool on);
    bool online() const { return m_on; }
    QSize sizeHint() const override { return m_hint; }

protected:
    void paintEvent(QPaintEvent *) override;
    /** Drawn over the photo; `photo` is where the photo landed, in widget coordinates. */
    virtual void paintOverlay(QPainter &, const QRectF &) {}

private:
    QImage m_photo;
    QImage m_dimmed;
    QImage m_scaled;
    QSize m_scaledFor;
    QSize m_hint;
    bool m_scaledOn = false;
    bool m_on = false;
};

/** Photo of the user's gamepad with live highlights on the pressed buttons and moved sticks. */
class PadView : public PhotoWidget {
    Q_OBJECT
public:
    explicit PadView(QWidget *parent = nullptr);
    void setState(const drive::ControlInput &input, const std::array<bool, int(drive::PadButton::Count)> &buttons);

protected:
    void paintOverlay(QPainter &p, const QRectF &photo) override;

private:
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
