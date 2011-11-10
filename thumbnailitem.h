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

#include <QtCore/QWeakPointer>
#include <QtDeclarative/QDeclarativeItem>

namespace KWin
{

class EffectWindow;
class EffectWindowImpl;

class ThumbnailItem : public QDeclarativeItem
{
    Q_OBJECT
    Q_PROPERTY(qulonglong wId READ wId WRITE setWId NOTIFY wIdChanged SCRIPTABLE true)
public:
    ThumbnailItem(QDeclarativeItem *parent = 0);
    virtual ~ThumbnailItem();

    qulonglong wId() const {
        return m_wId;
    }
    void setWId(qulonglong wId);
    virtual void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget);
Q_SIGNALS:
    void wIdChanged(qulonglong wid);
private Q_SLOTS:
    void init();
    void effectWindowAdded();
private:
    void findParentEffectWindow();
    qulonglong m_wId;
    QWeakPointer<EffectWindowImpl> m_parent;
};

} // KWin

#endif // KWIN_THUMBNAILITEM_H
