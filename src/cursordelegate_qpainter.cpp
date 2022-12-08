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

CursorDelegateQPainter::CursorDelegateQPainter(Output *output, QObject *parent)
    : CursorDelegate(output, parent)
{
}

void CursorDelegateQPainter::paint(RenderTarget *renderTarget, const QRegion &region)
{
    if (region.isEmpty()) {
        return;
    }

    QImage *buffer = std::get<QImage *>(renderTarget->nativeHandle());
    if (Q_UNLIKELY(!buffer)) {
        return;
    }

    if (!layer()->superlayer()) {
        buffer->fill(Qt::transparent);
    }

    if (!region.intersects(layer()->mapToRoot(layer()->rect()))) {
        return;
    }

    const Cursor *cursor = Cursors::self()->currentCursor();
    QPainter painter(buffer);
    painter.setClipRegion(region);
    painter.drawImage(layer()->mapToRoot(layer()->rect()), cursor->image());
}

} // namespace KWin
