/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2006 Lubos Lunak <l.lunak@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef KWIN_DELETED_H
#define KWIN_DELETED_H

#include "abstract_client.h"

namespace KWin
{

namespace Decoration
{
class Renderer;
}

class KWIN_EXPORT Deleted : public AbstractClient
{
    Q_OBJECT

public:
    static Deleted* create(Toplevel* c);
    // used by effects to keep the window around for e.g. fadeout effects when it's destroyed
    void refWindow();
    void unrefWindow();
    void discard();
    QMargins frameMargins() const override;
    qreal bufferScale() const override;
    int desktop() const override;
    QStringList activities() const override;
    QVector<VirtualDesktop *> desktops() const override;
    QPoint clientPos() const override;
    QRect transparentRect() const override;
    bool isDeleted() const override;
    xcb_window_t frameId() const override;
    bool wasDecorated() const;
    void layoutDecorationRects(QRect &left, QRect &top, QRect &right, QRect &bottom) const override;
    Layer layer() const override {
        return m_layer;
    }
    bool isMinimized() const {
        return m_minimized;
    }
    bool isModal() const {
        return m_modal;
    }
    QList<AbstractClient*> mainClients() const override {
        return m_mainClients;
    }
    NET::WindowType windowType(bool direct = false, int supported_types = 0) const override;
    bool wasClient() const {
        return m_wasClient;
    }
    QByteArray windowRole() const override;

    const Decoration::Renderer *decorationRenderer() const {
        return m_decorationRenderer;
    }

    bool isFullScreen() const override {
        return m_fullscreen;
    }

    bool keepAbove() const {
        return m_keepAbove;
    }
    bool keepBelow() const {
        return m_keepBelow;
    }

    QString captionNormal() const override { return m_caption; }
    QString captionSuffix() const override { return {}; }
    bool isCloseable() const override { return false; }
    bool isShown(bool /*shaded_is_shown*/) const override { return false; }
    bool isHiddenInternal() const override { return false; }
    void hideClient(bool /*hide*/) override {}
    AbstractClient *findModal(bool /*allow_itself*/) override { return nullptr; }
    bool isResizable() const override { return false; }
    bool isMovable() const override { return false; }
    bool isMovableAcrossScreens() const override { return false; }
    bool takeFocus() override { return false; }
    bool wantsInput() const override { return false; }
    void setFrameGeometry(const QRect &/*rect*/) override {}
    void killWindow() override {}
    void destroyClient() override {}
    void closeWindow() override {}
    bool acceptsFocus() const override { return false; }
    bool belongsToSameApplication(const AbstractClient *other, SameApplicationChecks /*checks*/) const override { return other == this; }
    void updateCaption() override {}
    void resizeWithChecks(const QSize&) override {}

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
    bool isLockScreen() const override {
        return m_wasLockScreen;
    }

private Q_SLOTS:
    void mainClientClosed(KWin::Toplevel *client);

private:
    Deleted();   // use create()
    void copyToDeleted(Toplevel* c);
    ~Deleted() override; // deleted only using unrefWindow()

    QMargins m_frameMargins;

    int delete_refcount;
    int desk;
    QStringList activityList;
    QRect contentsRect; // for clientPos()/clientSize()
    QRect transparent_rect;
    xcb_window_t m_frame;
    QVector <VirtualDesktop *> m_desktops;

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
    NET::WindowType m_type = NET::Unknown;
    QByteArray m_windowRole;
    bool m_fullscreen;
    bool m_keepAbove;
    bool m_keepBelow;
    QString m_caption;
    bool m_wasPopupWindow;
    bool m_wasOutline;
    bool m_wasDecorated;
    bool m_wasLockScreen;
    qreal m_bufferScale = 1;
};

inline void Deleted::refWindow()
{
    ++delete_refcount;
}

} // namespace

Q_DECLARE_METATYPE(KWin::Deleted*)

#endif
