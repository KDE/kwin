/*
    SPDX-FileCopyrightText: 2010 Fredrik Höglund <fredrik@kde.org>
    SPDX-FileCopyrightText: 2014 Marco Martin <mart@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <kwineffects.h>
#include <kwinglplatform.h>
#include <kwinglutils.h>

#include <QVector2D>
#include <QVector>

namespace KWaylandServer
{
class ContrastManagerInterface;
}

namespace KWin
{

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
    void drawWindow(EffectWindow *w, int mask, const QRegion &region, WindowPaintData &data) override;

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
    void slotPropertyNotify(KWin::EffectWindow *w, long atom);
    void slotScreenGeometryChanged();

private:
    QRegion contrastRegion(const EffectWindow *w) const;
    bool shouldContrast(const EffectWindow *w, int mask, const WindowPaintData &data) const;
    void updateContrastRegion(EffectWindow *w);
    void doContrast(EffectWindow *w, const QRegion &shape, const QRect &screen, const float opacity, const QMatrix4x4 &screenProjection);
    void uploadRegion(QVector2D *&map, const QRegion &region, qreal scale);
    Q_REQUIRED_RESULT bool uploadGeometry(GLVertexBuffer *vbo, const QRegion &region, qreal scale);

private:
    std::unique_ptr<ContrastShader> m_shader;
    long m_net_wm_contrast_region = 0;
    QHash<const EffectWindow *, QMetaObject::Connection> m_contrastChangedConnections; // used only in Wayland to keep track of effect changed
    struct Data
    {
        QMatrix4x4 colorMatrix;
        QRegion contrastRegion;
    };
    QHash<const EffectWindow *, Data> m_windowData;
    static KWaylandServer::ContrastManagerInterface *s_contrastManager;
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
