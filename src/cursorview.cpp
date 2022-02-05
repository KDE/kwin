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

void CursorDelegate::paint(const QRegion &damage, const QRegion &repaint, QRegion &update, QRegion &valid)
{
    const Cursor *cursor = Cursors::self()->currentCursor();
    const QRegion cursorDamage = damage.intersected(cursor->geometry());
    const QRegion cursorRepaint = repaint.intersected(cursor->geometry());

    update = cursorDamage;
    valid = cursorDamage.united(cursorRepaint);

    if (!valid.isEmpty()) {
        m_view->paint(m_output, valid);
    }
}

} // namespace KWin
