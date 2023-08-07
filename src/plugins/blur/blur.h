/*
    SPDX-FileCopyrightText: 2010 Fredrik Höglund <fredrik@kde.org>
    SPDX-FileCopyrightText: 2018 Alex Nemeth <alex.nemeth329@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "libkwineffects/kwineffects.h"
#include "libkwineffects/kwinglplatform.h"
#include "libkwineffects/kwinglutils.h"

#include <QStack>
#include <QVector2D>
#include <QVector>

namespace KWaylandServer
{
class BlurManagerInterface;
}

namespace KWin
{

static const int borderSize = 5;

class BlurShader;

class BlurEffect : public KWin::Effect
{
    Q_OBJECT

public:
    BlurEffect();
    ~BlurEffect() override;

    static bool supported();
    static bool enabledByDefault();

    void reconfigure(ReconfigureFlags flags) override;
    void prePaintScreen(ScreenPrePaintData &data, std::chrono::milliseconds presentTime) override;
    void prePaintWindow(EffectWindow *w, WindowPrePaintData &data, std::chrono::milliseconds presentTime) override;
    void drawWindow(const RenderTarget &renderTarget, const RenderViewport &viewport, EffectWindow *w, int mask, const QRegion &region, WindowPaintData &data) override;

    bool provides(Feature feature) override;
    bool isActive() const override;

    int requestedEffectChainPosition() const override
    {
        return 20;
    }

    bool eventFilter(QObject *watched, QEvent *event) override;

    bool blocksDirectScanout() const override;

public Q_SLOTS:
    void slotWindowAdded(KWin::EffectWindow *w);
    void slotWindowDeleted(KWin::EffectWindow *w);
    void slotPropertyNotify(KWin::EffectWindow *w, long atom);
    void setupDecorationConnections(EffectWindow *w);

private:
    struct ScreenData
    {
        // these are needed because the automatically generated ones are wrong with std::vector<std::unique_ptr<T>> as a member
        ScreenData() = default;
        ScreenData(const ScreenData &) = delete;
        ScreenData &operator=(ScreenData &&) = default;

        std::vector<std::unique_ptr<GLFramebuffer>> renderTargets;
        QStack<GLFramebuffer *> renderTargetStack;
    };

    QRect expand(const QRect &rect) const;
    QRegion expand(const QRegion &region) const;
    void initBlurStrengthValues();
    bool updateTexture(EffectScreen *screen, const RenderTarget &renderTarget);
    QRegion blurRegion(const EffectWindow *w) const;
    QRegion decorationBlurRegion(const EffectWindow *w) const;
    bool decorationSupportsBlurBehind(const EffectWindow *w) const;
    bool shouldBlur(const EffectWindow *w, int mask, const WindowPaintData &data) const;
    void updateBlurRegion(EffectWindow *w);
    void doBlur(const RenderTarget &renderTarget, const RenderViewport &viewport, const QRegion &shape, const QRect &screen, const float opacity, bool isDock, QRect windowRect);
    void uploadRegion(const std::span<QVector2D> map, size_t &index, const QRegion &region);
    Q_REQUIRED_RESULT bool uploadGeometry(GLVertexBuffer *vbo, const QRegion &expandedBlurRegion, const QRegion &blurRegion);
    void generateNoiseTexture();
    void screenAdded(EffectScreen *screen);
    void screenRemoved(EffectScreen *screen);
    void screenGeometryChanged(EffectScreen *screen);

    void upscaleRenderToScreen(const ScreenData &data, const RenderTarget &renderTarget, const RenderViewport &viewport, GLVertexBuffer *vbo, int vboStart, int blurRectCount, QPoint windowPosition);
    void applyNoise(const ScreenData &data, const RenderTarget &renderTarget, const RenderViewport &viewport, GLVertexBuffer *vbo, int vboStart, int blurRectCount, QPoint windowPosition);
    void downSampleTexture(const ScreenData &data, GLVertexBuffer *vbo, int blurRectCount, const QMatrix4x4 &projection);
    void upSampleTexture(const ScreenData &data, GLVertexBuffer *vbo, int blurRectCount, const QMatrix4x4 &projection);
    void copyScreenSampleTexture(const ScreenData &data, const RenderViewport &viewport, const QSize &fboSize, GLVertexBuffer *vbo, int blurRectCount, const QRect &boundingRect, const QMatrix4x4 &projection);

private:
    BlurShader *m_shader;
    std::map<EffectScreen *, ScreenData> m_screenData;

    std::unique_ptr<GLTexture> m_noiseTexture;

    long net_wm_blur_region = 0;
    QRegion m_paintedArea; // keeps track of all painted areas (from bottom to top)
    QRegion m_currentBlur; // keeps track of the currently blured area of the windows(from bottom to top)
    EffectScreen *m_currentScreen = nullptr;

    int m_downSampleIterations; // number of times the texture will be downsized to half size
    int m_offset;
    int m_expandSize;
    int m_noiseStrength;
    int m_scalingFactor;

    struct OffsetStruct
    {
        float minOffset;
        float maxOffset;
        int expandSize;
    };

    QVector<OffsetStruct> blurOffsets;

    struct BlurValuesStruct
    {
        int iteration;
        float offset;
    };

    QVector<BlurValuesStruct> blurStrengthValues;

    QMap<EffectWindow *, QMetaObject::Connection> windowBlurChangedConnections;
    QMap<const EffectWindow *, QRegion> blurRegions;

    static KWaylandServer::BlurManagerInterface *s_blurManager;
    static QTimer *s_blurManagerRemoveTimer;
};

inline bool BlurEffect::provides(Effect::Feature feature)
{
    if (feature == Blur) {
        return true;
    }
    return KWin::Effect::provides(feature);
}

} // namespace KWin
