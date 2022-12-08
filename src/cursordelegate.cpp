/*
    SPDX-FileCopyrightText: 2022 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "cursordelegate.h"
#include "core/output.h"
#include "cursor.h"

namespace KWin
{

CursorDelegate::CursorDelegate(Output *output, QObject *parent)
    : RenderLayerDelegate(parent)
    , m_output(output)
{
}

void CursorDelegate::postPaint()
{
    const std::chrono::milliseconds frameTime =
        std::chrono::duration_cast<std::chrono::milliseconds>(m_output->renderLoop()->lastPresentationTimestamp());

    if (!Cursors::self()->isCursorHidden()) {
        Cursor *cursor = Cursors::self()->currentCursor();
        if (cursor->geometry().intersects(m_output->geometry())) {
            cursor->markAsRendered(frameTime);
        }
    }
}

} // namespace KWin
