/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "kwineffects.h"

namespace KWin
{

class GLShader;
class OffscreenEffectPrivate;
class CrossFadeEffectPrivate;
class ShaderEffectPrivate;

/**
 * The OffscreenEffect class is the base class for effects that paint deformed windows.
 *
 * Under the hood, the OffscreenEffect will paint the window into an offscreen texture
 * and the offscreen texture will be transformed afterwards.
 *
 * The redirect() function must be called when the effect wants to transform a window.
 * Once the effect is no longer interested in the window, the unredirect() function
 * must be called.
 *
 * If a window is redirected into offscreen texture, the deform() function will be
 * called to transform the offscreen texture.
 */
class KWINEFFECTS_EXPORT OffscreenEffect : public Effect
{
    Q_OBJECT

public:
    explicit OffscreenEffect(QObject *parent = nullptr);
    ~OffscreenEffect() override;

    static bool supported();

protected:
    void drawWindow(EffectWindow *window, int mask, const QRegion &region, WindowPaintData &data) override;

    /**
     * This function must be called when the effect wants to animate the specified
     * @a window.
     */
    void redirect(EffectWindow *window);
    /**
     * This function must be called when the effect is done animating the specified
     * @a window. The window will be automatically unredirected if it's deleted.
     */
    void unredirect(EffectWindow *window);

    /**
     * Override this function to transform the window.
     */
    virtual void apply(EffectWindow *window, int mask, WindowPaintData &data, WindowQuadList &quads);

    /**
     * Allows to specify a @p shader to draw the redirected texture for @p window.
     * Can only be called once the window is redirected.
     **/
    void setShader(EffectWindow *window, GLShader *shader);

    /**
     * Set what mode to use to snap the vertices of this effect.
     *
     * @see RenderGeometry::VertexSnappingMode
     */
    void setVertexSnappingMode(RenderGeometry::VertexSnappingMode mode);

private Q_SLOTS:
    void handleWindowDamaged(EffectWindow *window);
    void handleWindowDeleted(EffectWindow *window);

private:
    void setupConnections();
    void destroyConnections();

    std::unique_ptr<OffscreenEffectPrivate> d;
};

/**
 * The CrossFadeEffect class is the base class for effects that paints crossfades
 *
 * Windows are snapshotted at the time we want to start crossfading from. Hereafter we draw both the new contents
 * and the old pixmap at the ratio defined by WindowPaintData::crossFadeProgress
 *
 * Subclasses are responsible for driving the animation and calling unredirect after animation completes.
 *
 * If window geometry changes shape after this point our "old" pixmap is resized to fit approximately matching
 * frame geometry
 */
class KWINEFFECTS_EXPORT CrossFadeEffect : public Effect
{
    Q_OBJECT
public:
    explicit CrossFadeEffect(QObject *parent = nullptr);
    ~CrossFadeEffect() override;

    void drawWindow(EffectWindow *window, int mask, const QRegion &region, WindowPaintData &data) override;

    /**
     * This function must be called when the effect wants to animate the specified
     * @a window.
     */
    void redirect(EffectWindow *window);
    /**
     * This function must be called when the effect is done animating the specified
     * @a window. The window will be automatically unredirected if it's deleted.
     */
    void unredirect(EffectWindow *window);

    /**
     * Allows to specify a @p shader to draw the redirected texture for @p window.
     * Can only be called once the window is redirected.
     * @since 5.25
     **/
    void setShader(EffectWindow *window, GLShader *shader);

    static bool supported();

private:
    void handleWindowDeleted(EffectWindow *window);
    std::unique_ptr<CrossFadeEffectPrivate> d;
};

} // namespace KWin
