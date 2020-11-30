/*
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "eglonxbackend.h"

namespace KWin
{

class EglBackend : public EglOnXBackend
{
    Q_OBJECT

public:
    explicit EglBackend(Display *display);

    SceneOpenGLTexturePrivate *createBackendTexture(SceneOpenGLTexture *texture) override;
    QRegion beginFrame(int screenId) override;
    void endFrame(int screenId, const QRegion &damage, const QRegion &damagedRegion) override;
    void screenGeometryChanged(const QSize &size) override;

private:
    void presentSurface(EGLSurface surface, const QRegion &damage, const QRect &screenGeometry);

    int m_bufferAge = 0;
};

class EglTexture : public AbstractEglTexture
{
public:
    ~EglTexture() override;

    void onDamage() override;
    bool loadTexture(WindowPixmap *pixmap) override;

private:
    friend class EglBackend;
    EglTexture(SceneOpenGLTexture *texture, EglBackend *backend);
    EglBackend *m_backend;
};

} // namespace KWin
