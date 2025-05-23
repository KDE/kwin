/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2022 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "utils/common.h"

#include <QHash>
#include <QList>
#include <QRect>
#include <QString>
#include <QUuid>

namespace KWin
{

class Window;
class Workspace;

class PlacementTracker : public QObject
{
    Q_OBJECT
public:
    PlacementTracker(Workspace *workspace);

    void add(Window *window);
    void remove(Window *window);

    void restore(const QString &key);
    void init(const QString &key);

    void inhibit();
    void uninhibit();

private:
    struct WindowData
    {
        QString outputUuid;
        QRectF geometry;
        MaximizeMode maximize;
        QuickTileMode quickTile;
        QRectF geometryRestore;
        bool fullscreen;
        QRectF fullscreenGeometryRestore;
        uint32_t interactiveMoveResizeCount;
    };

    void saveGeometry(Window *window);
    void saveInteractionCounter(Window *window);
    void saveMaximize(Window *window);
    void saveQuickTile(Window *window);
    void saveFullscreen(Window *window);
    void saveMaximizeGeometryRestore(Window *window);
    void saveFullscreenGeometryRestore(Window *window);
    WindowData dataForWindow(Window *window) const;

    QList<Window *> m_savedWindows;
    QHash<QString, QHash<Window *, WindowData>> m_data;
    QHash<Window *, WindowData> m_lastRestoreData;
    QString m_currentKey;
    int m_inhibitCount = 0;
    Workspace *const m_workspace;
};

}
