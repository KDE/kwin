/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2006 Lubos Lunak <l.lunak@kde.org>

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

#ifndef KWIN_DELETED_H
#define KWIN_DELETED_H

#include "toplevel.h"

namespace KWin
{

class Deleted
    : public Toplevel
{
    Q_OBJECT
    Q_PROPERTY(bool minimized READ isMinimized)
public:
    static Deleted* create(Toplevel* c);
    // used by effects to keep the window around for e.g. fadeout effects when it's destroyed
    void refWindow();
    void unrefWindow(bool delay = false);
    void discard(allowed_t);
    virtual int desktop() const;
    virtual QStringList activities() const;
    virtual QPoint clientPos() const;
    virtual QSize clientSize() const;
    virtual QRect transparentRect() const;
    virtual bool isDeleted() const;
    const QPixmap *topDecoPixmap() const {
        return &decorationPixmapTop;
    }
    const QPixmap *leftDecoPixmap() const {
        return &decorationPixmapLeft;
    }
    const QPixmap *bottomDecoPixmap() const {
        return &decorationPixmapBottom;
    }
    const QPixmap *rightDecoPixmap() const {
        return &decorationPixmapRight;
    }
    bool noBorder() const {
        return no_border;
    }
    void layoutDecorationRects(QRect &left, QRect &top, QRect &right, QRect &bottom, int unused = 0) const;
    QRect decorationRect() const;
    virtual Layer layer() const {
        return m_layer;
    }
    bool isMinimized() const {
        return m_minimized;
    }
    NET::WindowType windowType(bool direct = false, int supported_types = 0) const;
    bool decorationPixmapRequiresRepaint() const {
        return false;
    }
    void ensureDecorationPixmapsPainted() {}
protected:
    virtual void debug(QDebug& stream) const;
    virtual bool shouldUnredirect() const;
private:
    Deleted(Workspace *ws);   // use create()
    void copyToDeleted(Toplevel* c);
    virtual ~Deleted(); // deleted only using unrefWindow()
    int delete_refcount;
    double window_opacity;
    int desk;
    QStringList activityList;
    QRect contentsRect; // for clientPos()/clientSize()
    QRect transparent_rect;

    QPixmap decorationPixmapLeft;
    QPixmap decorationPixmapRight;
    QPixmap decorationPixmapTop;
    QPixmap decorationPixmapBottom;
    bool no_border;
    QRect decoration_left;
    QRect decoration_right;
    QRect decoration_top;
    QRect decoration_bottom;
    int padding_left, padding_top, padding_right, padding_bottom;
    Layer m_layer;
    bool m_minimized;
};

inline void Deleted::refWindow()
{
    ++delete_refcount;
}

} // namespace

#endif
