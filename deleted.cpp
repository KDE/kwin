/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2006 Lubos Lunak <l.lunak@kde.org>

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

#include "deleted.h"

#include "workspace.h"
#include "client.h"
#include "shadow.h"

namespace KWin
{

Deleted::Deleted(Workspace* ws)
    : Toplevel(ws)
    , delete_refcount(1)
    , no_border(true)
    , padding_left(0)
    , padding_top(0)
    , padding_right(0)
    , padding_bottom(0)
{
}

Deleted::~Deleted()
{
    if (delete_refcount != 0)
        kError(1212) << "Deleted client has non-zero reference count (" << delete_refcount << ")";
    assert(delete_refcount == 0);
    workspace()->removeDeleted(this, Allowed);
    deleteEffectWindow();
}

Deleted* Deleted::create(Toplevel* c)
{
    Deleted* d = new Deleted(c->workspace());
    d->copyToDeleted(c);
    d->workspace()->addDeleted(d, Allowed);
    return d;
}

// to be used only from Workspace::finishCompositing()
void Deleted::discard(allowed_t)
{
    delete_refcount = 0;
    delete this;
}

void Deleted::copyToDeleted(Toplevel* c)
{
    assert(dynamic_cast< Deleted* >(c) == NULL);
    Toplevel::copyToDeleted(c);
    desk = c->desktop();
    activityList = c->activities();
    contentsRect = QRect(c->clientPos(), c->clientSize());
    transparent_rect = c->transparentRect();
    if (WinInfo* cinfo = dynamic_cast< WinInfo* >(info))
        cinfo->disable();
    Client* client = dynamic_cast<Client*>(c);
    if (client) {
        no_border = client->noBorder();
        padding_left = client->paddingLeft();
        padding_right = client->paddingRight();
        padding_bottom = client->paddingBottom();
        padding_top = client->paddingTop();
        if (!no_border) {
            client->layoutDecorationRects(decoration_left,
                                          decoration_top,
                                          decoration_right,
                                          decoration_bottom,
                                          Client::WindowRelative);
            decorationPixmapLeft = *client->leftDecoPixmap();
            decorationPixmapRight = *client->rightDecoPixmap();
            decorationPixmapTop = *client->topDecoPixmap();
            decorationPixmapBottom = *client->bottomDecoPixmap();
        }
    }
}

void Deleted::unrefWindow(bool delay)
{
    if (--delete_refcount > 0)
        return;
    // needs to be delayed when calling from effects, otherwise it'd be rather
    // complicated to handle the case of the window going away during a painting pass
    if (delay)
        deleteLater();
    else
        delete this;
}

int Deleted::desktop() const
{
    return desk;
}

QStringList Deleted::activities() const
{
    return activityList;
}

QPoint Deleted::clientPos() const
{
    return contentsRect.topLeft();
}

QSize Deleted::clientSize() const
{
    return contentsRect.size();
}

void Deleted::debug(QDebug& stream) const
{
    stream << "\'ID:" << window() << "\' (deleted)";
}

void Deleted::layoutDecorationRects(QRect& left, QRect& top, QRect& right, QRect& bottom) const
{
    left = decoration_left;
    top = decoration_top;
    right = decoration_right;
    bottom = decoration_bottom;
}

QRect Deleted::decorationRect() const
{
    QRect r(rect());
    r.adjust(-padding_left, -padding_top, padding_top, padding_bottom);
    if (hasShadow())
        r |= shadow()->shadowRegion().boundingRect();
    return r;
}

QRect Deleted::transparentRect() const
{
    return transparent_rect;
}

} // namespace

#include "deleted.moc"
