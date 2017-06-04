/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2010 by Fredrik Höglund <fredrik@kde.org>
Copyright (C) 2010 Martin Gräßlin <mgraesslin@kde.org>

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

#ifndef KWIN_LANCZOSFILTER_P_H
#define KWIN_LANCZOSFILTER_P_H

#include <QObject>
#include <QBasicTimer>
#include <QVector>
#include <QVector2D>
#include <QVector4D>

#include <kwinconfig.h>

namespace KWin
{

class EffectWindow;
class EffectWindowImpl;
class WindowPaintData;
class GLTexture;
class GLRenderTarget;
class GLShader;

class LanczosFilter
    : public QObject
{
    Q_OBJECT

public:
    explicit LanczosFilter(QObject* parent = 0);
    ~LanczosFilter();
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
    QVector2D m_offsets[16];
    QVector4D m_kernel[16];
};

} // namespace

#endif // KWIN_LANCZOSFILTER_P_H
