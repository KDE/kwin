/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2013 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef KWIN_SCREENS_H
#define KWIN_SCREENS_H

// KWin includes
#include <kwinglobals.h>
// KDE includes
#include <KConfig>
#include <KSharedConfig>
// Qt includes
#include <QObject>
#include <QRect>
#include <QTimer>
#include <QVector>

namespace KWin
{
class Window;
class Output;
class Platform;

class KWIN_EXPORT Screens : public QObject
{
    Q_OBJECT

public:
    explicit Screens();

    void init();

    /**
     * The highest scale() of all connected screens
     * for use when deciding what scale to load global assets at
     * Similar to QGuiApplication::scale
     * @see scale
     */
    qreal maxScale() const;

    /**
     * The output scale for this display, for use by high DPI displays
     */
    qreal scale(int screen) const;

Q_SIGNALS:
    /**
     * Emitted whenever the screens are changed either count or geometry.
     */
    void changed();
    /**
     * Emitted when the maximum scale of all attached screens changes
     * @see maxScale
     */
    void maxScaleChanged();

private Q_SLOTS:
    void updateSize();

private:
    Output *findOutput(int screenId) const;

    qreal m_maxScale;
};
}

#endif // KWIN_SCREENS_H
