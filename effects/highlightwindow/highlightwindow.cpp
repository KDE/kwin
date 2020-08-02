/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2009 Lucas Murray <lmurray@undefinedfire.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "highlightwindow.h"

namespace KWin
{

HighlightWindowEffect::HighlightWindowEffect()
    : m_finishing(false)
    , m_fadeDuration(float(animationTime(150)))
    , m_monitorWindow(nullptr)
{
    m_atom = effects->announceSupportProperty("_KDE_WINDOW_HIGHLIGHT", this);
    connect(effects, &EffectsHandler::windowAdded, this, &HighlightWindowEffect::slotWindowAdded);
    connect(effects, &EffectsHandler::windowClosed, this, &HighlightWindowEffect::slotWindowClosed);
    connect(effects, &EffectsHandler::windowDeleted, this, &HighlightWindowEffect::slotWindowDeleted);
    connect(effects, &EffectsHandler::propertyNotify, this,
        [this](EffectWindow *w, long atom) {
            slotPropertyNotify(w, atom, nullptr);
        }
    );
    connect(effects, &EffectsHandler::xcbConnectionChanged, this,
        [this] {
            m_atom = effects->announceSupportProperty("_KDE_WINDOW_HIGHLIGHT", this);
        }
    );
}

HighlightWindowEffect::~HighlightWindowEffect()
{
}

static bool isInitiallyHidden(EffectWindow* w)
{
    // Is the window initially hidden until it is highlighted?
    return w->isMinimized() || !w->isOnCurrentDesktop();
}

void HighlightWindowEffect::prePaintWindow(EffectWindow* w, WindowPrePaintData& data, int time)
{
    // Calculate window opacities
    QHash<EffectWindow*, float>::iterator opacity = m_windowOpacity.find(w);
    if (!m_highlightedWindows.isEmpty()) {
        // Initial fade out and changing highlight animation
        if (opacity == m_windowOpacity.end())
            opacity = m_windowOpacity.insert(w, 0.0f);
        float oldOpacity = *opacity;
        if (m_highlightedWindows.contains(w))
            *opacity = qMin(1.0f, oldOpacity + time / m_fadeDuration);
        else if (w->isNormalWindow() || w->isDialog())   // Only fade out windows
            *opacity = qMax(isInitiallyHidden(w) ? 0.0f : 0.15f, oldOpacity - time / m_fadeDuration);

        if (*opacity < 0.98f)
            data.setTranslucent();
        if (oldOpacity != *opacity)
            effects->addRepaint(w->expandedGeometry());
    } else if (m_finishing && m_windowOpacity.contains(w)) {
        // Final fading back in animation
        if (opacity == m_windowOpacity.end())
            opacity = m_windowOpacity.insert(w, 0.0f);
        float oldOpacity = *opacity;
        if (isInitiallyHidden(w))
            *opacity = qMax(0.0f, oldOpacity - time / m_fadeDuration);
        else
            *opacity = qMin(1.0f, oldOpacity + time / m_fadeDuration);

        if (*opacity < 0.98f)
            data.setTranslucent();
        if (oldOpacity != *opacity)
            effects->addRepaint(w->expandedGeometry());

        if (*opacity > 0.98f || *opacity < 0.02f) {
            m_windowOpacity.remove(w);   // We default to 1.0
            opacity = m_windowOpacity.end();
        }
    }

    // Show tabbed windows and windows on other desktops if highlighted
    if (opacity != m_windowOpacity.end() && *opacity > 0.01) {
        if (w->isMinimized())
            w->enablePainting(EffectWindow::PAINT_DISABLED_BY_MINIMIZE);
        if (!w->isOnCurrentDesktop())
            w->enablePainting(EffectWindow::PAINT_DISABLED_BY_DESKTOP);
    }

    effects->prePaintWindow(w, data, time);
}

void HighlightWindowEffect::paintWindow(EffectWindow* w, int mask, QRegion region, WindowPaintData& data)
{
    data.multiplyOpacity(m_windowOpacity.value(w, 1.0f));
    effects->paintWindow(w, mask, region, data);
}

void HighlightWindowEffect::slotWindowAdded(EffectWindow* w)
{
    if (!m_highlightedWindows.isEmpty()) {
        // The effect is activated thus we need to add it to the opacity hash
        foreach (const WId id, m_highlightedIds) {
            if (w == effects->findWindow(id)) {
                m_windowOpacity[w] = 1.0; // this window was demanded to be highlighted before it appeared
                return;
            }
        }
        m_windowOpacity[w] = 0.15; // this window is not currently highlighted
    }
    slotPropertyNotify(w, m_atom, w);   // Check initial value
}

void HighlightWindowEffect::slotWindowClosed(EffectWindow* w)
{
    if (m_monitorWindow == w)   // The monitoring window was destroyed
        finishHighlighting();
}

void HighlightWindowEffect::slotWindowDeleted(EffectWindow* w)
{
    m_windowOpacity.remove(w);
}

void HighlightWindowEffect::slotPropertyNotify(EffectWindow* w, long a, EffectWindow *addedWindow)
{
    if (a != m_atom || m_atom == XCB_ATOM_NONE)
        return; // Not our atom

    // if the window is null, the property was set on the root window - see events.cpp
    QByteArray byteData = w ? w->readProperty(m_atom, m_atom, 32) :
                          effects->readRootProperty(m_atom, m_atom, 32);
    if (byteData.length() < 1) {
        // Property was removed, clearing highlight
        if (!addedWindow || w != addedWindow)
            finishHighlighting();
        return;
    }
    auto* data = reinterpret_cast<uint32_t*>(byteData.data());

    if (!data[0]) {
        // Purposely clearing highlight by issuing a NULL target
        finishHighlighting();
        return;
    }
    m_monitorWindow = w;
    bool found = false;
    int length = byteData.length() / sizeof(data[0]);
    //foreach ( EffectWindow* e, m_highlightedWindows )
    //    effects->setElevatedWindow( e, false );
    m_highlightedWindows.clear();
    m_highlightedIds.clear();
    for (int i = 0; i < length; i++) {
        m_highlightedIds << data[i];
        EffectWindow* foundWin = effects->findWindow(data[i]);
        if (!foundWin) {
            qCDebug(KWINEFFECTS) << "Invalid window targetted for highlight. Requested:" << data[i];
            continue; // might come in later.
        }
        m_highlightedWindows.append(foundWin);
        // TODO: We cannot just simply elevate the window as this will elevate it over
        // Plasma tooltips and other such windows as well
        //effects->setElevatedWindow( foundWin, true );
        found = true;
    }
    if (!found) {
        finishHighlighting();
        return;
    }
    prepareHighlighting();
    if (w)
        m_windowOpacity[w] = 1.0; // Because it's not in stackingOrder() yet

    /* TODO: Finish thumbnails of offscreen windows, not sure if it's worth it though
    if ( !m_highlightedWindow->isOnCurrentDesktop() )
        { // Window is offscreen, determine thumbnail position
        QRect screenArea = effects->clientArea( MaximizeArea ); // Workable area of the active screen
        QRect outerArea = outerArea.adjusted( outerArea.width() / 10, outerArea.height() / 10,
            -outerArea.width() / 10, -outerArea.height() / 10 ); // Add 10% margin around the edge
        QRect innerArea = outerArea.adjusted( outerArea.width() / 40, outerArea.height() / 40,
            -outerArea.width() / 40, -outerArea.height() / 40 ); // Outer edge of the thumbnail border (2.5%)
        QRect thumbArea = outerArea.adjusted( 20, 20, -20, -20 ); // Outer edge of the thumbnail (20px)

        // Determine the maximum size that we can make the thumbnail within the innerArea
        double areaAspect = double( thumbArea.width() ) / double( thumbArea.height() );
        double windowAspect = aspectRatio( m_highlightedWindow );
        QRect thumbRect; // Position doesn't matter right now, but it will later
        if ( windowAspect > areaAspect )
            // Top/bottom will touch first
            thumbRect = QRect( 0, 0, widthForHeight( thumbArea.height() ), thumbArea.height() );
        else // Left/right will touch first
            thumbRect = QRect( 0, 0, thumbArea.width(), heightForWidth( thumbArea.width() ));
        if ( thumbRect.width() >= m_highlightedWindow->width() )
            // Area is larger than the window, just use the window's size
            thumbRect = m_highlightedWindow->geometry();

        // Determine position of desktop relative to the current one
        QPoint direction = effects->desktopGridCoords( m_highlightedWindow->desktop() ) -
            effects->desktopGridCoords( effects->currentDesktop() );

        // Draw a line from the center of the current desktop to the center of the target desktop.
        QPointF desktopLine( 0, 0, direction.x() * screenArea.width(), direction.y() * screenArea.height() );
        desktopLeft.translate( screenArea.width() / 2, screenArea.height() / 2 ); // Move to the screen center

        // Take the point where the line crosses the outerArea, this will be the tip of our arrow
        QPointF arrowTip;
        QLineF testLine( // Top
            outerArea.x(), outerArea.y(),
            outerArea.x() + outerArea.width(), outerArea.y() );
        if ( desktopLine.intersect( testLine, &arrowTip ) != QLineF::BoundedIntersection )
            {
            testLine = QLineF( // Right
                outerArea.x() + outerArea.width(), outerArea.y(),
                outerArea.x() + outerArea.width(), outerArea.y() + outerArea.height() );
            if ( desktopLine.intersect( testLine, &arrowTip ) != QLineF::BoundedIntersection )
                {
                testLine = QLineF( // Bottom
                    outerArea.x() + outerArea.width(), outerArea.y() + outerArea.height(),
                    outerArea.x(), outerArea.y() + outerArea.height() );
                if ( desktopLine.intersect( testLine, &arrowTip ) != QLineF::BoundedIntersection )
                    {
                    testLine = QLineF( // Left
                        outerArea.x(), outerArea.y() + outerArea.height(),
                        outerArea.x(), outerArea.y() );
                    desktopLine.intersect( testLine, &arrowTip ); // Should never fail
                    }
                }
            }
        m_arrowTip = arrowTip.toPoint();
        } */
}

void HighlightWindowEffect::prepareHighlighting()
{
    // Create window data for every window. Just calling [w] creates it.
    m_finishing = false;
    foreach (EffectWindow * w, effects->stackingOrder()) {
        if (!m_windowOpacity.contains(w))    // Just in case we are still finishing from last time
            m_windowOpacity.insert(w, isInitiallyHidden(w) ? 0.0 : 1.0);
        if (!m_highlightedWindows.isEmpty())
            m_highlightedWindows.at(0)->addRepaintFull();
    }
}

void HighlightWindowEffect::finishHighlighting()
{
    m_finishing = true;
    m_monitorWindow = nullptr;
    m_highlightedWindows.clear();
    if (!m_windowOpacity.isEmpty())
        m_windowOpacity.constBegin().key()->addRepaintFull();
}

void HighlightWindowEffect::highlightWindows(const QVector<KWin::EffectWindow *> &windows)
{
    if (windows.isEmpty()) {
        finishHighlighting();
        return;
    }

    m_monitorWindow = nullptr;
    m_highlightedWindows.clear();
    m_highlightedIds.clear();
    for (auto w : windows) {
        m_highlightedWindows << w;
    }
    prepareHighlighting();
}

bool HighlightWindowEffect::isActive() const
{
    return !(m_windowOpacity.isEmpty() || effects->isScreenLocked());
}

bool HighlightWindowEffect::provides(Feature feature)
{
    switch (feature) {
    case HighlightWindows:
        return true;
    default:
        return false;
    }
}

bool HighlightWindowEffect::perform(Feature feature, const QVariantList &arguments)
{
    if (feature != HighlightWindows) {
        return false;
    }
    if (arguments.size() != 1) {
        return false;
    }
    highlightWindows(arguments.first().value<QVector<EffectWindow*>>());
    return true;
}

} // namespace
