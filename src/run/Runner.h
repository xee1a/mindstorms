#pragma once

#include "nxt/Brick.h"

#include <QJsonObject>
#include <QObject>
#include <memory>

namespace run {

struct Context;

/**
 * Runs a block program from the PC: each stack runs on its own thread and turns blocks
 * into direct commands sent live over USB, so the cable has to stay connected.
 * (Downloading a compiled program to the brick is milestone 2.)
 */
class Runner : public QObject {
    Q_OBJECT
public:
    explicit Runner(QObject *parent = nullptr);
    ~Runner() override;

    bool running() const { return m_ctx != nullptr; }
    /** Starts the program in `workspace` (Workspace::toJson). Returns an error, or empty on success. */
    QString start(const QJsonObject &workspace);
    /** Stops every stack and the motors. */
    void stop();

signals:
    void runningChanged(bool running);
    /** The program ended by itself, was stopped, or failed (`error` is then non-empty). */
    void finished(const QString &error);
    /** Text a display block would have written on the NXT screen. */
    void screenText(const QString &text);

private:
    void onBrick(const nxt::BrickState &s);
    void finish();

    std::shared_ptr<Context> m_ctx;
};

} // namespace run
