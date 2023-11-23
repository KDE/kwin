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
     * @param timestamp The X activation timestamp.
     */
    void start(quint32 timestamp = 0);
    /**
     * @brief Terminate the kill helper process.
     */
    void quit();

private:
    Window *m_window = nullptr;
    QProcess m_process;
};

} // namespace KWin
