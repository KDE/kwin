/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2009 Nikhil Marathe <nsm.nikhil@gmail.com>
Copyright (C) 2011 Arthur Arlt <a.arlt@stud.uni-heidelberg.de>

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

#include <tiling/tiling.h>

#include <klocale.h>
#include <knotification.h>
#include <kwindowinfo.h>
#include <kwindowsystem.h>

#include <KActionCollection>
#include <KDE/KAction>

#include "tiling/tile.h"
#include "tiling/tilinglayout.h"
#include "tilinglayoutfactory.h"
#include "workspace.h"

namespace KWin {
Tiling::Tiling(KWin::Workspace* w)
    : QObject(w)
    , m_workspace(w)
    , m_enabled(false)
{
}

Tiling::~Tiling()
{
}

void Tiling::initShortcuts(KActionCollection* keys){
    KAction *a = NULL;
    #define KEY( name, key, fnSlot )                                    \
    a = keys->addAction( name );                                        \
    a->setText( i18n( name ) );                                         \
    qobject_cast<KAction*>( a )->setGlobalShortcut(KShortcut(key));     \
    connect(a, SIGNAL(triggered(bool)), SLOT(fnSlot));

    a = keys->addAction("Group:Tiling");
    a->setText(i18n("Tiling"));
    KEY(I18N_NOOP("Enable/Disable Tiling"),                    Qt::SHIFT + Qt::ALT + Qt::Key_F11, slotToggleTiling());
    KEY(I18N_NOOP("Toggle Floating"),              Qt::META + Qt::Key_F, slotToggleFloating());

    KEY(I18N_NOOP("Switch Focus Left") ,   Qt::META + Qt::Key_H, slotFocusTileLeft());
    KEY(I18N_NOOP("Switch Focus Right") ,   Qt::META + Qt::Key_L, slotFocusTileRight());
    KEY(I18N_NOOP("Switch Focus Up") ,   Qt::META + Qt::Key_K, slotFocusTileTop());
    KEY(I18N_NOOP("Switch Focus Down") ,   Qt::META + Qt::Key_J, slotFocusTileBottom());
    KEY(I18N_NOOP("Move Window Left") ,   Qt::SHIFT + Qt::META + Qt::Key_H, slotMoveTileLeft());
    KEY(I18N_NOOP("Move Window Right") ,   Qt::SHIFT + Qt::META + Qt::Key_L, slotMoveTileRight());
    KEY(I18N_NOOP("Move Window Up") ,   Qt::SHIFT + Qt::META + Qt::Key_K, slotMoveTileTop());
    KEY(I18N_NOOP("Move Window Down") ,   Qt::SHIFT + Qt::META + Qt::Key_J, slotMoveTileBottom());
    KEY(I18N_NOOP("Next Layout"), Qt::META + Qt::Key_PageDown, slotNextTileLayout());
    KEY(I18N_NOOP("Previous Layout"), Qt::META + Qt::Key_PageUp, slotPreviousTileLayout());

}

bool Tiling::isEnabled() const
{
    return m_enabled;
}

void Tiling::setEnabled(bool tiling)
{
    if (isEnabled() == tiling) return;

    m_enabled = tiling;

    KSharedConfig::Ptr _config = KGlobal::config();
    KConfigGroup config(_config, "Windows");
    config.writeEntry("TilingOn", m_enabled);
    config.sync();
    options->tilingOn = m_enabled;
    options->tilingLayout = static_cast<TilingLayoutFactory::Layouts>(config.readEntry("TilingDefaultLayout", 0));
    options->tilingRaisePolicy = config.readEntry("TilingRaisePolicy", 0);

    if (m_enabled) {
        connect(m_workspace, SIGNAL(clientAdded(KWin::Client*)), this, SLOT(createTile(KWin::Client*)));
        connect(m_workspace, SIGNAL(clientAdded(KWin::Client*)), this, SLOT(slotResizeTilingLayouts()));
        connect(m_workspace, SIGNAL(numberDesktopsChanged(int)), this, SLOT(slotResizeTilingLayouts()));
        connect(m_workspace, SIGNAL(clientRemoved(KWin::Client*)), this, SLOT(removeTile(KWin::Client*)));
        connect(m_workspace, SIGNAL(clientActivated(KWin::Client*)), this, SLOT(notifyTilingWindowActivated(KWin::Client*)));
        m_tilingLayouts.resize(Workspace::self()->numberOfDesktops() + 1);
        foreach (Client * c, Workspace::self()->stackingOrder()) {
            createTile(c);
        }
    } else {
        disconnect(m_workspace, SIGNAL(clientAdded(KWin::Client*)));
        disconnect(m_workspace, SIGNAL(numberDesktopsChanged(int)));
        disconnect(m_workspace, SIGNAL(clientRemoved(KWin::Client*)));
        qDeleteAll(m_tilingLayouts);
        m_tilingLayouts.clear();
    }
}

void Tiling::slotToggleTiling()
{
    if (isEnabled()) {
        setEnabled(false);
        QString message = i18n("Tiling Disabled");
        KNotification::event("tilingdisabled", message, QPixmap(), NULL, KNotification::CloseOnTimeout, KComponentData("kwin"));
    } else {
        setEnabled(true);
        QString message = i18n("Tiling Enabled");
        KNotification::event("tilingenabled", message, QPixmap(), NULL, KNotification::CloseOnTimeout, KComponentData("kwin"));
    }
}

void Tiling::createTile(Client* c)
{
    if (c == NULL)
        return;

    if (c->desktop() < 0 || c->desktop() >= m_tilingLayouts.size()) return;

    kDebug(1212) << "Now tiling " << c->caption();
    if (!isEnabled() || !tileable(c))
        return;

    Tile *t = new Tile(c, Workspace::self()->clientArea(PlacementArea, c));
    if (!tileable(c)) {
        kDebug(1212) << c->caption() << "is not tileable";
        t->floatTile();
    }

    if (!m_tilingLayouts.value(c->desktop())) {
        m_tilingLayouts[c->desktop()] = TilingLayoutFactory::createLayout(TilingLayoutFactory::DefaultLayout, m_workspace);
        m_tilingLayouts[c->desktop()]->setParent(this);
    }
    m_tilingLayouts[c->desktop()]->addTile(t);
    m_tilingLayouts[c->desktop()]->commit();
    // if tiling is activated, connect to Client's signals and react with rearrangement when (un)minimizing
    connect(c, SIGNAL(clientMinimized(KWin::Client*,bool)), this, SLOT(notifyTilingWindowMinimizeToggled(KWin::Client*)));
    connect(c, SIGNAL(clientUnminimized(KWin::Client*,bool)), this, SLOT(notifyTilingWindowMinimizeToggled(KWin::Client*)));
    connect(c, SIGNAL(clientUnminimized(KWin::Client*,bool)), this, SLOT(updateAllTiles()));
}

void Tiling::removeTile(Client *c)
{
    if (!m_tilingLayouts.value(c->desktop())) {
        return;
    }
    if (m_tilingLayouts[ c->desktop()])
        m_tilingLayouts[ c->desktop()]->removeTile(c);
}

bool Tiling::tileable(Client* c)
{
    kDebug(1212) << c->caption();
    KWindowInfo info = KWindowSystem::windowInfo(c->window(), -1U, NET::WM2WindowClass);
    kDebug(1212) << "WINDOW CLASS IS " << info.windowClassClass();
    if (info.windowClassClass() == "Plasma-desktop") {
        return false;
    }
    // TODO: if application specific settings
    // to ignore, put them here

    if (!c->isNormalWindow()) {
        return false;
    }

    // 0 means tile it, if we get 1 (floating), don't tile
    if (c->rules()->checkTilingOption(0) == 1) {
        return false;
    }

    kDebug() << "Tiling" << c;
    return true;
}

void Tiling::belowCursor()
{
    // TODO ... "WHAT?" remove? What's a parameterless void function supposed to do?
}

Tile* Tiling::getNiceTile() const
{
    if (!isEnabled()) return NULL;
    if (!m_workspace->activeClient()) return NULL;
    if (!m_tilingLayouts.value(m_workspace->activeClient()->desktop())) return NULL;

    return m_tilingLayouts[ m_workspace->activeClient()->desktop()]->findTile(m_workspace->activeClient());
    // TODO ... WHAT?
}

void Tiling::updateAllTiles()
{
    foreach (TilingLayout * t, m_tilingLayouts) {
        if (!t) continue;
        t->commit();
    }
}

/*
 * Resize the neighbouring clients to close any gaps
 */
void Tiling::notifyTilingWindowResize(Client *c, const QRect &moveResizeGeom, const QRect &orig)
{
    if (m_tilingLayouts.value(c->desktop()) == NULL)
        return;
    m_tilingLayouts[ c->desktop()]->clientResized(c, moveResizeGeom, orig);
}

void Tiling::notifyTilingWindowMove(Client *c, const QRect &moveResizeGeom, const QRect &orig)
{
    if (m_tilingLayouts.value(c->desktop()) == NULL) {
        return;
    }
    m_tilingLayouts[ c->desktop()]->clientMoved(c, moveResizeGeom, orig);
    updateAllTiles();
}

void Tiling::notifyTilingWindowResizeDone(Client *c, const QRect &moveResizeGeom, const QRect &orig, bool canceled)
{
    if (canceled)
        notifyTilingWindowResize(c, orig, moveResizeGeom);
    else
        notifyTilingWindowResize(c, moveResizeGeom, orig);
}

void Tiling::notifyTilingWindowMoveDone(Client *c, const QRect &moveResizeGeom, const QRect &orig, bool canceled)
{
    if (canceled)
        notifyTilingWindowMove(c, orig, moveResizeGeom);
    else
        notifyTilingWindowMove(c, moveResizeGeom, orig);
}

void Tiling::notifyTilingWindowDesktopChanged(Client *c, int old_desktop)
{
    if (c->desktop() < 1 || c->desktop() > m_workspace->numberOfDesktops())
        return;

    if (m_tilingLayouts.value(old_desktop)) {
        Tile *t = m_tilingLayouts[ old_desktop ]->findTile(c);

        // TODO: copied from createTile(), move this into separate method?
        if (!m_tilingLayouts.value(c->desktop())) {
            m_tilingLayouts[c->desktop()] = TilingLayoutFactory::createLayout(TilingLayoutFactory::DefaultLayout, m_workspace);
        }

        if (t)
            m_tilingLayouts[ c->desktop()]->addTile(t);

        m_tilingLayouts[ old_desktop ]->removeTile(c);
        m_tilingLayouts[ old_desktop ]->commit();
    }
}

/*
 * Implements the 3 raising modes in Window Behaviour -> Advanced
 */
void Tiling::notifyTilingWindowActivated(KWin::Client *c)
{
    if (c == NULL)
        return;

    if (options->tilingRaisePolicy == 1)   // individual raise/lowers
        return;

    if (m_tilingLayouts.value(c->desktop())) {
        QList<Tile *> tiles = m_tilingLayouts[ c->desktop()]->tiles();

        StackingUpdatesBlocker blocker(m_workspace);

        Tile *tile_to_raise = m_tilingLayouts[ c->desktop()]->findTile(c);

        if (!tile_to_raise) {
            return;
        }

        kDebug(1212) << "FOUND TILE";
        bool raise_floating = false;
        if (options->tilingRaisePolicy == 2)   // floating always on top
            raise_floating = true;
        else
            raise_floating = tile_to_raise->floating();

        foreach (Tile * t, tiles) {
            if (t->floating() == raise_floating && t != tile_to_raise)
                m_workspace->raiseClient(t->client());
        }
        // raise the current tile last so that it ends up on top
        // but only if it supposed to be raised, required to support tilingRaisePolicy
        kDebug(1212) << "Raise floating? " << raise_floating << "to raise is floating?" << tile_to_raise->floating();
        if (tile_to_raise->floating() == raise_floating)
            m_workspace->raiseClient(tile_to_raise->client());
    }
}

void Tiling::notifyTilingWindowMinimizeToggled(KWin::Client* c)
{
    if (m_tilingLayouts.value(c->desktop())) {
        m_tilingLayouts[ c->desktop()]->clientMinimizeToggled(c);
    }
}

void Tiling::notifyTilingWindowMaximized(Client *c, Options::WindowOperation op)
{
    if (m_tilingLayouts.value(c->desktop())) {
        Tile *t = m_tilingLayouts[ c->desktop()]->findTile(c);
        if (!t) {
            createTile(c);
            t = m_tilingLayouts[ c->desktop()]->findTile(c);

            // if still no tile, it couldn't be tiled
            // so ignore it
            if (!t)
                return;
        }

        // if window IS tiled and a maximize
        // is attempted, make the window float.
        // That is all we do since that can
        // mess up the layout.
        // In all other cases, don't do
        // anything, let the user manage toggling
        // using Meta+F
        if (!t->floating()
                && (op == Options::MaximizeOp
                    || op == Options::HMaximizeOp
                    || op == Options::VMaximizeOp)) {
            m_tilingLayouts[ c->desktop()]->toggleFloatTile(c);
        }

    }
}

Tile* Tiling::findAdjacentTile(Tile *ref, int d)
{
    QRect reference = ref->geometry();
    QPoint origin = reference.center();

    Tile *closest = NULL;
    int minDist = -1;

    QList<Tile *> tiles = m_tilingLayouts[ ref->client()->desktop()]->tiles();

    foreach (Tile * t, tiles) {
        if (t->client() == ref->client() || t->ignoreGeometry())
            continue;

        bool consider = false;

        QRect other = t->geometry();
        QPoint otherCenter = other.center();

        switch(d) {
        case Tile::Top:
            consider = otherCenter.y() < origin.y()
                       && other.bottom() < reference.top();
            break;

        case Tile::Right:
            consider = otherCenter.x() > origin.x()
                       && other.left() > reference.right();
            break;

        case Tile::Bottom:
            consider = otherCenter.y() > origin.y()
                       && other.top() > reference.bottom();
            break;

        case Tile::Left:
            consider = otherCenter.x() < origin.x()
                       && other.right() < reference.left();
            break;

        default:
            abort();
        }

        if (consider) {
            int dist = (otherCenter - origin).manhattanLength();
            if (minDist > dist || minDist < 0) {
                minDist = dist;
                closest = t;
            }
        }
    }
    return closest;
}

void Tiling::focusTile(int d)
{
    Tile *t = getNiceTile();
    if (t) {
        Tile *adj = findAdjacentTile(t, d);
        if (adj)
            m_workspace->activateClient(adj->client());
    }
}

void Tiling::moveTile(int d)
{
    Tile *t = getNiceTile();
    if (t) {
        Tile* adj = findAdjacentTile(t, d);

        m_tilingLayouts[ t->client()->desktop()]->swapTiles(t, adj);
    }
}

void Tiling::slotFocusTileLeft()
{
    focusTile(Tile::Left);
}

void Tiling::slotFocusTileRight()
{
    focusTile(Tile::Right);
}

void Tiling::slotFocusTileTop()
{
    focusTile(Tile::Top);
}

void Tiling::slotFocusTileBottom()
{
    focusTile(Tile::Bottom);
}

void Tiling::slotMoveTileLeft()
{
    moveTile(Tile::Left);
}

void Tiling::slotMoveTileRight()
{
    moveTile(Tile::Right);
}

void Tiling::slotMoveTileTop()
{
    moveTile(Tile::Top);
}

void Tiling::slotMoveTileBottom()
{
    moveTile(Tile::Bottom);
}

void Tiling::slotToggleFloating()
{
    Client *c = m_workspace->activeClient();
    if (!c)
        return;
    if (m_tilingLayouts.value(c->desktop())) {
        m_tilingLayouts[ c->desktop()]->toggleFloatTile(c);
    }
}

void Tiling::slotNextTileLayout()
{
    if (m_tilingLayouts.value(m_workspace->currentDesktop())) {

        m_tilingLayouts.replace(m_workspace->currentDesktop(), TilingLayoutFactory::nextLayout(m_tilingLayouts[m_workspace->currentDesktop()]));

        m_tilingLayouts[m_workspace->currentDesktop()]->commit();
    }
}

void Tiling::slotPreviousTileLayout()
{
    if (m_tilingLayouts.value(m_workspace->currentDesktop())) {

        m_tilingLayouts.replace(m_workspace->currentDesktop(), TilingLayoutFactory::previousLayout(m_tilingLayouts[m_workspace->currentDesktop()]));

        m_tilingLayouts[m_workspace->currentDesktop()]->commit();
    }
}

KDecorationDefines::Position Tiling::supportedTilingResizeMode(Client *c, KDecorationDefines::Position currentMode)
{
    if (m_tilingLayouts.value(c->desktop())) {
        return m_tilingLayouts[c->desktop()]->resizeMode(c, currentMode);
    }
    return currentMode;
}

void Tiling::dumpTiles() const
{
    foreach (TilingLayout * t, m_tilingLayouts) {
        if (!t) {
            kDebug(1212) << "EMPTY DESKTOP";
            continue;
        }
        kDebug(1212) << "Desktop" << m_tilingLayouts.indexOf(t);
        foreach (Tile * tile, t->tiles()) {
            tile->dumpTile("--");
        }
    }
}

const QVector< TilingLayout* >& Tiling::tilingLayouts() const
{
    return m_tilingLayouts;
}

void Tiling::slotResizeTilingLayouts()
{
    m_tilingLayouts.resize(m_workspace->numberOfDesktops() + 1);
}

}
