/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2011 Martin Gräßlin <mgraesslin@kde.org>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/

#ifndef KWIN_THUMBNAILITEM_H
#define KWIN_THUMBNAILITEM_H

#include <QPointer>
#include <QWeakPointer>
#include <QQuickPaintedItem>

namespace KWin
{

class AbstractClient;
class EffectWindow;
class EffectWindowImpl;

class AbstractThumbnailItem : public QQuickPaintedItem
{
    Q_OBJECT
    Q_PROPERTY(qreal brightness READ brightness WRITE setBrightness NOTIFY brightnessChanged)
    Q_PROPERTY(qreal saturation READ saturation WRITE setSaturation NOTIFY saturationChanged)
    Q_PROPERTY(QQuickItem *clipTo READ clipTo WRITE setClipTo NOTIFY clipToChanged)
public:
    virtual ~AbstractThumbnailItem();
    qreal brightness() const;
    qreal saturation() const;
    QQuickItem *clipTo() const;

public Q_SLOTS:
    void setBrightness(qreal brightness);
    void setSaturation(qreal saturation);
    void setClipTo(QQuickItem *clip);

Q_SIGNALS:
    void brightnessChanged();
    void saturationChanged();
    void clipToChanged();

protected:
    explicit AbstractThumbnailItem(QQuickItem *parent = 0);

protected Q_SLOTS:
    virtual void repaint(KWin::EffectWindow* w) = 0;

private Q_SLOTS:
    void init();
    void effectWindowAdded();
    void compositingToggled();

private:
    void findParentEffectWindow();
    QWeakPointer<EffectWindowImpl> m_parent;
    qreal m_brightness;
    qreal m_saturation;
    QPointer<QQuickItem> m_clipToItem;
};

class WindowThumbnailItem : public AbstractThumbnailItem
{
    Q_OBJECT
    Q_PROPERTY(qulonglong wId READ wId WRITE setWId NOTIFY wIdChanged SCRIPTABLE true)
    Q_PROPERTY(KWin::AbstractClient *client READ client WRITE setClient NOTIFY clientChanged)
public:
    explicit WindowThumbnailItem(QQuickItem *parent = 0);
    virtual ~WindowThumbnailItem();

    qulonglong wId() const {
        return m_wId;
    }
    void setWId(qulonglong wId);
    AbstractClient *client() const;
    void setClient(AbstractClient *client);
    void paint(QPainter *painter) Q_DECL_OVERRIDE;
Q_SIGNALS:
    void wIdChanged(qulonglong wid);
    void clientChanged();
protected Q_SLOTS:
    void repaint(KWin::EffectWindow* w) Q_DECL_OVERRIDE;
private:
    qulonglong m_wId;
    AbstractClient *m_client;
};

class DesktopThumbnailItem : public AbstractThumbnailItem
{
    Q_OBJECT
    Q_PROPERTY(int desktop READ desktop WRITE setDesktop NOTIFY desktopChanged)
public:
    DesktopThumbnailItem(QQuickItem *parent = 0);
    virtual ~DesktopThumbnailItem();

    int desktop() const {
        return m_desktop;
    }
    void setDesktop(int desktop);
    void paint(QPainter *painter) Q_DECL_OVERRIDE;
Q_SIGNALS:
    void desktopChanged(int desktop);
protected Q_SLOTS:
    void repaint(KWin::EffectWindow* w) Q_DECL_OVERRIDE;
private:
    int m_desktop;
};

inline
qreal AbstractThumbnailItem::brightness() const
{
    return m_brightness;
}

inline
qreal AbstractThumbnailItem::saturation() const
{
    return m_saturation;
}

inline
QQuickItem* AbstractThumbnailItem::clipTo() const
{
    return m_clipToItem.data();
}

inline
AbstractClient *WindowThumbnailItem::client() const
{
    return m_client;
}

} // KWin

#endif // KWIN_THUMBNAILITEM_H
