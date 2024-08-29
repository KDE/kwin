/*
    SPDX-FileCopyrightText: 2010 Fredrik Höglund <fredrik@kde.org>
    SPDX-FileCopyrightText: 2014 Marco Martin <mart@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "effect/effect.h"
#include "opengl/glplatform.h"
#include "opengl/glutils.h"
#include "scene/item.h"

#include <QList>
#include <QVector2D>
#include <unordered_map>

namespace KWin
{

class ContrastManagerInterface;
class ContrastShader;

class ContrastEffect : public KWin::Effect
{
    Q_OBJECT
public:
    ContrastEffect();
    ~ContrastEffect() override;

    static bool supported();
    static bool enabledByDefault();

    static QMatrix4x4 colorMatrix(qreal contrast, qreal intensity, qreal saturation);
    void drawWindow(const RenderTarget &renderTarget, const RenderViewport &viewport, EffectWindow *w, int mask, const QRegion &region, WindowPaintData &data) override;

    bool provides(Feature feature) override;
    bool isActive() const override;

    int requestedEffectChainPosition() const override
    {
        return 21;
    }

    bool eventFilter(QObject *watched, QEvent *event) override;

    bool blocksDirectScanout() const override;

public Q_SLOTS:
    void slotWindowAdded(KWin::EffectWindow *w);
    void slotWindowDeleted(KWin::EffectWindow *w);
#if KWIN_BUILD_X11
    void slotPropertyNotify(KWin::EffectWindow *w, long atom);
#endif
    void slotScreenGeometryChanged();

private:
    QRegion contrastRegion(const EffectWindow *w) const;
    bool shouldContrast(const EffectWindow *w, int mask, const WindowPaintData &data) const;
    void updateContrastRegion(EffectWindow *w);
    void doContrast(const RenderTarget &renderTarget, const RenderViewport &viewport, EffectWindow *w, const QRegion &shape, const float opacity, const QMatrix4x4 &screenProjection);
    void uploadRegion(std::span<QVector2D> map, const QRegion &region, qreal scale);
    Q_REQUIRED_RESULT bool uploadGeometry(GLVertexBuffer *vbo, const QRegion &region, qreal scale);

private:
    std::unique_ptr<ContrastShader> m_shader;

#if KWIN_BUILD_X11
    long m_net_wm_contrast_region = 0;
#endif
    QHash<const EffectWindow *, QMetaObject::Connection> m_contrastChangedConnections; // used only in Wayland to keep track of effect changed
    struct Data
    {
        QMatrix4x4 colorMatrix;
        QRegion contrastRegion;
        std::unique_ptr<GLTexture> texture;
        std::unique_ptr<GLFramebuffer> fbo;
        ItemEffect windowEffect;
    };
    std::unordered_map<const EffectWindow *, Data> m_windowData;
    static ContrastManagerInterface *s_contrastManager;
    static QTimer *s_contrastManagerRemoveTimer;
};

inline bool ContrastEffect::provides(Effect::Feature feature)
{
    if (feature == Contrast) {
        return true;
    }
    return KWin::Effect::provides(feature);
}

} // namespace KWin
