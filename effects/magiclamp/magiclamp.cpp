/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2008 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

// based on minimize animation by Rivo Laks <rivolaks@hot.ee>

#include "magiclamp.h"
// KConfigSkeleton
#include "magiclampconfig.h"

namespace KWin
{

MagicLampEffect::MagicLampEffect()
{
    initConfig<MagicLampConfig>();
    reconfigure(ReconfigureAll);
    connect(effects, &EffectsHandler::windowDeleted, this, &MagicLampEffect::slotWindowDeleted);
    connect(effects, &EffectsHandler::windowMinimized, this, &MagicLampEffect::slotWindowMinimized);
    connect(effects, &EffectsHandler::windowUnminimized, this, &MagicLampEffect::slotWindowUnminimized);
}

bool MagicLampEffect::supported()
{
    return effects->isOpenGLCompositing() && effects->animationsSupported();
}

void MagicLampEffect::reconfigure(ReconfigureFlags)
{
    MagicLampConfig::self()->read();

    // TODO: Rename animationDuration to duration so we can use
    // animationTime<MagicLampConfig>(250).
    const int d = MagicLampConfig::animationDuration() != 0
        ? MagicLampConfig::animationDuration()
        : 250;
    m_duration = std::chrono::milliseconds(static_cast<int>(animationTime(d)));
}

void MagicLampEffect::prePaintScreen(ScreenPrePaintData& data, int time)
{
    const std::chrono::milliseconds delta(time);

    auto animationIt = m_animations.begin();
    while (animationIt != m_animations.end()) {
        (*animationIt).update(delta);
        ++animationIt;
    }

    // We need to mark the screen windows as transformed. Otherwise the
    // whole screen won't be repainted, resulting in artefacts.
    data.mask |= PAINT_SCREEN_WITH_TRANSFORMED_WINDOWS;

    effects->prePaintScreen(data, time);
}

void MagicLampEffect::prePaintWindow(EffectWindow* w, WindowPrePaintData& data, int time)
{
    // Schedule window for transformation if the animation is still in
    //  progress
    if (m_animations.contains(w)) {
        // We'll transform this window
        data.setTransformed();
        data.quads = data.quads.makeGrid(40);
        w->enablePainting(EffectWindow::PAINT_DISABLED_BY_MINIMIZE);
    }

    effects->prePaintWindow(w, data, time);
}

void MagicLampEffect::paintWindow(EffectWindow* w, int mask, QRegion region, WindowPaintData& data)
{
    auto animationIt = m_animations.constFind(w);
    if (animationIt != m_animations.constEnd()) {
        // 0 = not minimized, 1 = fully minimized
        const qreal progress = (*animationIt).value();

        QRect geo = w->geometry();
        QRect icon = w->iconGeometry();
        IconPosition position = Top;
        // If there's no icon geometry, minimize to the center of the screen
        if (!icon.isValid()) {
            QRect extG = geo;
            QPoint pt = cursorPos();
            // focussing inside the window is no good, leads to ugly artefacts, find nearest border
            if (extG.contains(pt)) {
                const int d[2][2] = { {pt.x() - extG.x(), extG.right()  - pt.x()},
                    {pt.y() - extG.y(), extG.bottom() - pt.y()}
                };
                int di = d[1][0];
                position = Top;
                if (d[0][0] < di) {
                    di = d[0][0];
                    position = Left;
                }
                if (d[1][1] < di) {
                    di = d[1][1];
                    position = Bottom;
                }
                if (d[0][1] < di)
                    position = Right;
                switch(position) {
                case Top: pt.setY(extG.y()); break;
                case Left: pt.setX(extG.x()); break;
                case Bottom: pt.setY(extG.bottom()); break;
                case Right: pt.setX(extG.right()); break;
                }
            } else {
                if (pt.y() < geo.y())
                    position = Top;
                else if (pt.x() < geo.x())
                    position = Left;
                else if (pt.y() > geo.bottom())
                    position = Bottom;
                else if (pt.x() > geo.right())
                    position = Right;
            }
            icon = QRect(pt, QSize(0, 0));
        } else {
            // Assumption: there is a panel containing the icon position
            EffectWindow* panel = nullptr;
            foreach (EffectWindow * window, effects->stackingOrder()) {
                if (!window->isDock())
                    continue;
                // we have to use intersects as there seems to be a Plasma bug
                // the published icon geometry might be bigger than the panel
                if (window->geometry().intersects(icon)) {
                    panel = window;
                    break;
                }
            }
            if (panel) {
                // Assumption: width of horizonal panel is greater than its height and vice versa
                // The panel has to border one screen edge, so get it's screen area
                QRect panelScreen = effects->clientArea(ScreenArea, panel);
                if (panel->width() >= panel->height()) {
                    // horizontal panel
                    if (panel->y() <= panelScreen.height()/2)
                        position = Top;
                    else
                        position = Bottom;
                } else {
                    // vertical panel
                    if (panel->x() <= panelScreen.width()/2)
                        position = Left;
                    else
                        position = Right;
                }
            } else {
                // we did not find a panel, so it might be autohidden
                QRect iconScreen = effects->clientArea(ScreenArea, icon.topLeft(), effects->currentDesktop());
                // as the icon geometry could be overlap a screen edge we use an intersection
                QRect rect = iconScreen.intersected(icon);
                // here we need a different assumption: icon geometry borders one screen edge
                // this assumption might be wrong for e.g. task applet being the only applet in panel
                // in this case the icon borders two screen edges
                // there might be a wrong animation, but not distorted
                if (rect.x() == iconScreen.x()) {
                    position = Left;
                } else if (rect.x() + rect.width() == iconScreen.x() + iconScreen.width()) {
                    position = Right;
                } else if (rect.y() == iconScreen.y()) {
                    position = Top;
                } else {
                    position = Bottom;
                }
            }
        }

#define SANITIZE_PROGRESS   if (p_progress[0] < 0)\
                                p_progress[0] = -p_progress[0];\
                            if (p_progress[1] < 0)\
                                p_progress[1] = -p_progress[1]
#define SET_QUADS(_SET_A_, _A_, _DA_, _SET_B_, _B_, _O0_, _O1_, _O2_, _O3_) quad[0]._SET_A_((icon._A_() + icon._DA_()*(quad[0]._A_() / geo._DA_()) - (quad[0]._A_() + geo._A_()))*p_progress[_O0_] + quad[0]._A_());\
                                                                            quad[1]._SET_A_((icon._A_() + icon._DA_()*(quad[1]._A_() / geo._DA_()) - (quad[1]._A_() + geo._A_()))*p_progress[_O1_] + quad[1]._A_());\
                                                                            quad[2]._SET_A_((icon._A_() + icon._DA_()*(quad[2]._A_() / geo._DA_()) - (quad[2]._A_() + geo._A_()))*p_progress[_O2_] + quad[2]._A_());\
                                                                            quad[3]._SET_A_((icon._A_() + icon._DA_()*(quad[3]._A_() / geo._DA_()) - (quad[3]._A_() + geo._A_()))*p_progress[_O3_] + quad[3]._A_());\
                                                                            \
                                                                            quad[0]._SET_B_(quad[0]._B_() + offset[_O0_]);\
                                                                            quad[1]._SET_B_(quad[1]._B_() + offset[_O1_]);\
                                                                            quad[2]._SET_B_(quad[2]._B_() + offset[_O2_]);\
                                                                            quad[3]._SET_B_(quad[3]._B_() + offset[_O3_])

        WindowQuadList newQuads;
        newQuads.reserve(data.quads.count());
        float quadFactor;   // defines how fast a quad is vertically moved: y coordinates near to window top are slowed down
                            // it is used as quadFactor^3/windowHeight^3
                            // quadFactor is the y position of the quad but is changed towards becomming the window height
                            // by that the factor becomes 1 and has no influence any more
        float offset[2] = {0,0};    // how far has a quad to be moved? Distance between icon and window multiplied by the progress and by the quadFactor
        float p_progress[2] = {0,0};  // the factor which defines how far the x values have to be changed
                            // factor is the current moved y value diveded by the distance between icon and window
        WindowQuad lastQuad(WindowQuadError);
        lastQuad[0].setX(-1);
        lastQuad[0].setY(-1);
        lastQuad[1].setX(-1);
        lastQuad[1].setY(-1);
        lastQuad[2].setX(-1);
        lastQuad[2].setY(-1);

        if (position == Bottom) {
            float height_cube = float(geo.height()) * float(geo.height()) * float(geo.height());
            foreach (WindowQuad quad, data.quads) { // krazy:exclude=foreach

                if (quad[0].y() != lastQuad[0].y() || quad[2].y() != lastQuad[2].y()) {
                    quadFactor = quad[0].y() + (geo.height() - quad[0].y()) * progress;
                    offset[0] = (icon.y() + quad[0].y() - geo.y()) * progress * ((quadFactor * quadFactor * quadFactor) / height_cube);
                    quadFactor = quad[2].y() + (geo.height() - quad[2].y()) * progress;
                    offset[1] = (icon.y() + quad[2].y() - geo.y()) * progress * ((quadFactor * quadFactor * quadFactor) / height_cube);
                    p_progress[1] = qMin(offset[1] / (icon.y() + icon.height() - geo.y() - float(quad[2].y())), 1.0f);
                    p_progress[0] = qMin(offset[0] / (icon.y() + icon.height() - geo.y() - float(quad[0].y())), 1.0f);
                } else
                    lastQuad = quad;

                SANITIZE_PROGRESS;
                // x values are moved towards the center of the icon
                SET_QUADS(setX, x, width, setY, y, 0,0,1,1);

                newQuads.append(quad);
            }
        } else if (position == Top) {
            float height_cube = float(geo.height()) * float(geo.height()) * float(geo.height());
            foreach (WindowQuad quad, data.quads) { // krazy:exclude=foreach

                if (quad[0].y() != lastQuad[0].y() || quad[2].y() != lastQuad[2].y()) {
                    quadFactor = geo.height() - quad[0].y() + (quad[0].y()) * progress;
                    offset[0] = (geo.y() - icon.height() + geo.height() + quad[0].y() - icon.y()) * progress * ((quadFactor * quadFactor * quadFactor) / height_cube);
                    quadFactor = geo.height() - quad[2].y() + (quad[2].y()) * progress;
                    offset[1] = (geo.y() - icon.height() + geo.height() + quad[2].y() - icon.y()) * progress * ((quadFactor * quadFactor * quadFactor) / height_cube);
                    p_progress[0] = qMin(offset[0] / (geo.y() - icon.height() + geo.height() - icon.y() - float(geo.height() - quad[0].y())), 1.0f);
                    p_progress[1] = qMin(offset[1] / (geo.y() - icon.height() + geo.height() - icon.y() - float(geo.height() - quad[2].y())), 1.0f);
                } else
                    lastQuad = quad;

                offset[0] = -offset[0];
                offset[1] = -offset[1];

                SANITIZE_PROGRESS;
                // x values are moved towards the center of the icon
                SET_QUADS(setX, x, width, setY, y, 0,0,1,1);

                newQuads.append(quad);
            }
        } else if (position == Left) {
            float width_cube = float(geo.width()) * float(geo.width()) * float(geo.width());
            foreach (WindowQuad quad, data.quads) { // krazy:exclude=foreach

                if (quad[0].x() != lastQuad[0].x() || quad[1].x() != lastQuad[1].x()) {
                    quadFactor = geo.width() - quad[0].x() + (quad[0].x()) * progress;
                    offset[0] = (geo.x() - icon.width() + geo.width() + quad[0].x() - icon.x()) * progress * ((quadFactor * quadFactor * quadFactor) / width_cube);
                    quadFactor = geo.width() - quad[1].x() + (quad[1].x()) * progress;
                    offset[1] = (geo.x() - icon.width() + geo.width() + quad[1].x() - icon.x()) * progress * ((quadFactor * quadFactor * quadFactor) / width_cube);
                    p_progress[0] = qMin(offset[0] / (geo.x() - icon.width() + geo.width() - icon.x() - float(geo.width() - quad[0].x())), 1.0f);
                    p_progress[1] = qMin(offset[1] / (geo.x() - icon.width() + geo.width() - icon.x() - float(geo.width() - quad[1].x())), 1.0f);
                } else
                    lastQuad = quad;

                offset[0] = -offset[0];
                offset[1] = -offset[1];

                SANITIZE_PROGRESS;
                // y values are moved towards the center of the icon
                SET_QUADS(setY, y, height, setX, x, 0,1,1,0);

                newQuads.append(quad);
            }
        } else if (position == Right) {
            float width_cube = float(geo.width()) * float(geo.width()) * float(geo.width());
            foreach (WindowQuad quad, data.quads) { // krazy:exclude=foreach

                if (quad[0].x() != lastQuad[0].x() || quad[1].x() != lastQuad[1].x()) {
                    quadFactor = quad[0].x() + (geo.width() - quad[0].x()) * progress;
                    offset[0] = (icon.x() + quad[0].x() - geo.x()) * progress * ((quadFactor * quadFactor * quadFactor) / width_cube);
                    quadFactor = quad[1].x() + (geo.width() - quad[1].x()) * progress;
                    offset[1] = (icon.x() + quad[1].x() - geo.x()) * progress * ((quadFactor * quadFactor * quadFactor) / width_cube);
                    p_progress[0] = qMin(offset[0] / (icon.x() + icon.width() - geo.x() - float(quad[0].x())), 1.0f);
                    p_progress[1] = qMin(offset[1] / (icon.x() + icon.width() - geo.x() - float(quad[1].x())), 1.0f);
                } else
                    lastQuad = quad;

                SANITIZE_PROGRESS;
                // y values are moved towards the center of the icon
                SET_QUADS(setY, y, height, setX, x, 0,1,1,0);

                newQuads.append(quad);
            }
        }
        data.quads = newQuads;
    }

    // Call the next effect.
    effects->paintWindow(w, mask, region, data);
}

void MagicLampEffect::postPaintScreen()
{
    auto animationIt = m_animations.begin();
    while (animationIt != m_animations.end()) {
        if ((*animationIt).done()) {
            animationIt = m_animations.erase(animationIt);
        } else {
            ++animationIt;
        }
    }

    effects->addRepaintFull();

    // Call the next effect.
    effects->postPaintScreen();
}

void MagicLampEffect::slotWindowDeleted(EffectWindow* w)
{
    m_animations.remove(w);
}

void MagicLampEffect::slotWindowMinimized(EffectWindow* w)
{
    if (effects->activeFullScreenEffect())
        return;

    TimeLine &timeLine = m_animations[w];

    if (timeLine.running()) {
        timeLine.toggleDirection();
    } else {
        timeLine.setDirection(TimeLine::Forward);
        timeLine.setDuration(m_duration);
        timeLine.setEasingCurve(QEasingCurve::Linear);
    }

    effects->addRepaintFull();
}

void MagicLampEffect::slotWindowUnminimized(EffectWindow* w)
{
    if (effects->activeFullScreenEffect())
        return;

    TimeLine &timeLine = m_animations[w];

    if (timeLine.running()) {
        timeLine.toggleDirection();
    } else {
        timeLine.setDirection(TimeLine::Backward);
        timeLine.setDuration(m_duration);
        timeLine.setEasingCurve(QEasingCurve::Linear);
    }

    effects->addRepaintFull();
}

bool MagicLampEffect::isActive() const
{
    return !m_animations.isEmpty();
}

} // namespace
