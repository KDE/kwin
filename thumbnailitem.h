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

#include <QWeakPointer>
#include <QtDeclarative/QDeclarativeItem>

namespace KWin
{

class Client;
class EffectWindow;
class EffectWindowImpl;

class AbstractThumbnailItem : public QDeclarativeItem
{
    Q_OBJECT
    Q_PROPERTY(bool clip READ isClip WRITE setClip NOTIFY clipChanged SCRIPTABLE true)
    Q_PROPERTY(qulonglong parentWindow READ parentWindow WRITE setParentWindow)
    Q_PROPERTY(qreal brightness READ brightness WRITE setBrightness NOTIFY brightnessChanged)
    Q_PROPERTY(qreal saturation READ saturation WRITE setSaturation NOTIFY saturationChanged)
public:
    virtual ~AbstractThumbnailItem();
    bool isClip() const {
        return m_clip;
    }
    void setClip(bool clip);
    qulonglong parentWindow() const {
        return m_parentWindow;
    }
    void setParentWindow(qulonglong parentWindow);
    qreal brightness() const;
    qreal saturation() const;

public Q_SLOTS:
    void setBrightness(qreal brightness);
    void setSaturation(qreal saturation);

Q_SIGNALS:
    void clipChanged(bool clipped);
    void brightnessChanged();
    void saturationChanged();

protected:
    explicit AbstractThumbnailItem(QDeclarativeItem *parent = 0);

protected Q_SLOTS:
    virtual void repaint(KWin::EffectWindow* w) = 0;

private Q_SLOTS:
    void init();
    void effectWindowAdded();
    void compositingToggled();

private:
    void findParentEffectWindow();
    bool m_clip;
    QWeakPointer<EffectWindowImpl> m_parent;
    qulonglong m_parentWindow;
    qreal m_brightness;
    qreal m_saturation;
};

class WindowThumbnailItem : public AbstractThumbnailItem
{
    Q_OBJECT
    Q_PROPERTY(qulonglong wId READ wId WRITE setWId NOTIFY wIdChanged SCRIPTABLE true)
    Q_PROPERTY(KWin::Client *client READ client WRITE setClient NOTIFY clientChanged)
public:
    explicit WindowThumbnailItem(QDeclarativeItem *parent = 0);
    virtual ~WindowThumbnailItem();

    qulonglong wId() const {
        return m_wId;
    }
    void setWId(qulonglong wId);
    Client *client() const;
    void setClient(Client *client);
    virtual void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget);
Q_SIGNALS:
    void wIdChanged(qulonglong wid);
    void clientChanged();
protected Q_SLOTS:
    virtual void repaint(KWin::EffectWindow* w);
private:
    qulonglong m_wId;
    Client *m_client;
};

class DesktopThumbnailItem : public AbstractThumbnailItem
{
    Q_OBJECT
    Q_PROPERTY(int desktop READ desktop WRITE setDesktop NOTIFY desktopChanged)
public:
    DesktopThumbnailItem(QDeclarativeItem *parent = 0);
    virtual ~DesktopThumbnailItem();

    int desktop() const {
        return m_desktop;
    }
    void setDesktop(int desktop);
    virtual void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget);
Q_SIGNALS:
    void desktopChanged(int desktop);
protected Q_SLOTS:
    virtual void repaint(KWin::EffectWindow* w);
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
Client *WindowThumbnailItem::client() const
{
    return m_client;
}

} // KWin

#endif // KWIN_THUMBNAILITEM_H
