/*
    SPDX-FileCopyrightText: 2022 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "cursordelegate_qpainter.h"
#include "cursor.h"
#include "renderlayer.h"
#include "rendertarget.h"

#include <QPainter>

namespace KWin
{

CursorDelegateQPainter::CursorDelegateQPainter(QObject *parent)
    : RenderLayerDelegate(parent)
{
}

void CursorDelegateQPainter::paint(RenderTarget *renderTarget, const QRegion &region)
{
    if (!region.intersects(layer()->mapToGlobal(layer()->rect()))) {
        return;
    }

    QImage *buffer = std::get<QImage *>(renderTarget->nativeHandle());
    if (Q_UNLIKELY(!buffer)) {
        return;
    }

    const Cursor *cursor = Cursors::self()->currentCursor();
    QPainter painter(buffer);
    painter.setClipRegion(region);
    painter.drawImage(layer()->mapToGlobal(layer()->rect()), cursor->image());
}

} // namespace KWin
