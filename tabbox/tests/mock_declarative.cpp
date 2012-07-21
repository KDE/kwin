/********************************************************************
KWin - the KDE window manager
This file is part of the KDE project.

Copyright (C) 2012 Martin Gräßlin <mgraesslin@kde.org>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/
#include "declarative.h"

namespace KWin
{
namespace TabBox
{
DeclarativeView::DeclarativeView(QAbstractItemModel *model, TabBoxConfig::TabBoxMode mode, QWidget *parent)
    : QDeclarativeView(parent)
{
    Q_UNUSED(model)
    Q_UNUSED(mode)
}

void DeclarativeView::currentIndexChanged(int row)
{
    Q_UNUSED(row)
}

void DeclarativeView::updateQmlSource(bool force)
{
    Q_UNUSED(force)
}

void DeclarativeView::setCurrentIndex(const QModelIndex &index, bool disableAnimation)
{
    Q_UNUSED(index)
    Q_UNUSED(disableAnimation)
}

bool DeclarativeView::sendKeyEvent(QKeyEvent *e)
{
    Q_UNUSED(e)
    return false;
}

void DeclarativeView::slotEmbeddedChanged(bool enabled)
{
    Q_UNUSED(enabled)
}

void DeclarativeView::slotUpdateGeometry()
{
}

void DeclarativeView::slotWindowChanged(WId wId, unsigned int properties)
{
    Q_UNUSED(wId)
    Q_UNUSED(properties)
}

void DeclarativeView::showEvent(QShowEvent *event)
{
    Q_UNUSED(event)
}

void DeclarativeView::resizeEvent(QResizeEvent *event)
{
    Q_UNUSED(event)
}

void DeclarativeView::hideEvent(QHideEvent *event)
{
    Q_UNUSED(event)
}

bool DeclarativeView::x11Event(XEvent *e)
{
    Q_UNUSED(e)
    return false;
}

} // namespace Tabbox
} // namespace KWin

#include "../declarative.moc"
