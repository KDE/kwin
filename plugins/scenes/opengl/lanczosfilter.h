/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2010 Fredrik Höglund <fredrik@kde.org>
    SPDX-FileCopyrightText: 2010 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef KWIN_LANCZOSFILTER_P_H
#define KWIN_LANCZOSFILTER_P_H

#include <QObject>
#include <QBasicTimer>
#include <QVector>
#include <QVector2D>
#include <QVector4D>
#include <array>

namespace KWin
{

class EffectWindow;
class EffectWindowImpl;
class WindowPaintData;
class GLTexture;
class GLRenderTarget;
class GLShader;
class Scene;

class LanczosFilter : public QObject
{
    Q_OBJECT

public:
    explicit LanczosFilter(Scene *parent);
    ~LanczosFilter() override;
    void performPaint(EffectWindowImpl* w, int mask, QRegion region, WindowPaintData& data);

protected:
    void timerEvent(QTimerEvent*) override;
private:
    void init();
    void updateOffscreenSurfaces();
    void setUniforms();
    void discardCacheTexture(EffectWindow *w);

    void createKernel(float delta, int *kernelSize);
    void createOffsets(int count, float width, Qt::Orientation direction);
    GLTexture *m_offscreenTex;
    GLRenderTarget *m_offscreenTarget;
    QBasicTimer m_timer;
    bool m_inited;
    QScopedPointer<GLShader> m_shader;
    int m_uOffsets;
    int m_uKernel;
    std::array<QVector2D, 16> m_offsets;
    std::array<QVector4D, 16> m_kernel;
    Scene *m_scene;
};

} // namespace

#endif // KWIN_LANCZOSFILTER_P_H
