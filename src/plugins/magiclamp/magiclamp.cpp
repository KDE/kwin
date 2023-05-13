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

    setVertexSnappingMode(RenderGeometry::VertexSnappingMode::None);
}

bool MagicLampEffect::supported()
{
    return OffscreenEffect::supported() && effects->animationsSupported();
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

void MagicLampEffect::prePaintScreen(ScreenPrePaintData &data, std::chrono::milliseconds presentTime)
{
    auto animationIt = m_animations.begin();
    while (animationIt != m_animations.end()) {
        (*animationIt).timeLine.advance(presentTime);
        ++animationIt;
    }

    // We need to mark the screen windows as transformed. Otherwise the
    // whole screen won't be repainted, resulting in artefacts.
    data.mask |= PAINT_SCREEN_WITH_TRANSFORMED_WINDOWS;

    effects->prePaintScreen(data, presentTime);
}

void MagicLampEffect::prePaintWindow(EffectWindow *w, WindowPrePaintData &data, std::chrono::milliseconds presentTime)
{
    // Schedule window for transformation if the animation is still in
    //  progress
    if (m_animations.contains(w)) {
        // We'll transform this window
        data.setTransformed();
    }

    effects->prePaintWindow(w, data, presentTime);
}

void MagicLampEffect::apply(EffectWindow *w, int mask, WindowPaintData &data, WindowQuadList &quads)
{
    auto animationIt = m_animations.constFind(w);
    if (animationIt != m_animations.constEnd()) {
        // 0 = not minimized, 1 = fully minimized
        const qreal progress = (*animationIt).timeLine.value();

        QRect geo = w->frameGeometry().toRect();
        QRect icon = w->iconGeometry().toRect();
        IconPosition position = Top;
        // If there's no icon geometry, minimize to the center of the screen
        if (!icon.isValid()) {
            QRect extG = geo;
            QPoint pt = cursorPos().toPoint();
            // focussing inside the window is no good, leads to ugly artefacts, find nearest border
            if (extG.contains(pt)) {
                const int d[2][2] = {{pt.x() - extG.x(), extG.right() - pt.x()},
                                     {pt.y() - extG.y(), extG.bottom() - pt.y()}};
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
                if (d[0][1] < di) {
                    position = Right;
                }
                switch (position) {
                case Top:
                    pt.setY(extG.y());
                    break;
                case Left:
                    pt.setX(extG.x());
                    break;
                case Bottom:
                    pt.setY(extG.bottom());
                    break;
                case Right:
                    pt.setX(extG.right());
                    break;
                }
            } else {
                if (pt.y() < geo.y()) {
                    position = Top;
                } else if (pt.x() < geo.x()) {
                    position = Left;
                } else if (pt.y() > geo.bottom()) {
                    position = Bottom;
                } else if (pt.x() > geo.right()) {
                    position = Right;
                }
            }
            icon = QRect(pt, QSize(0, 0));
        } else {
            // Assumption: there is a panel containing the icon position
            EffectWindow *panel = nullptr;
            const auto stackingOrder = effects->stackingOrder();
            for (EffectWindow *window : stackingOrder) {
                if (!window->isDock()) {
                    continue;
                }
                // we have to use intersects as there seems to be a Plasma bug
                // the published icon geometry might be bigger than the panel
                if (window->frameGeometry().intersects(icon)) {
                    panel = window;
                    break;
                }
            }
            if (panel) {
                // Assumption: width of horizonal panel is greater than its height and vice versa
                const QRectF windowScreen = effects->clientArea(ScreenArea, w);

                if (panel->width() >= panel->height()) {
                    // horizontal panel
                    if (icon.center().y() <= windowScreen.center().y()) {
                        position = Top;
                    } else {
                        position = Bottom;
                    }
                } else {
                    // vertical panel
                    if (icon.center().x() <= windowScreen.center().x()) {
                        position = Left;
                    } else {
                        position = Right;
                    }
                }
            } else {
                // we did not find a panel, so it might be autohidden
                QRectF iconScreen = effects->clientArea(ScreenArea, icon.topLeft(), effects->currentDesktop());
                // as the icon geometry could be overlap a screen edge we use an intersection
                QRectF rect = iconScreen.intersected(icon);
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

#define SET_QUADS(_SET_A_, _A_, _DA_, _SET_B_, _B_, _O0_, _O1_, _O2_, _O3_)                                                                      \
    quad[0]._SET_A_((icon._A_() + icon._DA_() * (quad[0]._A_() / geo._DA_()) - (quad[0]._A_() + geo._A_())) * p_progress[_O0_] + quad[0]._A_()); \
    quad[1]._SET_A_((icon._A_() + icon._DA_() * (quad[1]._A_() / geo._DA_()) - (quad[1]._A_() + geo._A_())) * p_progress[_O1_] + quad[1]._A_()); \
    quad[2]._SET_A_((icon._A_() + icon._DA_() * (quad[2]._A_() / geo._DA_()) - (quad[2]._A_() + geo._A_())) * p_progress[_O2_] + quad[2]._A_()); \
    quad[3]._SET_A_((icon._A_() + icon._DA_() * (quad[3]._A_() / geo._DA_()) - (quad[3]._A_() + geo._A_())) * p_progress[_O3_] + quad[3]._A_()); \
                                                                                                                                                 \
    quad[0]._SET_B_(quad[0]._B_() + offset[_O0_]);                                                                                               \
    quad[1]._SET_B_(quad[1]._B_() + offset[_O1_]);                                                                                               \
    quad[2]._SET_B_(quad[2]._B_() + offset[_O2_]);                                                                                               \
    quad[3]._SET_B_(quad[3]._B_() + offset[_O3_])

        quads = quads.makeGrid(40);
        float quadFactor; // defines how fast a quad is vertically moved: y coordinates near to window top are slowed down
                          // it is used as quadFactor^3/windowHeight^3
                          // quadFactor is the y position of the quad but is changed towards becomming the window height
                          // by that the factor becomes 1 and has no influence any more
        float offset[2] = {0, 0}; // how far has a quad to be moved? Distance between icon and window multiplied by the progress and by the quadFactor
        float p_progress[2] = {0, 0}; // the factor which defines how far the x values have to be changed
                                      // factor is the current moved y value diveded by the distance between icon and window
        WindowQuad lastQuad;
        lastQuad[0].setX(-1);
        lastQuad[0].setY(-1);
        lastQuad[1].setX(-1);
        lastQuad[1].setY(-1);
        lastQuad[2].setX(-1);
        lastQuad[2].setY(-1);

        if (position == Bottom) {
            const float height_cube = float(geo.height()) * float(geo.height()) * float(geo.height());
            const float maxY = icon.y() - geo.y();

            for (WindowQuad &quad : quads) {

                if (quad[0].y() != lastQuad[0].y() || quad[2].y() != lastQuad[2].y()) {
                    quadFactor = quad[0].y() + (geo.height() - quad[0].y()) * progress;
                    offset[0] = (icon.y() + quad[0].y() - geo.y()) * progress * ((quadFactor * quadFactor * quadFactor) / height_cube);
                    quadFactor = quad[2].y() + (geo.height() - quad[2].y()) * progress;
                    offset[1] = (icon.y() + quad[2].y() - geo.y()) * progress * ((quadFactor * quadFactor * quadFactor) / height_cube);
                    p_progress[1] = std::min(offset[1] / (icon.y() - geo.y() - float(quad[2].y())), 1.0f);
                    p_progress[0] = std::min(offset[0] / (icon.y() - geo.y() - float(quad[0].y())), 1.0f);
                } else {
                    lastQuad = quad;
                }

                p_progress[0] = std::abs(p_progress[0]);
                p_progress[1] = std::abs(p_progress[1]);
                // x values are moved towards the center of the icon
                SET_QUADS(setX, x, width, setY, y, 0, 0, 1, 1);

                if (quad[0].y() > maxY) {
                    quad[0].setY(maxY);
                }
                if (quad[1].y() > maxY) {
                    quad[1].setY(maxY);
                }
                if (quad[2].y() > maxY) {
                    quad[2].setY(maxY);
                }
                if (quad[3].y() > maxY) {
                    quad[3].setY(maxY);
                }
            }
        } else if (position == Top) {
            const float height_cube = float(geo.height()) * float(geo.height()) * float(geo.height());
            const float minY = icon.y() + icon.height() - geo.y();

            for (WindowQuad &quad : quads) {

                if (quad[0].y() != lastQuad[0].y() || quad[2].y() != lastQuad[2].y()) {
                    quadFactor = geo.height() - quad[0].y() + (quad[0].y()) * progress;
                    offset[0] = (geo.y() - icon.height() + geo.height() + quad[0].y() - icon.y()) * progress * ((quadFactor * quadFactor * quadFactor) / height_cube);
                    quadFactor = geo.height() - quad[2].y() + (quad[2].y()) * progress;
                    offset[1] = (geo.y() - icon.height() + geo.height() + quad[2].y() - icon.y()) * progress * ((quadFactor * quadFactor * quadFactor) / height_cube);
                    p_progress[0] = std::min(offset[0] / (geo.y() - icon.height() + geo.height() - icon.y() - float(geo.height() - quad[0].y())), 1.0f);
                    p_progress[1] = std::min(offset[1] / (geo.y() - icon.height() + geo.height() - icon.y() - float(geo.height() - quad[2].y())), 1.0f);
                } else {
                    lastQuad = quad;
                }

                offset[0] = -offset[0];
                offset[1] = -offset[1];

                p_progress[0] = std::abs(p_progress[0]);
                p_progress[1] = std::abs(p_progress[1]);
                // x values are moved towards the center of the icon
                SET_QUADS(setX, x, width, setY, y, 0, 0, 1, 1);

                if (quad[0].y() < minY) {
                    quad[0].setY(minY);
                }
                if (quad[1].y() < minY) {
                    quad[1].setY(minY);
                }
                if (quad[2].y() < minY) {
                    quad[2].setY(minY);
                }
                if (quad[3].y() < minY) {
                    quad[3].setY(minY);
                }
            }
        } else if (position == Left) {
            const float width_cube = float(geo.width()) * float(geo.width()) * float(geo.width());
            const float minX = icon.x() + icon.width() - geo.x();

            for (WindowQuad &quad : quads) {

                if (quad[0].x() != lastQuad[0].x() || quad[1].x() != lastQuad[1].x()) {
                    quadFactor = geo.width() - quad[0].x() + (quad[0].x()) * progress;
                    offset[0] = (geo.x() - icon.width() + geo.width() + quad[0].x() - icon.x()) * progress * ((quadFactor * quadFactor * quadFactor) / width_cube);
                    quadFactor = geo.width() - quad[1].x() + (quad[1].x()) * progress;
                    offset[1] = (geo.x() - icon.width() + geo.width() + quad[1].x() - icon.x()) * progress * ((quadFactor * quadFactor * quadFactor) / width_cube);
                    p_progress[0] = std::min(offset[0] / (geo.x() - icon.width() + geo.width() - icon.x() - float(geo.width() - quad[0].x())), 1.0f);
                    p_progress[1] = std::min(offset[1] / (geo.x() - icon.width() + geo.width() - icon.x() - float(geo.width() - quad[1].x())), 1.0f);
                } else {
                    lastQuad = quad;
                }

                offset[0] = -offset[0];
                offset[1] = -offset[1];

                p_progress[0] = std::abs(p_progress[0]);
                p_progress[1] = std::abs(p_progress[1]);
                // y values are moved towards the center of the icon
                SET_QUADS(setY, y, height, setX, x, 0, 1, 1, 0);

                if (quad[0].x() < minX) {
                    quad[0].setX(minX);
                }
                if (quad[1].x() < minX) {
                    quad[1].setX(minX);
                }
                if (quad[2].x() < minX) {
                    quad[2].setX(minX);
                }
                if (quad[3].x() < minX) {
                    quad[3].setX(minX);
                }
            }
        } else if (position == Right) {
            const float width_cube = float(geo.width()) * float(geo.width()) * float(geo.width());
            const float maxX = icon.x() - geo.x();

            for (WindowQuad &quad : quads) {

                if (quad[0].x() != lastQuad[0].x() || quad[1].x() != lastQuad[1].x()) {
                    quadFactor = quad[0].x() + (geo.width() - quad[0].x()) * progress;
                    offset[0] = (icon.x() + quad[0].x() - geo.x()) * progress * ((quadFactor * quadFactor * quadFactor) / width_cube);
                    quadFactor = quad[1].x() + (geo.width() - quad[1].x()) * progress;
                    offset[1] = (icon.x() + quad[1].x() - geo.x()) * progress * ((quadFactor * quadFactor * quadFactor) / width_cube);
                    p_progress[0] = std::min(offset[0] / (icon.x() - geo.x() - float(quad[0].x())), 1.0f);
                    p_progress[1] = std::min(offset[1] / (icon.x() - geo.x() - float(quad[1].x())), 1.0f);
                } else {
                    lastQuad = quad;
                }

                p_progress[0] = std::abs(p_progress[0]);
                p_progress[1] = std::abs(p_progress[1]);
                // y values are moved towards the center of the icon
                SET_QUADS(setY, y, height, setX, x, 0, 1, 1, 0);

                if (quad[0].x() > maxX) {
                    quad[0].setX(maxX);
                }
                if (quad[1].x() > maxX) {
                    quad[1].setX(maxX);
                }
                if (quad[2].x() > maxX) {
                    quad[2].setX(maxX);
                }
                if (quad[3].x() > maxX) {
                    quad[3].setX(maxX);
                }
            }
        }
    }
}

void MagicLampEffect::postPaintScreen()
{
    auto animationIt = m_animations.begin();
    while (animationIt != m_animations.end()) {
        if ((*animationIt).timeLine.done()) {
            unredirect(animationIt.key());
            animationIt = m_animations.erase(animationIt);
        } else {
            ++animationIt;
        }
    }

    effects->addRepaintFull();

    // Call the next effect.
    effects->postPaintScreen();
}

void MagicLampEffect::slotWindowDeleted(EffectWindow *w)
{
    m_animations.remove(w);
}

void MagicLampEffect::slotWindowMinimized(EffectWindow *w)
{
    if (effects->activeFullScreenEffect()) {
        return;
    }

    MagicLampAnimation &animation = m_animations[w];

    if (animation.timeLine.running()) {
        animation.timeLine.toggleDirection();
    } else {
        animation.visibleRef = EffectWindowVisibleRef(w, EffectWindow::PAINT_DISABLED_BY_MINIMIZE);
        animation.timeLine.setDirection(TimeLine::Forward);
        animation.timeLine.setDuration(m_duration);
        animation.timeLine.setEasingCurve(QEasingCurve::Linear);
    }

    redirect(w);
    effects->addRepaintFull();
}

void MagicLampEffect::slotWindowUnminimized(EffectWindow *w)
{
    if (effects->activeFullScreenEffect()) {
        return;
    }

    MagicLampAnimation &animation = m_animations[w];

    if (animation.timeLine.running()) {
        animation.timeLine.toggleDirection();
    } else {
        animation.visibleRef = EffectWindowVisibleRef(w, EffectWindow::PAINT_DISABLED_BY_MINIMIZE);
        animation.timeLine.setDirection(TimeLine::Backward);
        animation.timeLine.setDuration(m_duration);
        animation.timeLine.setEasingCurve(QEasingCurve::Linear);
    }

    redirect(w);
    effects->addRepaintFull();
}

bool MagicLampEffect::isActive() const
{
    return !m_animations.isEmpty();
}

} // namespace
