/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2013 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef KWIN_SCENE_QPAINTER_BACKEND_H
#define KWIN_SCENE_QPAINTER_BACKEND_H

#include "renderbackend.h"

class QImage;
class QRegion;
class QSize;
class QString;

namespace KWin
{

class SurfacePixmapInternal;
class SurfacePixmapWayland;
class SurfaceTexture;
class AbstractOutput;

class KWIN_EXPORT QPainterBackend : public RenderBackend
{
    Q_OBJECT

public:
    virtual ~QPainterBackend();

    CompositingType compositingType() const override final;

    SurfaceTexture *createSurfaceTextureInternal(SurfacePixmapInternal *pixmap);
    SurfaceTexture *createSurfaceTextureWayland(SurfacePixmapWayland *pixmap);

    /**
     * @brief Whether the creation of the Backend failed.
     *
     * The SceneQPainter should test whether the Backend got constructed correctly. If this method
     * returns @c true, the SceneQPainter should not try to start the rendering.
     *
     * @return bool @c true if the creation of the Backend failed, @c false otherwise.
     */
    bool isFailed() const {
        return m_failed;
    }
    virtual QImage *bufferForScreen(AbstractOutput *output) = 0;

protected:
    QPainterBackend();
    /**
     * @brief Sets the backend initialization to failed.
     *
     * This method should be called by the concrete subclass in case the initialization failed.
     * The given @p reason is logged as a warning.
     *
     * @param reason The reason why the initialization failed.
     */
    void setFailed(const QString &reason);

private:
    bool m_failed;
};

} // KWin

#endif
