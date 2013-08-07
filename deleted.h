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
class PaintRedirector;

class Deleted
    : public Toplevel
{
    Q_OBJECT
    Q_PROPERTY(bool minimized READ isMinimized)
    Q_PROPERTY(bool modal READ isModal)
public:
    static Deleted* create(Toplevel* c);
    // used by effects to keep the window around for e.g. fadeout effects when it's destroyed
    void refWindow();
    void unrefWindow();
    void discard();
    virtual int desktop() const;
    virtual QStringList activities() const;
    virtual QPoint clientPos() const;
    virtual QSize clientSize() const;
    virtual QRect transparentRect() const;
    virtual bool isDeleted() const;
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
    bool isModal() const {
        return m_modal;
    }
    ClientList mainClients() const {
        return m_mainClients;
    }
    NET::WindowType windowType(bool direct = false, int supported_types = 0) const;
    PaintRedirector *decorationPaintRedirector() {
        return m_paintRedirector;
    }
    bool wasClient() const {
        return m_wasClient;
    }
protected:
    virtual void debug(QDebug& stream) const;
    virtual bool shouldUnredirect() const;
private Q_SLOTS:
    void mainClientClosed(KWin::Toplevel *client);
private:
    Deleted();   // use create()
    void copyToDeleted(Toplevel* c);
    virtual ~Deleted(); // deleted only using unrefWindow()
    int delete_refcount;
    double window_opacity;
    int desk;
    QStringList activityList;
    QRect contentsRect; // for clientPos()/clientSize()
    QRect transparent_rect;

    bool no_border;
    QRect decoration_left;
    QRect decoration_right;
    QRect decoration_top;
    QRect decoration_bottom;
    int padding_left, padding_top, padding_right, padding_bottom;
    Layer m_layer;
    bool m_minimized;
    bool m_modal;
    ClientList m_mainClients;
    PaintRedirector *m_paintRedirector;
    bool m_wasClient;
};

inline void Deleted::refWindow()
{
    ++delete_refcount;
}

} // namespace

#endif
