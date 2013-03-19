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

class EffectWindow;
class EffectWindowImpl;

class ThumbnailItem : public QDeclarativeItem
{
    Q_OBJECT
    Q_PROPERTY(qulonglong wId READ wId WRITE setWId NOTIFY wIdChanged SCRIPTABLE true)
    Q_PROPERTY(bool clip READ isClip WRITE setClip NOTIFY clipChanged SCRIPTABLE true)
    Q_PROPERTY(qulonglong parentWindow READ parentWindow WRITE setParentWindow)
    Q_PROPERTY(qreal brightness READ brightness WRITE setBrightness NOTIFY brightnessChanged)
    Q_PROPERTY(qreal saturation READ saturation WRITE setSaturation NOTIFY saturationChanged)
public:
    explicit ThumbnailItem(QDeclarativeItem *parent = 0);
    virtual ~ThumbnailItem();

    qulonglong wId() const {
        return m_wId;
    }
    void setWId(qulonglong wId);
    bool isClip() const {
        return m_clip;
    }
    void setClip(bool clip);
    virtual void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget);
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
    void wIdChanged(qulonglong wid);
    void clipChanged(bool clipped);
    void brightnessChanged();
    void saturationChanged();
private Q_SLOTS:
    void init();
    void effectWindowAdded();
    void repaint(KWin::EffectWindow* w);
    void compositingToggled();
private:
    void findParentEffectWindow();
    qulonglong m_wId;
    bool m_clip;
    QWeakPointer<EffectWindowImpl> m_parent;
    qulonglong m_parentWindow;
    qreal m_brightness;
    qreal m_saturation;
};

inline
qreal ThumbnailItem::brightness() const
{
    return m_brightness;
}

inline
qreal ThumbnailItem::saturation() const
{
    return m_saturation;
}

} // KWin

#endif // KWIN_THUMBNAILITEM_H
