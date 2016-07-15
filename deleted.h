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

class AbstractClient;

namespace Decoration
{
class Renderer;
}

class KWIN_EXPORT Deleted
    : public Toplevel
{
    Q_OBJECT
    Q_PROPERTY(bool minimized READ isMinimized)
    Q_PROPERTY(bool modal READ isModal)
    Q_PROPERTY(bool fullScreen READ isFullScreen CONSTANT)
    Q_PROPERTY(bool isCurrentTab READ isCurrentTab)
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
    QPoint clientContentPos() const override {
        return m_contentPos;
    }
    virtual QRect transparentRect() const;
    virtual bool isDeleted() const;
    virtual xcb_window_t frameId() const override;
    bool noBorder() const {
        return no_border;
    }
    void layoutDecorationRects(QRect &left, QRect &top, QRect &right, QRect &bottom) const;
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
    QList<AbstractClient*> mainClients() const {
        return m_mainClients;
    }
    NET::WindowType windowType(bool direct = false, int supported_types = 0) const;
    bool wasClient() const {
        return m_wasClient;
    }
    double opacity() const override;
    QByteArray windowRole() const override;

    const Decoration::Renderer *decorationRenderer() const {
        return m_decorationRenderer;
    }

    bool isFullScreen() const {
        return m_fullscreen;
    }

    bool isCurrentTab() const {
        return m_wasCurrentTab;
    }
protected:
    virtual void debug(QDebug& stream) const;
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
    QPoint m_contentPos;
    QRect transparent_rect;
    xcb_window_t m_frame;

    bool no_border;
    QRect decoration_left;
    QRect decoration_right;
    QRect decoration_top;
    QRect decoration_bottom;
    Layer m_layer;
    bool m_minimized;
    bool m_modal;
    QList<AbstractClient*> m_mainClients;
    bool m_wasClient;
    bool m_wasCurrentTab;
    Decoration::Renderer *m_decorationRenderer;
    double m_opacity;
    NET::WindowType m_type = NET::Unknown;
    QByteArray m_windowRole;
    bool m_fullscreen;
};

inline void Deleted::refWindow()
{
    ++delete_refcount;
}

} // namespace

Q_DECLARE_METATYPE(KWin::Deleted*)

#endif
