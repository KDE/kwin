/*
 * SPDX-FileCopyrightText: 2023 Kai Uwe Broulik <kde@broulik.de>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <QProcess>

namespace KWin
{

class Window;

class KillPrompt
{
public:
    /**
     * @brief Creates a kill helper process.
     * @param window The window to kill, must be an X11Window or XdgToplevelWindow.
     */
    explicit KillPrompt(Window *window);

    /**
     * @brief Whether the kill helper process is currently running.
     */
    bool isRunning() const;

    /**
     * @brief Starts the kill helper process.
     * @param token The XDG activation token or X activation timestamp.
     */
    void start(const QString &token);
    /**
     * @brief Terminate the kill helper process.
     */
    void quit();

    /**
     * @brief The application ID of the kill helper process.
     */
    static QString appId();

private:
    Window *m_window = nullptr;
    QProcess m_process;
};

} // namespace KWin
