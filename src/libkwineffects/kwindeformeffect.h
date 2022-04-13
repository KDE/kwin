/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "kwineffects.h"

namespace KWin
{

class DeformEffectPrivate;

/**
 * The DeformEffect class is the base class for effects that paint deformed windows.
 *
 * Under the hood, the DeformEffect will paint the window into an offscreen texture,
 * which will be mapped onto transformed window quad grid later on.
 *
 * The redirect() function must be called when the effect wants to transform a window.
 * Once the effect is no longer interested in the window, the unredirect() function
 * must be called.
 *
 * If a window is redirected into offscreen texture, the deform() function will be
 * called with the window quads that can be mutated by the effect. The effect can
 * sub-divide, remove, or transform the window quads.
 */
class KWINEFFECTS_EXPORT DeformEffect : public Effect
{
    Q_OBJECT

public:
    explicit DeformEffect(QObject *parent = nullptr);
    ~DeformEffect() override;

    static bool supported();

    /**
     * If set our offscreen texture will be updated with the latest contents
     * It should be set before redirecting windows
     * The default is true
     */
    void setLive(bool live);

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
     * Override this function to transform the window quad grid of the given window.
     */
    virtual void deform(EffectWindow *window, int mask, WindowPaintData &data, WindowQuadList &quads);

    /**
     * Allows to specify a @p shader to draw the redirected texture for @p window.
     * Can only be called once the window is redirected.
     * @since 5.25
     **/
    void setShader(EffectWindow *window, GLShader *shader);

private Q_SLOTS:
    void handleWindowDamaged(EffectWindow *window);
    void handleWindowDeleted(EffectWindow *window);

private:
    void setupConnections();
    void destroyConnections();

    QScopedPointer<DeformEffectPrivate> d;
};

} // namespace KWin
