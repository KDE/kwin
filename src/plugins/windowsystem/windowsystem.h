/*
    SPDX-FileCopyrightText: 2019 Martin Flöser <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/
#pragma once

#include <private/kwindowsystem_p.h>

#include <QObject>

namespace KWin
{

class WindowSystem : public QObject, public KWindowSystemPrivateV2
{
    Q_OBJECT
public:
    WindowSystem();
    void activateWindow(QWindow *win, long time) override;
    bool showingDesktop() override;
    void setShowingDesktop(bool showing) override;
    void requestToken(QWindow *win, uint32_t serial, const QString &app_id) override;
    void setCurrentToken(const QString &token) override;
    quint32 lastInputSerial(QWindow *window) override;
    void exportWindow(QWindow *window) override;
    void unexportWindow(QWindow *window) override;
    void setMainWindow(QWindow *window, const QString &handle) override;
};

}
