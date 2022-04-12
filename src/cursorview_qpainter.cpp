/*
    SPDX-FileCopyrightText: 2022 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "cursorview_qpainter.h"
#include "cursor.h"
#include "renderlayer.h"
#include "rendertarget.h"

#include <QPainter>

namespace KWin
{

QPainterCursorView::QPainterCursorView(QObject *parent)
    : CursorView(parent)
{
}

void QPainterCursorView::paint(RenderLayer *renderLayer, RenderTarget *renderTarget, const QRegion &region)
{
    QImage *buffer = std::get<QImage *>(renderTarget->nativeHandle());
    if (Q_UNLIKELY(!buffer)) {
        return;
    }

    const Cursor *cursor = Cursors::self()->currentCursor();
    QPainter painter(buffer);
    painter.setClipRegion(region);
    painter.drawImage(renderLayer->mapToGlobal(renderLayer->rect()), cursor->image());
}

} // namespace KWin
