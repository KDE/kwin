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
    Q_PROPERTY(bool keepAbove READ keepAbove CONSTANT)
    Q_PROPERTY(bool keepBelow READ keepBelow CONSTANT)
    Q_PROPERTY(QString caption READ caption CONSTANT)

public:
    static Deleted* create(Toplevel* c);
    // used by effects to keep the window around for e.g. fadeout effects when it's destroyed
    void refWindow();
    void unrefWindow();
    void discard();
    virtual int desktop() const;
    virtual QStringList activities() const;
    virtual QVector<VirtualDesktop *> desktops() const;
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
    bool keepAbove() const {
        return m_keepAbove;
    }
    bool keepBelow() const {
        return m_keepBelow;
    }
    QString caption() const {
        return m_caption;
    }

    /**
     *  Returns whether the client was active.
     *
     * @returns @c true if the client was active at the time when it was closed,
     *   @c false otherwise
     **/
    bool wasActive() const {
        return m_wasActive;
    }

    /**
     * Returns whether this was an X11 client.
     *
     * @returns @c true if it was an X11 client, @c false otherwise.
     **/
    bool wasX11Client() const {
        return m_wasX11Client;
    }

    /**
     * Returns whether this was a Wayland client.
     *
     * @returns @c true if it was a Wayland client, @c false otherwise.
     **/
    bool wasWaylandClient() const {
        return m_wasWaylandClient;
    }

    /**
     * Returns whether the client was a transient.
     *
     * @returns @c true if it was a transient, @c false otherwise.
     **/
    bool wasTransient() const {
        return !m_transientFor.isEmpty();
    }

    /**
     * Returns whether the client was a group transient.
     *
     * @returns @c true if it was a group transient, @c false otherwise.
     * @note This is relevant only for X11 clients.
     **/
    bool wasGroupTransient() const {
        return m_wasGroupTransient;
    }

    /**
     * Checks whether this client was a transient for given toplevel.
     *
     * @param toplevel Toplevel against which we are testing.
     * @returns @c true if it was a transient for given toplevel, @c false otherwise.
     **/
    bool wasTransientFor(const Toplevel *toplevel) const {
        return m_transientFor.contains(const_cast<Toplevel *>(toplevel));
    }

    /**
     * Returns the list of transients.
     *
     * Because the window is Deleted, it can have only Deleted child transients.
     **/
    DeletedList transients() const {
        return m_transients;
    }

    /**
     * Returns whether the client was a popup.
     *
     * @returns @c true if the client was a popup, @c false otherwise.
     **/
    bool isPopupWindow() const override {
        return m_wasPopupWindow;
    }

    QVector<uint> x11DesktopIds() const;

protected:
    virtual void debug(QDebug& stream) const;

private Q_SLOTS:
    void mainClientClosed(KWin::Toplevel *client);
    void transientForClosed(Toplevel *toplevel, Deleted *deleted);

private:
    Deleted();   // use create()
    void copyToDeleted(Toplevel* c);
    virtual ~Deleted(); // deleted only using unrefWindow()

    void addTransient(Deleted *transient);
    void removeTransient(Deleted *transient);
    void addTransientFor(AbstractClient *parent);
    void removeTransientFor(Deleted *parent);

    int delete_refcount;
    int desk;
    QStringList activityList;
    QRect contentsRect; // for clientPos()/clientSize()
    QPoint m_contentPos;
    QRect transparent_rect;
    xcb_window_t m_frame;
    QVector <VirtualDesktop *> m_desktops;

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
    bool m_keepAbove;
    bool m_keepBelow;
    QString m_caption;
    bool m_wasActive;
    bool m_wasX11Client;
    bool m_wasWaylandClient;
    bool m_wasGroupTransient;
    ToplevelList m_transientFor;
    DeletedList m_transients;
    bool m_wasPopupWindow;
};

inline void Deleted::refWindow()
{
    ++delete_refcount;
}

} // namespace

Q_DECLARE_METATYPE(KWin::Deleted*)

#endif
