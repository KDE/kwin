/*
    SPDX-FileCopyrightText: 2022 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "cursorview_qpainter.h"
#include "abstract_output.h"
#include "composite.h"
#include "cursor.h"
#include "qpainterbackend.h"

#include <QPainter>

namespace KWin
{

QPainterCursorView::QPainterCursorView(QObject *parent)
    : CursorView(parent)
{
}

void QPainterCursorView::paint(AbstractOutput *output, const QRegion &region)
{
    QImage *renderTarget = static_cast<QPainterBackend *>(Compositor::self()->backend())->bufferForScreen(output);
    if (Q_UNLIKELY(!renderTarget)) {
        return;
    }

    const Cursor *cursor = Cursors::self()->currentCursor();
    QPainter painter(renderTarget);
    painter.setWindow(output->geometry());
    painter.setClipRegion(region);
    painter.drawImage(cursor->geometry(), cursor->image());
}

} // namespace KWin
