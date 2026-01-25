/*
 * SPDX-FileCopyrightText: 2026 Kai Uwe Broulik <kde@broulik.de>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <QObject>
#include <QProcess>

namespace KWin
{

class Window;

class UserActionPrompt : public QObject
{
    Q_OBJECT

public:
    enum class Prompt {
        NoBorder,
        FullScreen,
    };
    Q_ENUM(Prompt)

    /**
     * @brief Creates a user action prompt helper process.
     * @param window The window to kill, must be an X11Window or XdgToplevelWindow.
     */
    explicit UserActionPrompt(Window *window, Prompt prompt, QObject *parent = nullptr);

    ~UserActionPrompt() override;

    Prompt prompt() const;

    /**
     * @brief Whether the helper process is currently running.
     */
    bool isRunning() const;

    /**
     * @brief Starts the user action prompt helper process.
     */
    void start();
    /**
     * @brief Terminate the helper process.
     */
    void quit();

Q_SIGNALS:
    /**
     * @brief Emitted when the user requested the action be undone
     *
     * For example, in the NoBorder prompt, this should restore the window border.
     */
    void undoRequested();
    /**
     * @brief Emitted when the dialog finishes
     */
    void finished();

private:
    void onProcessFinished(int exitCode, QProcess::ExitStatus status);

    Window *m_window;
    QProcess m_process;
    Prompt m_prompt;
};

} // namespace KWin
