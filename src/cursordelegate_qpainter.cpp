/*
    SPDX-FileCopyrightText: 2022 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "cursordelegate_qpainter.h"
#include "composite.h"
#include "core/renderlayer.h"
#include "core/rendertarget.h"
#include "cursor.h"
#include "scene/cursorscene.h"

#include <QPainter>

namespace KWin
{

void CursorDelegateQPainter::paint(RenderTarget *renderTarget, const QRegion &region)
{
    if (!region.intersects(layer()->mapToGlobal(layer()->rect()))) {
        return;
    }

    QImage *buffer = std::get<QImage *>(renderTarget->nativeHandle());
    if (Q_UNLIKELY(!buffer)) {
        return;
    }

    const QSize bufferSize = Cursors::self()->currentCursor()->rect().size() * renderTarget->devicePixelRatio();
    if (m_buffer.size() != bufferSize) {
        m_buffer = QImage(bufferSize, QImage::Format_ARGB32_Premultiplied);
    }

    RenderTarget offscreenRenderTarget(&m_buffer);
    offscreenRenderTarget.setDevicePixelRatio(renderTarget->devicePixelRatio());

    RenderLayer renderLayer(layer()->loop());
    renderLayer.setDelegate(std::make_unique<SceneDelegate>(Compositor::self()->cursorScene()));
    renderLayer.delegate()->prePaint();
    renderLayer.delegate()->paint(&offscreenRenderTarget, infiniteRegion());
    renderLayer.delegate()->postPaint();

    QPainter painter(buffer);
    painter.setClipRegion(region);
    painter.drawImage(layer()->mapToGlobal(layer()->rect()), m_buffer);
}

} // namespace KWin
