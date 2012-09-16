/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2003 Lubos Lunak <l.lunak@kde.org>

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

#include "bridge.h"

#include "client.h"
#include "options.h"

#include <kconfiggroup.h>
#include "composite.h"

namespace KWin
{

Bridge::Bridge(Client* cl)
    : c(cl)
{
}

#define BRIDGE_HELPER( rettype, prototype, args1, args2, cst ) \
    rettype Bridge::prototype ( args1 ) cst \
    { \
        return c->prototype( args2 ); \
    }

BRIDGE_HELPER(bool, isCloseable, , , const)
BRIDGE_HELPER(bool, isMaximizable, , , const)
BRIDGE_HELPER(Bridge::MaximizeMode, maximizeMode, , , const)
BRIDGE_HELPER(bool, isMinimizable, , , const)
BRIDGE_HELPER(bool, providesContextHelp, , , const)
BRIDGE_HELPER(int, desktop, , , const)
BRIDGE_HELPER(bool, isModal, , , const)
BRIDGE_HELPER(bool, isShadeable, , , const)
BRIDGE_HELPER(bool, isShade, , , const)
BRIDGE_HELPER(bool, keepAbove, , , const)
BRIDGE_HELPER(bool, keepBelow, , , const)
BRIDGE_HELPER(bool, isMovable, , , const)
BRIDGE_HELPER(bool, isResizable, , , const)
BRIDGE_HELPER(QString, caption, , , const)
BRIDGE_HELPER(void, processMousePressEvent, QMouseEvent* e, e,)
BRIDGE_HELPER(QRect, geometry, , , const)
BRIDGE_HELPER(void, closeWindow, , ,)
BRIDGE_HELPER(void, maximize, MaximizeMode m, m,)
BRIDGE_HELPER(void, minimize, , ,)
BRIDGE_HELPER(void, showContextHelp, , ,)
BRIDGE_HELPER(void, setDesktop, int desktop, desktop,)

bool Bridge::isActive() const
{
    return c->isActive() || (c->tabGroup() && c->tabGroup()->isActive());
}

void Bridge::setKeepAbove(bool set)
{
    if (c->keepAbove() != set)
        c->workspace()->performWindowOperation(c, KeepAboveOp);
}

void Bridge::setKeepBelow(bool set)
{
    if (c->keepBelow() != set)
        c->workspace()->performWindowOperation(c, KeepBelowOp);
}

NET::WindowType Bridge::windowType(unsigned long supported_types) const
{
    return c->windowType(false, supported_types);
}

QIcon Bridge::icon() const
{
    QIcon ret(c->icon());
    ret.addPixmap(c->miniIcon());
    return ret;
}

bool Bridge::isSetShade() const
{
    return c->shadeMode() != ShadeNone;
}

void Bridge::showWindowMenu(const QPoint &p)
{
    c->workspace()->showWindowMenu(QRect(p,p), c);
}

void Bridge::showWindowMenu(const QPoint &p, long id)
{
    Client *cc = clientForId(id);
    if (!cc)
        cc = c;
    cc->workspace()->showWindowMenu(QRect(p,p), cc);
}

void Bridge::showWindowMenu(const QRect &p)
{
    c->workspace()->showWindowMenu(p, c);
}

void Bridge::performWindowOperation(WindowOperation op)
{
    c->workspace()->performWindowOperation(c, op);
}

void Bridge::setMask(const QRegion& r, int mode)
{
    c->setMask(r, mode);
}

bool Bridge::isPreview() const
{
    return false;
}

QRect Bridge::iconGeometry() const
{
    NETRect r = c->info->iconGeometry();
    return QRect(r.pos.x, r.pos.y, r.size.width, r.size.height);
}

WId Bridge::windowId() const
{
    return c->window();
}

void Bridge::titlebarDblClickOperation()
{
    c->workspace()->performWindowOperation(c, options->operationTitlebarDblClick());
}

void Bridge::titlebarMouseWheelOperation(int delta)
{
    c->performMouseCommand(options->operationTitlebarMouseWheel(delta), cursorPos());
}

void Bridge::setShade(bool set)
{
    c->setShade(set ? ShadeNormal : ShadeNone);
}

int Bridge::currentDesktop() const
{
    return c->workspace()->currentDesktop();
}

QWidget* Bridge::initialParentWidget() const
{
    return NULL;
}

Qt::WFlags Bridge::initialWFlags() const
{
    return 0;
}

QRegion Bridge::unobscuredRegion(const QRegion& r) const
{
    QRegion reg(r);
    const ToplevelList stacking_order = c->workspace()->stackingOrder();
    int pos = stacking_order.indexOf(c);
    ++pos;
    for (; pos < stacking_order.count(); ++pos) {
        Client *client = qobject_cast<Client*>(stacking_order[pos]);
        if (!client) {
            continue;
        }
        if (!client->isShown(true))
            continue; // these don't obscure the window
        if (c->isOnAllDesktops()) {
            if (!client->isOnCurrentDesktop())
                continue;
        } else {
            if (!client->isOnDesktop(c->desktop()))
                continue;
        }
        /* the clients all have their mask-regions in local coords
           so we have to translate them to a shared coord system
           we choose ours */
        int dx = client->x() - c->x();
        int dy = client->y() - c->y();
        QRegion creg = client->mask();
        creg.translate(dx, dy);
        reg -= creg;
        if (reg.isEmpty()) {
            // early out, we are completely obscured
            break;
        }
    }
    return reg;
}

void Bridge::grabXServer(bool grab)
{
    if (grab)
        KWin::grabXServer();
    else
        KWin::ungrabXServer();
}

bool Bridge::compositingActive() const
{
    return Compositor::compositing();
}

QRect Bridge::transparentRect() const
{
    return c->transparentRect().translated(-c->decorationRect().topLeft());
}

//BEGIN TABBING

Client *Bridge::clientForId(long id) const
{
    Client* client = reinterpret_cast<Client*>(id);
    if (!c->workspace()->hasClient(client)) {
        kWarning(1212) << "****** ARBITRARY CODE EXECUTION ATTEMPT DETECTED ******" << id;
        return 0;
    }
    return client;
}

int Bridge::tabCount() const
{
    if (c->tabGroup())
        return c->tabGroup()->count();
    return 1;
}

long Bridge::tabId(int idx) const
{
    if (c->tabGroup())
        return tabIdOf(c->tabGroup()->clients().at(idx));
    return tabIdOf(c);
}

QIcon Bridge::icon(int idx) const
{
    if (c->tabGroup()) {
        Client *tabC = c->tabGroup()->clients().at(idx);
        QIcon icon(tabC->icon());
        icon.addPixmap(tabC->miniIcon());
        return icon;
    }
    return icon();
}

QString Bridge::caption(int idx) const
{
    if (c->tabGroup())
        return c->tabGroup()->clients().at(idx)->caption();
    return c->caption();
}

long Bridge::currentTabId() const
{
    if (c->tabGroup())
        return tabIdOf(c->tabGroup()->current());
    return 0;
}

void Bridge::setCurrentTab(long id)
{
    if (c->tabGroup())
        c->tabGroup()->setCurrent(clientForId(id));
}

void Bridge::tab_A_before_B(long A, long B)
{
    if (!B) {
        if (c->tabGroup()) {
            if (Client *a = clientForId(A))
                a->untab();
        }
        return;
    }

    if (Client *a = clientForId(A))
    if (Client *b = clientForId(B))
        a->tabBefore(b, true);
}

void Bridge::tab_A_behind_B(long A, long B)
{
    if (!B) {
        if (c->tabGroup()) {
            if (Client *a = clientForId(A))
            a->untab();
        }
        return;
    }

    if (Client *a = clientForId(A))
    if (Client *b = clientForId(B))
        a->tabBehind(b, true);
}


void Bridge::untab(long id, const QRect& newGeom)
{
    if (c->tabGroup())
    if (Client* client = clientForId(id))
    if (client->untab(newGeom)) {
        if (options->focusPolicyIsReasonable())
            c->workspace()->takeActivity(client, ActivityFocus | ActivityRaise, true);
        c->workspace()->raiseClient(client);
    }
}

void Bridge::closeTab(long id)
{
    if (Client* client = clientForId(id))
        client->closeWindow();
}

void Bridge::closeTabGroup()
{
    if (c->tabGroup())
        c->tabGroup()->closeAll();
}

//END TABBING

KDecoration::WindowOperation Bridge::buttonToWindowOperation(Qt::MouseButtons button)
{
    return c->mouseButtonToWindowOperation(button);
}

} // namespace
