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
class Platform;

class KWIN_EXPORT Screens : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int count READ count WRITE setCount NOTIFY countChanged)
    Q_PROPERTY(int current READ current WRITE setCurrent NOTIFY currentChanged)
    Q_PROPERTY(bool currentFollowsMouse READ isCurrentFollowsMouse WRITE setCurrentFollowsMouse)

public:
    ~Screens() override;
    /**
     * @internal
     */
    void setConfig(KSharedConfig::Ptr config);
    int count() const;
    int current() const;
    void setCurrent(int current);
    /**
     * Called e.g. when a user clicks on a window, set current screen to be the screen
     * where the click occurred
     */
    void setCurrent(const QPoint &pos);
    /**
     * Check whether a client moved completely out of what's considered the current screen,
     * if yes, set a new active screen.
     */
    void setCurrent(const AbstractClient *c);
    bool isCurrentFollowsMouse() const;
    void setCurrentFollowsMouse(bool follows);
    virtual QRect geometry(int screen) const = 0;
    /**
     * The bounding geometry of all screens combined. Overlapping areas
     * are not counted multiple times.
     * @see geometryChanged()
     */
    QRect geometry() const;
    /**
     * The output name of the screen (usually eg. LVDS-1, VGA-0 or DVI-I-1 etc.)
     */
    virtual QString name(int screen) const;
    /**
     * @returns current refreshrate of the @p screen.
     */
    virtual float refreshRate(int screen) const;
    /**
     * @returns size of the @p screen.
     *
     * To get the size of all screens combined use size().
     * @see size()
     */
    virtual QSize size(int screen) const = 0;

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
    virtual qreal scale(int screen) const;
    /**
     * The bounding size of all screens combined. Overlapping areas
     * are not counted multiple times.
     *
     * @see geometry()
     * @see sizeChanged()
     */
    QSize size() const;
    virtual int number(const QPoint &pos) const = 0;

    inline bool isChanging() { return m_changedTimer->isActive(); }

    int intersecting(const QRect &r) const;

    /**
     * The virtual bounding size of all screens combined.
     * The default implementation returns the same as @ref size and that is the
     * method which should be preferred.
     *
     * This method is only for cases where the platform specific implementation needs
     * to support different virtual sizes like on X11 with XRandR panning.
     *
     * @see size
     */
    virtual QSize displaySize() const;


    /**
     * The physical size of @p screen in mm.
     * Default implementation returns a size derived from 96 DPI.
     */
    virtual QSizeF physicalSize(int screen) const;

    /**
     * @returns @c true if the @p screen is connected through an internal display (e.g. LVDS).
     * Default implementation returns @c false.
     */
    virtual bool isInternal(int screen) const;

    /**
     * @returns @c true if the @p screen can be rotated.
     * Default implementation returns @c false
     */
    virtual bool supportsTransformations(int screen) const;

    virtual Qt::ScreenOrientation orientation(int screen) const;

    int physicalDpiX(int screen) const;
    int physicalDpiY(int screen) const;

public Q_SLOTS:
    void reconfigure();

Q_SIGNALS:
    void countChanged(int previousCount, int newCount);
    /**
     * Emitted whenever the screens are changed either count or geometry.
     */
    void changed();
    void currentChanged();
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
    void startChangedTimer();
    virtual void updateCount() = 0;

protected:
    /**
     * Called once the singleton instance has been created.
     * Any initialization code should go into this method. Overriding classes have to call
     * the base implementation first.
     */
    virtual void init();

private Q_SLOTS:
    void updateSize();

private:
    int m_count;
    int m_current;
    bool m_currentFollowsMouse;
    QTimer *m_changedTimer;
    KSharedConfig::Ptr m_config;
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
bool Screens::isCurrentFollowsMouse() const
{
    return m_currentFollowsMouse;
}

inline
void Screens::startChangedTimer()
{
    m_changedTimer->start();
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
