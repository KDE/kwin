/*
    SPDX-FileCopyrightText: 2022 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "scene/cursordelegate_qpainter.h"
#include "compositor.h"
#include "core/output.h"
#include "core/renderlayer.h"
#include "core/rendertarget.h"
#include "core/renderviewport.h"
#include "cursor.h"
#include "scene/cursorscene.h"

#include <QPainter>

namespace KWin
{

CursorDelegateQPainter::CursorDelegateQPainter(Scene *scene, Output *output)
    : SceneDelegate(scene, nullptr)
    , m_output(output)
{
}

void CursorDelegateQPainter::paint(const RenderTarget &renderTarget, const QRegion &region)
{
    if (!region.intersects(layer()->mapToGlobal(layer()->rect()).toAlignedRect())) {
        return;
    }

    QImage *buffer = renderTarget.image();
    if (Q_UNLIKELY(!buffer)) {
        return;
    }

    const QSize bufferSize = (Cursors::self()->currentCursor()->rect().size() * m_output->scale()).toSize();
    if (m_buffer.size() != bufferSize) {
        m_buffer = QImage(bufferSize, QImage::Format_ARGB32_Premultiplied);
    }

    RenderTarget offscreenRenderTarget(&m_buffer);

    RenderLayer renderLayer(layer()->loop());
    renderLayer.setDelegate(std::make_unique<SceneDelegate>(Compositor::self()->cursorScene(), m_output));
    renderLayer.delegate()->prePaint();
    renderLayer.delegate()->paint(offscreenRenderTarget, infiniteRegion());
    renderLayer.delegate()->postPaint();

    QPainter painter(buffer);
    painter.setClipRegion(region);
    painter.drawImage(layer()->mapToGlobal(layer()->rect()), m_buffer);
}

} // namespace KWin
