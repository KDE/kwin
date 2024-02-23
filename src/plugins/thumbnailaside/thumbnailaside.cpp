/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2007 Lubos Lunak <l.lunak@kde.org>
    SPDX-FileCopyrightText: 2007 Christian Nitschkowski <christian.nitschkowski@kdemail.net>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "thumbnailaside.h"
#include "core/renderviewport.h"
#include "effect/effecthandler.h"
// KConfigSkeleton
#include "thumbnailasideconfig.h"

#include <KGlobalAccel>
#include <KLocalizedString>

#include <QAction>
#include <QMatrix4x4>

namespace KWin
{

ThumbnailAsideEffect::ThumbnailAsideEffect()
{
    ThumbnailAsideConfig::instance(effects->config());
    QAction *a = new QAction(this);
    a->setObjectName(QStringLiteral("ToggleCurrentThumbnail"));
    a->setText(i18n("Toggle Thumbnail for Current Window"));
    KGlobalAccel::self()->setDefaultShortcut(a, QList<QKeySequence>() << (Qt::META | Qt::CTRL | Qt::Key_T));
    KGlobalAccel::self()->setShortcut(a, QList<QKeySequence>() << (Qt::META | Qt::CTRL | Qt::Key_T));
    connect(a, &QAction::triggered, this, &ThumbnailAsideEffect::toggleCurrentThumbnail);

    connect(effects, &EffectsHandler::windowAdded, this, &ThumbnailAsideEffect::slotWindowAdded);
    connect(effects, &EffectsHandler::windowClosed, this, &ThumbnailAsideEffect::slotWindowClosed);
    connect(effects, &EffectsHandler::screenLockingChanged, this, &ThumbnailAsideEffect::repaintAll);

    const auto windows = effects->stackingOrder();
    for (EffectWindow *window : windows) {
        slotWindowAdded(window);
    }

    reconfigure(ReconfigureAll);
}

void ThumbnailAsideEffect::reconfigure(ReconfigureFlags)
{
    ThumbnailAsideConfig::self()->read();
    maxwidth = ThumbnailAsideConfig::maxWidth();
    spacing = ThumbnailAsideConfig::spacing();
    opacity = ThumbnailAsideConfig::opacity() / 100.0;
    screen = ThumbnailAsideConfig::screen(); // Xinerama screen TODO add gui option
    arrange();
}

void ThumbnailAsideEffect::paintScreen(const RenderTarget &renderTarget, const RenderViewport &viewport, int mask, const QRegion &region, Output *screen)
{
    painted = QRegion();
    effects->paintScreen(renderTarget, viewport, mask, region, screen);

    for (const Data &d : std::as_const(windows)) {
        if (painted.intersects(d.rect)) {
            WindowPaintData data;
            data.multiplyOpacity(opacity);
            QRect region;
            setPositionTransformations(data, region, d.window, d.rect, Qt::KeepAspectRatio);
            effects->drawWindow(renderTarget, viewport, d.window, PAINT_WINDOW_OPAQUE | PAINT_WINDOW_TRANSLUCENT | PAINT_WINDOW_TRANSFORMED, region, data);
        }
    }
}

void ThumbnailAsideEffect::paintWindow(const RenderTarget &renderTarget, const RenderViewport &viewport, EffectWindow *w, int mask, QRegion region, WindowPaintData &data)
{
    effects->paintWindow(renderTarget, viewport, w, mask, region, data);
    painted += region;
}

void ThumbnailAsideEffect::slotWindowDamaged(EffectWindow *w)
{
    for (const Data &d : std::as_const(windows)) {
        if (d.window == w) {
            effects->addRepaint(d.rect);
        }
    }
}

void ThumbnailAsideEffect::slotWindowFrameGeometryChanged(EffectWindow *w, const QRectF &old)
{
    for (const Data &d : std::as_const(windows)) {
        if (d.window == w) {
            if (w->size() == old.size()) {
                effects->addRepaint(d.rect);
            } else {
                arrange();
            }
            return;
        }
    }
}

void ThumbnailAsideEffect::slotWindowAdded(EffectWindow *w)
{
    connect(w, &EffectWindow::windowFrameGeometryChanged, this, &ThumbnailAsideEffect::slotWindowFrameGeometryChanged);
    connect(w, &EffectWindow::windowDamaged, this, &ThumbnailAsideEffect::slotWindowDamaged);
}

void ThumbnailAsideEffect::slotWindowClosed(EffectWindow *w)
{
    removeThumbnail(w);
}

void ThumbnailAsideEffect::toggleCurrentThumbnail()
{
    EffectWindow *active = effects->activeWindow();
    if (active == nullptr) {
        return;
    }
    if (windows.contains(active)) {
        removeThumbnail(active);
    } else {
        addThumbnail(active);
    }
}

void ThumbnailAsideEffect::addThumbnail(EffectWindow *w)
{
    repaintAll(); // repaint old areas
    Data d;
    d.window = w;
    d.index = windows.count();
    windows[w] = d;
    arrange();
}

void ThumbnailAsideEffect::removeThumbnail(EffectWindow *w)
{
    if (!windows.contains(w)) {
        return;
    }
    repaintAll(); // repaint old areas
    int index = windows[w].index;
    windows.remove(w);
    for (QHash<EffectWindow *, Data>::Iterator it = windows.begin();
         it != windows.end();
         ++it) {
        Data &d = *it;
        if (d.index > index) {
            --d.index;
        }
    }
    arrange();
}

void ThumbnailAsideEffect::arrange()
{
    if (windows.size() == 0) {
        return;
    }
    int height = 0;
    QList<int> pos(windows.size());
    qreal mwidth = 0;
    for (const Data &d : std::as_const(windows)) {
        height += d.window->height();
        mwidth = std::max(mwidth, d.window->width());
        pos[d.index] = d.window->height();
    }
    Output *effectiveScreen = effects->findScreen(screen);
    if (!effectiveScreen) {
        effectiveScreen = effects->activeScreen();
    }
    QRectF area = effects->clientArea(MaximizeArea, effectiveScreen, effects->currentDesktop());
    double scale = area.height() / double(height);
    scale = std::min(scale, maxwidth / double(mwidth)); // don't be wider than maxwidth pixels
    int add = 0;
    for (int i = 0;
         i < windows.size();
         ++i) {
        pos[i] = int(pos[i] * scale);
        pos[i] += spacing + add; // compute offset of each item
        add = pos[i];
    }
    for (QHash<EffectWindow *, Data>::Iterator it = windows.begin();
         it != windows.end();
         ++it) {
        Data &d = *it;
        int width = int(d.window->width() * scale);
        d.rect = QRect(area.right() - width, area.bottom() - pos[d.index], width, int(d.window->height() * scale));
    }
    repaintAll();
}

void ThumbnailAsideEffect::repaintAll()
{
    for (const Data &d : std::as_const(windows)) {
        effects->addRepaint(d.rect);
    }
}

bool ThumbnailAsideEffect::isActive() const
{
    return !windows.isEmpty() && !effects->isScreenLocked();
}

} // namespace

#include "moc_thumbnailaside.cpp"
