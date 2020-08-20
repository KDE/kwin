/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2006 Lubos Lunak <l.lunak@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

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

class KWIN_EXPORT Deleted : public Toplevel
{
    Q_OBJECT

public:
    static Deleted* create(Toplevel* c);
    // used by effects to keep the window around for e.g. fadeout effects when it's destroyed
    void refWindow();
    void unrefWindow();
    void discard();
    QRect bufferGeometry() const override;
    QMargins bufferMargins() const override;
    QMargins frameMargins() const override;
    qreal bufferScale() const override;
    int desktop() const override;
    QStringList activities() const override;
    QVector<VirtualDesktop *> desktops() const override;
    QPoint clientPos() const override;
    QPoint clientContentPos() const override {
        return m_contentPos;
    }
    QRect transparentRect() const override;
    bool isDeleted() const override;
    xcb_window_t frameId() const override;
    bool noBorder() const {
        return no_border;
    }
    void layoutDecorationRects(QRect &left, QRect &top, QRect &right, QRect &bottom) const;
    Layer layer() const override {
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
    NET::WindowType windowType(bool direct = false, int supported_types = 0) const override;
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
     */
    bool wasActive() const {
        return m_wasActive;
    }

    /**
     * Returns whether this was an X11 client.
     *
     * @returns @c true if it was an X11 client, @c false otherwise.
     */
    bool wasX11Client() const {
        return m_wasX11Client;
    }

    /**
     * Returns whether this was a Wayland client.
     *
     * @returns @c true if it was a Wayland client, @c false otherwise.
     */
    bool wasWaylandClient() const {
        return m_wasWaylandClient;
    }

    /**
     * Returns whether the client was a transient.
     *
     * @returns @c true if it was a transient, @c false otherwise.
     */
    bool wasTransient() const {
        return !m_transientFor.isEmpty();
    }

    /**
     * Returns whether the client was a group transient.
     *
     * @returns @c true if it was a group transient, @c false otherwise.
     * @note This is relevant only for X11 clients.
     */
    bool wasGroupTransient() const {
        return m_wasGroupTransient;
    }

    /**
     * Checks whether this client was a transient for given toplevel.
     *
     * @param toplevel Toplevel against which we are testing.
     * @returns @c true if it was a transient for given toplevel, @c false otherwise.
     */
    bool wasTransientFor(const Toplevel *toplevel) const {
        return m_transientFor.contains(const_cast<Toplevel *>(toplevel));
    }

    /**
     * Returns the list of transients.
     *
     * Because the window is Deleted, it can have only Deleted child transients.
     */
    QList<Deleted *> transients() const {
        return m_transients;
    }

    /**
     * Returns whether the client was a popup.
     *
     * @returns @c true if the client was a popup, @c false otherwise.
     */
    bool isPopupWindow() const override {
        return m_wasPopupWindow;
    }

    QVector<uint> x11DesktopIds() const;

    /**
     * Whether this Deleted represents the outline.
     */
    bool isOutline() const override {
        return m_wasOutline;
    }

private Q_SLOTS:
    void mainClientClosed(KWin::Toplevel *client);
    void transientForClosed(Toplevel *toplevel, Deleted *deleted);

private:
    Deleted();   // use create()
    void copyToDeleted(Toplevel* c);
    ~Deleted() override; // deleted only using unrefWindow()

    void addTransient(Deleted *transient);
    void removeTransient(Deleted *transient);
    void addTransientFor(AbstractClient *parent);
    void removeTransientFor(Deleted *parent);

    QRect m_bufferGeometry;
    QMargins m_bufferMargins;
    QMargins m_frameMargins;

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
    QList<Toplevel *> m_transientFor;
    QList<Deleted *> m_transients;
    bool m_wasPopupWindow;
    bool m_wasOutline;
    qreal m_bufferScale = 1;
};

inline void Deleted::refWindow()
{
    ++delete_refcount;
}

} // namespace

Q_DECLARE_METATYPE(KWin::Deleted*)

#endif
