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
class AbstractClient;
class AbstractOutput;
class Platform;

class KWIN_EXPORT Screens : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int count READ count WRITE setCount NOTIFY countChanged)

public:
    ~Screens() override;
    int count() const;
    QRect geometry(int screen) const;
    /**
     * The bounding geometry of all screens combined. Overlapping areas
     * are not counted multiple times.
     * @see geometryChanged()
     */
    QRect geometry() const;

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
    /**
     * The bounding size of all screens combined. Overlapping areas
     * are not counted multiple times.
     *
     * @see geometry()
     * @see sizeChanged()
     */
    QSize size() const;
    int number(const QPoint &pos) const;

    int intersecting(const QRect &r) const;

Q_SIGNALS:
    void countChanged(int previousCount, int newCount);
    /**
     * Emitted whenever the screens are changed either count or geometry.
     */
    void changed();
    /**
     * Emitted when the geometry of all screens combined changes.
     * Not emitted when the geometry of an individual screen changes.
     * @see geometry()
     */
    void geometryChanged();
    /**
     * Emitted when the size of all screens combined changes.
     * Not emitted when the size of an individual screen changes.
     * @see size()
     */
    void sizeChanged();
    /**
     * Emitted when the maximum scale of all attached screens changes
     * @see maxScale
     */
    void maxScaleChanged();

protected Q_SLOTS:
    void setCount(int count);
    void updateCount();

protected:
    /**
     * Called once the singleton instance has been created.
     * Any initialization code should go into this method. Overriding classes have to call
     * the base implementation first.
     */
    void init();

private Q_SLOTS:
    void updateSize();

private:
    AbstractOutput *findOutput(int screenId) const;

    int m_count;
    QSize m_boundingSize;
    qreal m_maxScale;

    KWIN_SINGLETON(Screens)
};

inline
int Screens::count() const
{
    return m_count;
}

inline
QSize Screens::size() const
{
    return m_boundingSize;
}

inline
QRect Screens::geometry() const
{
    return QRect(QPoint(0,0), size());
}

inline
Screens *screens()
{
    return Screens::self();
}

}

#endif // KWIN_SCREENS_H
