/*
    SPDX-FileCopyrightText: 2022 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "cursorview.h"
#include "abstract_output.h"
#include "cursor.h"
#include "renderlayer.h"

namespace KWin
{

CursorView::CursorView(QObject *parent)
    : QObject(parent)
{
}

CursorDelegate::CursorDelegate(AbstractOutput *output, CursorView *view)
    : m_view(view)
    , m_output(output)
{
}

void CursorDelegate::paint(const QRegion &region)
{
    const Cursor *cursor = Cursors::self()->currentCursor();
    if (region.intersects(cursor->geometry())) {
        m_view->paint(m_output, region);
    }
}

} // namespace KWin
