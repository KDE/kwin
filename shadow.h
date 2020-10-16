/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2011 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef KWIN_SHADOW_H
#define KWIN_SHADOW_H

#include <QObject>
#include <QPixmap>
#include <kwineffects.h>

namespace KDecoration2
{
class Decoration;
class DecorationShadow;
}

namespace KWaylandServer
{
class ShadowInterface;
}

namespace KWin {

class Toplevel;

/**
 * @short Class representing a Window's Shadow to be rendered by the Compositor.
 *
 * This class holds all information about the Shadow to be rendered together with the
 * window during the Compositing stage. The Shadow consists of several pixmaps and offsets.
 * For a complete description please refer to https://community.kde.org/KWin/Shadow
 *
 * To create a Shadow instance use the static factory method createShadow which will
 * create an instance for the currently used Compositing Backend. It will read the X11 Property
 * and create the Shadow and all required data (such as WindowQuads). If there is no Shadow
 * defined for the Toplevel the factory method returns @c NULL.
 * 
 * @author Martin Gräßlin <mgraesslin@kde.org>
 * @todo React on Toplevel size changes.
 */
class KWIN_EXPORT Shadow : public QObject
{
    Q_OBJECT
public:
    ~Shadow() override;

    /**
     * @return Region of the shadow.
     */
    const QRegion &shadowRegion() const {
        return m_shadowRegion;
    };
    /**
     * @return Cached Shadow Quads
     */
    const WindowQuadList &shadowQuads() const {
        return m_shadowQuads;
    };
    WindowQuadList &shadowQuads() {
        return m_shadowQuads;
    };

    /**
     * This method updates the Shadow when the property has been changed.
     * It is the responsibility of the owner of the Shadow to call this method
     * whenever the owner receives a PropertyNotify event.
     * This method will invoke a re-read of the Property. In case the Property has
     * been withdrawn the method returns @c false. In that case the owner should
     * delete the Shadow.
     * @returns @c true when the shadow has been updated, @c false if the property is not set anymore.
     */
    virtual bool updateShadow();

    /**
     * Factory Method to create the shadow from the property.
     * This method takes care of creating an instance of the
     * Shadow class for the current Compositing Backend.
     *
     * If there is no shadow defined for @p toplevel this method
     * will return @c NULL.
     * @param toplevel The Toplevel for which the shadow should be created
     * @return Created Shadow or @c NULL in case there is no shadow defined.
     */
    static Shadow *createShadow(Toplevel *toplevel);

    /**
     * Reparents the shadow to @p toplevel.
     * Used when a window is deleted.
     * @param toplevel The new parent
     */
    void setToplevel(Toplevel *toplevel);

    bool hasDecorationShadow() const {
        return !m_decorationShadow.isNull();
    }
    QImage decorationShadowImage() const;

    QWeakPointer<KDecoration2::DecorationShadow> decorationShadow() const {
        return m_decorationShadow.toWeakRef();
    }

public Q_SLOTS:
    void geometryChanged();

protected:
    Shadow(Toplevel *toplevel);
    enum ShadowElements {
        ShadowElementTop,
        ShadowElementTopRight,
        ShadowElementRight,
        ShadowElementBottomRight,
        ShadowElementBottom,
        ShadowElementBottomLeft,
        ShadowElementLeft,
        ShadowElementTopLeft,
        ShadowElementsCount
    };

    inline const QPixmap &shadowPixmap(ShadowElements element) const {
        return m_shadowElements[element];
    };
    QSize elementSize(ShadowElements element) const;

    int topOffset() const {
        return m_topOffset;
    };
    int rightOffset() const {
        return m_rightOffset;
    };
    int bottomOffset() const {
        return m_bottomOffset;
    };
    int leftOffset() const {
        return m_leftOffset;
    };
    virtual void buildQuads();
    void updateShadowRegion();
    Toplevel *topLevel() {
        return m_topLevel;
    };
    void setShadowRegion(const QRegion &region) {
        m_shadowRegion = region;
    };
    virtual bool prepareBackend() = 0;
    WindowQuadList m_shadowQuads;
    void setShadowElement(const QPixmap &shadow, ShadowElements element);

private:
    static Shadow *createShadowFromX11(Toplevel *toplevel);
    static Shadow *createShadowFromDecoration(Toplevel *toplevel);
    static Shadow *createShadowFromWayland(Toplevel *toplevel);
    static Shadow *createShadowFromInternalWindow(Toplevel *toplevel);
    static QVector<uint32_t> readX11ShadowProperty(xcb_window_t id);
    bool init(const QVector<uint32_t> &data);
    bool init(KDecoration2::Decoration *decoration);
    bool init(const QPointer<KWaylandServer::ShadowInterface> &shadow);
    bool init(const QWindow *window);
    Toplevel *m_topLevel;
    // shadow pixmaps
    QPixmap m_shadowElements[ShadowElementsCount];
    // shadow offsets
    int m_topOffset;
    int m_rightOffset;
    int m_bottomOffset;
    int m_leftOffset;
    // caches
    QRegion m_shadowRegion;
    QSize m_cachedSize;
    // Decoration based shadows
    QSharedPointer<KDecoration2::DecorationShadow> m_decorationShadow;
};

}

#endif // KWIN_SHADOW_H
