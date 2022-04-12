/*
    SPDX-FileCopyrightText: 2022 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "cursorview.h"
#include "renderlayer.h"

namespace KWin
{

CursorView::CursorView(QObject *parent)
    : QObject(parent)
{
}

CursorDelegate::CursorDelegate(CursorView *view)
    : m_view(view)
{
}

void CursorDelegate::paint(RenderTarget *renderTarget, const QRegion &region)
{
    if (region.intersects(layer()->mapToGlobal(layer()->rect()))) {
        m_view->paint(layer(), renderTarget, region);
    }
}

} // namespace KWin
