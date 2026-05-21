/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2007 Lubos Lunak <l.lunak@kde.org>
    SPDX-FileCopyrightText: 2007 Christian Nitschkowski <christian.nitschkowski@kdemail.net>
    SPDX-FileCopyrightText: 2026 Xaver Hugl <xaver.hugl@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "thumbnailaside.h"
#include "core/renderviewport.h"
#include "effect/effecthandler.h"
#include "scene/windowitem.h"
#include "scene/workspacescene.h"
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
    KGlobalAccel::self()->setGlobalShortcut(a, QKeySequence(Qt::META | Qt::CTRL | Qt::Key_T));
    connect(a, &QAction::triggered, this, &ThumbnailAsideEffect::toggleCurrentThumbnail);

    connect(effects, &EffectsHandler::windowClosed, this, &ThumbnailAsideEffect::slotWindowClosed);
    connect(effects, &EffectsHandler::screenLockingChanged, this, &ThumbnailAsideEffect::handleScreenLockingChanged);

    reconfigure(ReconfigureAll);
}

void ThumbnailAsideEffect::reconfigure(ReconfigureFlags)
{
    ThumbnailAsideConfig::self()->read();
    m_maxWidth = ThumbnailAsideConfig::maxWidth();
    m_spacing = ThumbnailAsideConfig::spacing();
    m_opacity = ThumbnailAsideConfig::opacity() / 100.0;
    m_screen = ThumbnailAsideConfig::screen(); // Xinerama screen TODO add gui option
    arrange();
}

void ThumbnailAsideEffect::slotWindowFrameGeometryChanged(EffectWindow *w, const RectF &old)
{
    if (w->size() != old.size()) {
        arrange();
    }
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
    if (m_windows.contains(active)) {
        removeThumbnail(active);
    } else {
        addThumbnail(active);
    }
}

void ThumbnailAsideEffect::addThumbnail(EffectWindow *w)
{
    Data d;
    d.index = m_windows.size();
    d.item = std::make_unique<MirrorItem>(w->windowItem(), effects->scene()->overlayItem());
    m_windows[w] = std::move(d);
    connect(w, &EffectWindow::windowFrameGeometryChanged, this, &ThumbnailAsideEffect::slotWindowFrameGeometryChanged);
    arrange();
}

void ThumbnailAsideEffect::removeThumbnail(EffectWindow *w)
{
    if (!m_windows.contains(w)) {
        return;
    }
    int index = m_windows[w].index;
    m_windows.erase(w);
    for (auto &[window, data] : m_windows) {
        if (data.index > index) {
            --data.index;
        }
    }
    arrange();
}

void ThumbnailAsideEffect::arrange()
{
    if (m_windows.size() == 0) {
        return;
    }
    int height = 0;
    QList<int> pos(m_windows.size());
    qreal mwidth = 0;
    for (const auto &[window, data] : std::as_const(m_windows)) {
        height += window->height();
        mwidth = std::max(mwidth, window->width());
        pos[data.index] = window->height();
    }
    LogicalOutput *effectiveScreen = effects->findScreen(m_screen);
    if (!effectiveScreen) {
        effectiveScreen = effects->activeScreen();
    }
    RectF area = effects->clientArea(MaximizeArea, effectiveScreen);
    double scale = area.height() / double(height);
    scale = std::min(scale, m_maxWidth / double(mwidth)); // don't be wider than maxwidth pixels
    int add = 0;
    for (size_t i = 0; i < m_windows.size(); i++) {
        pos[i] = int(pos[i] * scale);
        pos[i] += m_spacing + add; // compute offset of each item
        add = pos[i];
    }
    for (auto &[window, data] : m_windows) {
        int width = int(window->width() * scale);
        const RectF rect(QPointF(area.right() - width - m_spacing, area.bottom() - pos[data.index]), window->size());
        data.item->setGeometry(rect);
        data.item->setOpacity(m_opacity);
        QTransform transform;
        transform.scale(scale, scale);
        data.item->setTransform(transform);
    }
}

void ThumbnailAsideEffect::handleScreenLockingChanged()
{
    for (auto &[window, data] : m_windows) {
        data.item->setVisible(!effects->isScreenLocked());
    }
}

bool ThumbnailAsideEffect::isActive() const
{
    return !m_windows.empty() && !effects->isScreenLocked();
}

} // namespace

#include "moc_thumbnailaside.cpp"
