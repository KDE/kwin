/*
    SPDX-FileCopyrightText: 2022 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "cursordelegate_qpainter.h"
#include "core/renderlayer.h"
#include "core/rendertarget.h"
#include "cursor.h"

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

    const Cursor *cursor = Cursors::self()->currentCursor();
    QPainter painter(buffer);
    painter.setClipRegion(region);
    painter.drawImage(layer()->mapToGlobal(layer()->rect()), cursor->image());
}

} // namespace KWin
