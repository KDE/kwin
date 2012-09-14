/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

 Copyright (C) 2008, 2009 Martin Gräßlin <kde@martin-graesslin.com>

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
#include "flipswitch.h"
// KConfigSkeleton
#include "flipswitchconfig.h"

#include <kwinconfig.h>
#include <QFont>
#include <QKeyEvent>
#include <QMatrix4x4>

#include <kdebug.h>
#include <KAction>
#include <KActionCollection>
#include <KLocale>

#include <kwinglutils.h>

#include <math.h>

namespace KWin
{

KWIN_EFFECT(flipswitch, FlipSwitchEffect)
KWIN_EFFECT_SUPPORTED(flipswitch, FlipSwitchEffect::supported())

FlipSwitchEffect::FlipSwitchEffect()
    : m_currentAnimationShape(QTimeLine::EaseInOutCurve)
    , m_active(false)
    , m_start(false)
    , m_stop(false)
    , m_animation(false)
    , m_hasKeyboardGrab(false)
    , m_captionFrame(NULL)
{
    reconfigure(ReconfigureAll);

    // Caption frame
    m_captionFont.setBold(true);
    m_captionFont.setPointSize(m_captionFont.pointSize() * 2);

    KActionCollection* actionCollection = new KActionCollection(this);
    KAction* a = (KAction*)actionCollection->addAction("FlipSwitchCurrent");
    a->setText(i18n("Toggle Flip Switch (Current desktop)"));
    a->setGlobalShortcut(KShortcut(), KAction::ActiveShortcut);
    m_shortcutCurrent = a->globalShortcut();
    connect(a, SIGNAL(triggered(bool)), this, SLOT(toggleActiveCurrent()));
    connect(a, SIGNAL(globalShortcutChanged(QKeySequence)), this, SLOT(globalShortcutChangedCurrent(QKeySequence)));
    KAction* b = (KAction*)actionCollection->addAction("FlipSwitchAll");
    b->setText(i18n("Toggle Flip Switch (All desktops)"));
    b->setGlobalShortcut(KShortcut(), KAction::ActiveShortcut);
    m_shortcutAll = b->globalShortcut();
    connect(b, SIGNAL(triggered(bool)), this, SLOT(toggleActiveAllDesktops()));
    connect(b, SIGNAL(globalShortcutChanged(QKeySequence)), this, SLOT(globalShortcutChangedAll(QKeySequence)));
    connect(effects, SIGNAL(windowAdded(KWin::EffectWindow*)), this, SLOT(slotWindowAdded(KWin::EffectWindow*)));
    connect(effects, SIGNAL(windowClosed(KWin::EffectWindow*)), this, SLOT(slotWindowClosed(KWin::EffectWindow*)));
    connect(effects, SIGNAL(tabBoxAdded(int)), this, SLOT(slotTabBoxAdded(int)));
    connect(effects, SIGNAL(tabBoxClosed()), this, SLOT(slotTabBoxClosed()));
    connect(effects, SIGNAL(tabBoxUpdated()), this, SLOT(slotTabBoxUpdated()));
    connect(effects, SIGNAL(tabBoxKeyEvent(QKeyEvent*)), this, SLOT(slotTabBoxKeyEvent(QKeyEvent*)));
}

FlipSwitchEffect::~FlipSwitchEffect()
{
    foreach (ElectricBorder border, m_borderActivate) {
        effects->unreserveElectricBorder(border);
    }
    foreach (ElectricBorder border, m_borderActivateAll) {
        effects->unreserveElectricBorder(border);
    }
    delete m_captionFrame;
}

bool FlipSwitchEffect::supported()
{
    return effects->compositingType() == OpenGLCompositing;
}

void FlipSwitchEffect::reconfigure(ReconfigureFlags)
{
    FlipSwitchConfig::self()->readConfig();
    foreach (ElectricBorder border, m_borderActivate) {
        effects->unreserveElectricBorder(border);
    }
    foreach (ElectricBorder border, m_borderActivateAll) {
        effects->unreserveElectricBorder(border);
    }
    m_borderActivate.clear();
    m_borderActivateAll.clear();
    foreach (int i, FlipSwitchConfig::borderActivate()) {
        m_borderActivate.append(ElectricBorder(i));
        effects->reserveElectricBorder(ElectricBorder(i));
    }
    foreach (int i, FlipSwitchConfig::borderActivateAll()) {
        m_borderActivateAll.append(ElectricBorder(i));
        effects->reserveElectricBorder(ElectricBorder(i));
    }
    m_tabbox = FlipSwitchConfig::tabBox();
    m_tabboxAlternative = FlipSwitchConfig::tabBoxAlternative();
    const int duration = animationTime(FlipSwitchConfig::duration() != 0 ? FlipSwitchConfig::duration() : 200);
    m_timeLine.setDuration(duration);
    m_startStopTimeLine.setDuration(duration);

    m_angle = FlipSwitchConfig::angle();
    m_xPosition = FlipSwitchConfig::xPosition() / 100.0f;
    m_yPosition = FlipSwitchConfig::yPosition() / 100.0f;
    m_windowTitle = FlipSwitchConfig::windowTitle();
}

void FlipSwitchEffect::prePaintScreen(ScreenPrePaintData& data, int time)
{
    if (m_active) {
        data.mask |= PAINT_SCREEN_WITH_TRANSFORMED_WINDOWS;
        if (m_start)
            m_startStopTimeLine.setCurrentTime(m_startStopTimeLine.currentTime() + time);
        if (m_stop && m_scheduledDirections.isEmpty())
            m_startStopTimeLine.setCurrentTime(m_startStopTimeLine.currentTime() - time);
        if (m_animation)
            m_timeLine.setCurrentTime(m_timeLine.currentTime() + time);
    }
    effects->prePaintScreen(data, time);
}

void FlipSwitchEffect::paintScreen(int mask, QRegion region, ScreenPaintData& data)
{
    effects->paintScreen(mask, region, data);
    if (m_active) {
        EffectWindowList tempList;
        if (m_mode == TabboxMode)
            tempList = effects->currentTabBoxWindowList();
        else {
            // we have to setup the list
            // using stacking order directly is not possible
            // as not each window in stacking order is shown
            // TODO: store list instead of calculating in each frame?
            foreach (EffectWindow * w, effects->stackingOrder()) {
                if (m_windows.contains(w))
                    tempList.append(w);
            }
        }
        m_flipOrderedWindows.clear();
        int index = tempList.indexOf(m_selectedWindow);

        int tabIndex = index;
        if (m_mode == TabboxMode) {
            foreach (SwitchingDirection direction, m_scheduledDirections) { // krazy:exclude=foreach
                if (direction == DirectionBackward)
                    index++;
                else
                    index--;
                if (index < 0)
                    index = tempList.count() + index;
                if (index >= tempList.count())
                    index = index % tempList.count();
            }
            tabIndex = index;
            EffectWindow* w = NULL;
            if (!m_scheduledDirections.isEmpty() && m_scheduledDirections.head() == DirectionBackward) {
                index--;
                if (index < 0)
                    index = tempList.count() + index;
                w = tempList.at(index);
            }
            for (int i = index - 1; i >= 0; i--)
                m_flipOrderedWindows.append(tempList.at(i));
            for (int i = effects->currentTabBoxWindowList().count() - 1; i >= index; i--)
                m_flipOrderedWindows.append(tempList.at(i));
            if (w) {
                m_flipOrderedWindows.removeAll(w);
                m_flipOrderedWindows.append(w);
            }
        } else {
            foreach (SwitchingDirection direction, m_scheduledDirections) { // krazy:exclude=foreach
                if (direction == DirectionForward)
                    index++;
                else
                    index--;
                if (index < 0)
                    index = tempList.count() - 1;
                if (index >= tempList.count())
                    index = 0;
            }
            tabIndex = index;
            EffectWindow* w = NULL;
            if (!m_scheduledDirections.isEmpty() && m_scheduledDirections.head() == DirectionBackward) {
                index++;
                if (index >= tempList.count())
                    index = 0;
            }
            // sort from stacking order
            for (int i = index + 1; i < tempList.count(); i++)
                m_flipOrderedWindows.append(tempList.at(i));
            for (int i = 0; i <= index; i++)
                m_flipOrderedWindows.append(tempList.at(i));
            if (w) {
                m_flipOrderedWindows.removeAll(w);
                m_flipOrderedWindows.append(w);
            }
        }

        // multiscreen part taken from coverswitch.cpp
        // TODO: move to kwinglutils
        QMatrix4x4 origProjection;
        QMatrix4x4 origModelview;
        ShaderManager *shaderManager = ShaderManager::instance();
        if (effects->numScreens() > 1) {
            // unfortunatelly we have to change the projection matrix in dual screen mode
            QRect fullRect = effects->clientArea(FullArea, effects->activeScreen(), effects->currentDesktop());
            float fovy = 60.0f;
            float aspect = 1.0f;
            float zNear = 0.1f;
            float zFar = 100.0f;
            float ymax = zNear * tan(fovy  * M_PI / 360.0f);
            float ymin = -ymax;
            float xmin =  ymin * aspect;
            float xmax = ymax * aspect;
            float xTranslate = 0.0;
            float yTranslate = 0.0;
            float xminFactor = 1.0;
            float xmaxFactor = 1.0;
            float yminFactor = 1.0;
            float ymaxFactor = 1.0;
            if (m_screenArea.x() == 0 && m_screenArea.width() != fullRect.width()) {
                // horizontal layout: left screen
                xminFactor = (float)m_screenArea.width() / (float)fullRect.width();
                xmaxFactor = ((float)fullRect.width() - (float)m_screenArea.width() * 0.5f) / ((float)fullRect.width() * 0.5f);
                xTranslate = (float)fullRect.width() * 0.5f - (float)m_screenArea.width() * 0.5f;
            }
            if (m_screenArea.x() != 0 && m_screenArea.width() != fullRect.width()) {
                // horizontal layout: right screen
                xminFactor = ((float)fullRect.width() - (float)m_screenArea.width() * 0.5f) / ((float)fullRect.width() * 0.5f);
                xmaxFactor = (float)m_screenArea.width() / (float)fullRect.width();
                xTranslate = (float)fullRect.width() * 0.5f - (float)m_screenArea.width() * 0.5f;
            }
            if (m_screenArea.y() == 0 && m_screenArea.height() != fullRect.height()) {
                // vertical layout: top screen
                yminFactor = ((float)fullRect.height() - (float)m_screenArea.height() * 0.5f) / ((float)fullRect.height() * 0.5f);
                ymaxFactor = (float)m_screenArea.height() / (float)fullRect.height();
                yTranslate = (float)fullRect.height() * 0.5f - (float)m_screenArea.height() * 0.5f;
            }
            if (m_screenArea.y() != 0 && m_screenArea.height() != fullRect.height()) {
                // vertical layout: bottom screen
                yminFactor = (float)m_screenArea.height() / (float)fullRect.height();
                ymaxFactor = ((float)fullRect.height() - (float)m_screenArea.height() * 0.5f) / ((float)fullRect.height() * 0.5f);
                yTranslate = (float)fullRect.height() * 0.5f - (float)m_screenArea.height() * 0.5f;
            }
            QMatrix4x4 projection;
            projection.frustum(xmin * xminFactor, xmax * xmaxFactor, ymin * yminFactor, ymax * ymaxFactor, zNear, zFar);
            QMatrix4x4 modelview;
            modelview.translate(xTranslate, yTranslate, 0.0);
            if (shaderManager->isShaderBound()) {
                GLShader *shader = shaderManager->pushShader(ShaderManager::GenericShader);
                origProjection = shader->getUniformMatrix4x4("projection");
                origModelview = shader->getUniformMatrix4x4("modelview");
                shader->setUniform("projection", projection);
                shader->setUniform("modelview", origModelview * modelview);
                shaderManager->popShader();
            } else {
#ifndef KWIN_HAVE_OPENGLES
                glMatrixMode(GL_PROJECTION);
                pushMatrix();
                loadMatrix(projection);
                glMatrixMode(GL_MODELVIEW);
                pushMatrix(modelview);
#endif
            }
        }

        int winMask = PAINT_WINDOW_TRANSFORMED | PAINT_WINDOW_TRANSLUCENT;
        // fade in/out one window at the end of the stack during animation
        if (m_animation && !m_scheduledDirections.isEmpty()) {
            EffectWindow* w = m_flipOrderedWindows.last();
            if (ItemInfo *info = m_windows.value(w,0)) {
                WindowPaintData data(w);
                data.setRotationAxis(Qt::YAxis);
                data.setRotationAngle(m_angle * m_startStopTimeLine.currentValue());
                data.setOpacity(info->opacity);
                data.setBrightness(info->brightness);
                data.setSaturation(info->saturation);
                int distance = tempList.count() - 1;
                float zDistance = 500.0f;
                data.translate(- (w->x() - m_screenArea.x() + data.xTranslation()) * m_startStopTimeLine.currentValue());

                data.translate(m_screenArea.width() * m_xPosition * m_startStopTimeLine.currentValue(),
                               (m_screenArea.y() + m_screenArea.height() * m_yPosition - (w->y() + w->height() + data.yTranslation())) * m_startStopTimeLine.currentValue());
                data.translate(- (m_screenArea.width() * 0.25f) * distance * m_startStopTimeLine.currentValue(),
                               - (m_screenArea.height() * 0.10f) * distance * m_startStopTimeLine.currentValue(),
                               - (zDistance * distance) * m_startStopTimeLine.currentValue());
                if (m_scheduledDirections.head() == DirectionForward)
                    data.multiplyOpacity(0.8 * m_timeLine.currentValue());
                else
                    data.multiplyOpacity(0.8 * (1.0 - m_timeLine.currentValue()));

                if (effects->numScreens() > 1) {
                    adjustWindowMultiScreen(w, data);
                }
                effects->drawWindow(w, winMask, infiniteRegion(), data);
            }
        }

        foreach (EffectWindow *w, m_flipOrderedWindows) {
            ItemInfo *info = m_windows.value(w,0);
            if (!info)
                continue;
            WindowPaintData data(w);
            data.setRotationAxis(Qt::YAxis);
            data.setRotationAngle(m_angle * m_startStopTimeLine.currentValue());
            data.setOpacity(info->opacity);
            data.setBrightness(info->brightness);
            data.setSaturation(info->saturation);
            int windowIndex = tempList.indexOf(w);
            int distance;
            if (m_mode == TabboxMode) {
                if (windowIndex < tabIndex)
                    distance = tempList.count() - (tabIndex - windowIndex);
                else if (windowIndex > tabIndex)
                    distance = windowIndex - tabIndex;
                else
                    distance = 0;
            } else {
                distance = m_flipOrderedWindows.count() - m_flipOrderedWindows.indexOf(w) - 1;

                if (!m_scheduledDirections.isEmpty() && m_scheduledDirections.head() == DirectionBackward) {
                    distance--;
                }
            }
            if (!m_scheduledDirections.isEmpty() && m_scheduledDirections.head() == DirectionBackward) {
                if (w == m_flipOrderedWindows.last()) {
                    distance = -1;
                    data.multiplyOpacity(m_timeLine.currentValue());
                }
            }
            float zDistance = 500.0f;
            data.translate(- (w->x() - m_screenArea.x() + data.xTranslation()) * m_startStopTimeLine.currentValue());
            data.translate(m_screenArea.width() * m_xPosition * m_startStopTimeLine.currentValue(),
                           (m_screenArea.y() + m_screenArea.height() * m_yPosition - (w->y() + w->height() + data.yTranslation())) * m_startStopTimeLine.currentValue());

            data.translate(-(m_screenArea.width() * 0.25f) * distance * m_startStopTimeLine.currentValue(),
                           -(m_screenArea.height() * 0.10f) * distance * m_startStopTimeLine.currentValue(),
                           -(zDistance * distance) * m_startStopTimeLine.currentValue());
            if (m_animation && !m_scheduledDirections.isEmpty()) {
                if (m_scheduledDirections.head() == DirectionForward) {
                    data.translate((m_screenArea.width() * 0.25f) * m_timeLine.currentValue(),
                                   (m_screenArea.height() * 0.10f) * m_timeLine.currentValue(),
                                   zDistance * m_timeLine.currentValue());
                    if (distance == 0)
                        data.multiplyOpacity((1.0 - m_timeLine.currentValue()));
                } else {
                    data.translate(- (m_screenArea.width() * 0.25f) * m_timeLine.currentValue(),
                                   - (m_screenArea.height() * 0.10f) * m_timeLine.currentValue(),
                                   - zDistance * m_timeLine.currentValue());
                }
            }
            data.multiplyOpacity((0.8 + 0.2 * (1.0 - m_startStopTimeLine.currentValue())));
            if (effects->numScreens() > 1) {
                adjustWindowMultiScreen(w, data);
            }

            effects->drawWindow(w, winMask, infiniteRegion(), data);
        }

        if (effects->numScreens() > 1) {
            if (shaderManager->isShaderBound())  {
                GLShader *shader = shaderManager->pushShader(ShaderManager::GenericShader);
                shader->setUniform("projection", origProjection);
                shader->setUniform("modelview", origModelview);
                shaderManager->popShader();
            } else {
#ifndef KWIN_HAVE_OPENGLES
                popMatrix();
                // revert change of projection matrix
                glMatrixMode(GL_PROJECTION);
                popMatrix();
                glMatrixMode(GL_MODELVIEW);
#endif
            }
        }

        if (m_windowTitle) {
            // Render the caption frame
            if (m_animation) {
                m_captionFrame->setCrossFadeProgress(m_timeLine.currentValue());
            }
            m_captionFrame->render(region, m_startStopTimeLine.currentValue());
        }
    }
}

void FlipSwitchEffect::postPaintScreen()
{
    if (m_active) {
        if (m_start && m_startStopTimeLine.currentValue() == 1.0f) {
            m_start = false;
            if (!m_scheduledDirections.isEmpty()) {
                m_animation = true;
                m_timeLine.setCurrentTime(0);
                if (m_scheduledDirections.count() == 1) {
                    m_currentAnimationShape = QTimeLine::EaseOutCurve;
                    m_timeLine.setCurveShape(m_currentAnimationShape);
                } else {
                    m_currentAnimationShape = QTimeLine::LinearCurve;
                    m_timeLine.setCurveShape(m_currentAnimationShape);
                }
            }
            effects->addRepaintFull();
        }
        if (m_stop && m_startStopTimeLine.currentValue() == 0.0f) {
            m_stop = false;
            m_active = false;
            m_captionFrame->free();
            effects->setActiveFullScreenEffect(0);
            effects->addRepaintFull();
            qDeleteAll(m_windows);
            m_windows.clear();
        }
        if (m_animation && m_timeLine.currentValue() == 1.0f) {
            m_timeLine.setCurrentTime(0);
            m_scheduledDirections.dequeue();
            if (m_scheduledDirections.isEmpty()) {
                m_animation = false;
                effects->addRepaintFull();
            } else {
                if (m_scheduledDirections.count() == 1) {
                    if (m_stop)
                        m_currentAnimationShape = QTimeLine::LinearCurve;
                    else
                        m_currentAnimationShape = QTimeLine::EaseOutCurve;
                } else {
                    m_currentAnimationShape = QTimeLine::LinearCurve;
                }
                m_timeLine.setCurveShape(m_currentAnimationShape);
            }
        }
        if (m_start || m_stop || m_animation)
            effects->addRepaintFull();
    }
    effects->postPaintScreen();
}

void FlipSwitchEffect::prePaintWindow(EffectWindow* w, WindowPrePaintData& data, int time)
{
    if (m_active) {
        if (m_windows.contains(w)) {
            data.setTransformed();
            data.setTranslucent();
            if (!w->isOnCurrentDesktop())
                w->enablePainting(EffectWindow::PAINT_DISABLED_BY_DESKTOP);
            if (w->isMinimized())
                w->enablePainting(EffectWindow::PAINT_DISABLED_BY_MINIMIZE);
            if (!w->isCurrentTab())
                w->enablePainting(EffectWindow::PAINT_DISABLED_BY_TAB_GROUP);
        } else {
            if ((m_start || m_stop) && !w->isDesktop() && w->isOnCurrentDesktop())
                data.setTranslucent();
            else if (!w->isDesktop())
                w->disablePainting(EffectWindow::PAINT_DISABLED_BY_DESKTOP);
        }
    }
    effects->prePaintWindow(w, data, time);
}

void FlipSwitchEffect::paintWindow(EffectWindow* w, int mask, QRegion region, WindowPaintData& data)
{
    if (m_active) {
        ItemInfo *info = m_windows.value(w,0);
        if (info) {
            info->opacity = data.opacity();
            info->brightness = data.brightness();
            info->saturation = data.saturation();
        }

        // fade out all windows not in window list except the desktops
        const bool isFader = (m_start || m_stop) && !info && !w->isDesktop();
        if (isFader)
            data.multiplyOpacity((1.0 - m_startStopTimeLine.currentValue()));

        // if not a fader or the desktop, skip painting here to prevent flicker
        if (!(isFader || w->isDesktop()))
            return;
    }
    effects->paintWindow(w, mask, region, data);
}

//*************************************************************
// Tabbox handling
//*************************************************************
void FlipSwitchEffect::slotTabBoxAdded(int mode)
{
    if (effects->activeFullScreenEffect() && effects->activeFullScreenEffect() != this)
        return;
    // only for windows mode
    if (((mode == TabBoxWindowsMode && m_tabbox) ||
            (mode == TabBoxWindowsAlternativeMode && m_tabboxAlternative) ||
            (mode == TabBoxCurrentAppWindowsMode && m_tabbox) ||
            (mode == TabBoxCurrentAppWindowsAlternativeMode && m_tabboxAlternative))
            && (!m_active || (m_active && m_stop))
            && !effects->currentTabBoxWindowList().isEmpty()) {
        setActive(true, TabboxMode);
        if (m_active)
            effects->refTabBox();
    }
}

void FlipSwitchEffect::slotTabBoxClosed()
{
    if (m_active) {
        setActive(false, TabboxMode);
        effects->unrefTabBox();
    }
}

void FlipSwitchEffect::slotTabBoxUpdated()
{
    if (m_active && !m_stop) {
        if (!effects->currentTabBoxWindowList().isEmpty()) {
            // determine the switch direction
            if (m_selectedWindow != effects->currentTabBoxWindow()) {
                if (m_selectedWindow != NULL) {
                    int old_index = effects->currentTabBoxWindowList().indexOf(m_selectedWindow);
                    int new_index = effects->currentTabBoxWindowList().indexOf(effects->currentTabBoxWindow());
                    SwitchingDirection new_direction;
                    int distance = new_index - old_index;
                    if (distance > 0)
                        new_direction = DirectionForward;
                    if (distance < 0)
                        new_direction = DirectionBackward;
                    if (effects->currentTabBoxWindowList().count() == 2) {
                        new_direction = DirectionForward;
                        distance = 1;
                    }
                    if (distance != 0) {
                        distance = abs(distance);
                        int tempDistance = effects->currentTabBoxWindowList().count() - distance;
                        if (tempDistance < abs(distance)) {
                            distance = tempDistance;
                            if (new_direction == DirectionForward)
                                new_direction = DirectionBackward;
                            else
                                new_direction = DirectionForward;
                        }
                        scheduleAnimation(new_direction, distance);
                    }
                }
                m_selectedWindow = effects->currentTabBoxWindow();
                updateCaption();
            }
        }
        effects->addRepaintFull();
    }
}

//*************************************************************
// Window adding/removing handling
//*************************************************************

void FlipSwitchEffect::slotWindowAdded(EffectWindow* w)
{
    if (m_active && isSelectableWindow(w)) {
        m_windows[ w ] = new ItemInfo;
    }
}

void FlipSwitchEffect::slotWindowClosed(EffectWindow* w)
{
    if (m_selectedWindow == w)
        m_selectedWindow = 0;
    if (m_active) {
        QHash< const EffectWindow*, ItemInfo* >::iterator it = m_windows.find(w);
        if (it != m_windows.end()) {
            delete *it;
            m_windows.erase(it);
        }
    }
}

//*************************************************************
// Activation
//*************************************************************

void FlipSwitchEffect::setActive(bool activate, FlipSwitchMode mode)
{
    if (activate) {
        // effect already active, do some sanity checks
        if (m_active) {
            if (m_stop) {
                if (mode != m_mode) {
                    // only the same mode may reactivate the effect
                    return;
                }
            } else {
                // active, but not scheduled for closing -> abort
                return;
            }
        }

        m_mode = mode;
        foreach (EffectWindow * w, effects->stackingOrder()) {
            if (isSelectableWindow(w) && !m_windows.contains(w))
                m_windows[ w ] = new ItemInfo;
        }
        if (m_windows.isEmpty())
            return;

        effects->setActiveFullScreenEffect(this);
        m_active = true;
        m_start = true;
        m_startStopTimeLine.setCurveShape(QTimeLine::EaseInOutCurve);
        m_activeScreen = effects->activeScreen();
        m_screenArea = effects->clientArea(ScreenArea, m_activeScreen, effects->currentDesktop());

        if (m_stop) {
            // effect is still closing from last usage
            m_stop = false;
        } else {
            // things to do only when there is no closing animation
            m_scheduledDirections.clear();
        }

        switch(m_mode) {
        case TabboxMode:
            m_selectedWindow = effects->currentTabBoxWindow();
            m_input = effects->createFullScreenInputWindow(this, Qt::ArrowCursor);
            break;
        case CurrentDesktopMode:
            m_selectedWindow = effects->activeWindow();
            m_input = effects->createFullScreenInputWindow(this, Qt::BlankCursor);
            m_hasKeyboardGrab = effects->grabKeyboard(this);
            break;
        case AllDesktopsMode:
            m_selectedWindow = effects->activeWindow();
            m_input = effects->createFullScreenInputWindow(this, Qt::BlankCursor);
            m_hasKeyboardGrab = effects->grabKeyboard(this);
            break;
        }

        // Setup caption frame geometry
        QRect frameRect = QRect(m_screenArea.width() * 0.25f + m_screenArea.x(),
                                m_screenArea.height() * 0.1f + m_screenArea.y() - QFontMetrics(m_captionFont).height(),
                                m_screenArea.width() * 0.5f,
                                QFontMetrics(m_captionFont).height());
        if (!m_captionFrame) {
            m_captionFrame = effects->effectFrame(EffectFrameStyled);
            m_captionFrame->setFont(m_captionFont);
            m_captionFrame->enableCrossFade(true);
        }
        m_captionFrame->setGeometry(frameRect);
        m_captionFrame->setIconSize(QSize(frameRect.height(), frameRect.height()));
        updateCaption();
        effects->addRepaintFull();
    } else {
        // only deactivate if mode is current mode
        if (mode != m_mode)
            return;
        if (m_start && m_scheduledDirections.isEmpty()) {
            m_start = false;
        }
        m_stop = true;
        if (m_animation) {
            m_startStopTimeLine.setCurveShape(QTimeLine::EaseOutCurve);
            if (m_scheduledDirections.count() == 1) {
                if (m_currentAnimationShape == QTimeLine::EaseInOutCurve)
                    m_currentAnimationShape = QTimeLine::EaseInCurve;
                else if (m_currentAnimationShape == QTimeLine::EaseOutCurve)
                    m_currentAnimationShape = QTimeLine::LinearCurve;
                m_timeLine.setCurveShape(m_currentAnimationShape);
            }
        } else
            m_startStopTimeLine.setCurveShape(QTimeLine::EaseInOutCurve);
        effects->destroyInputWindow(m_input);
        if (m_hasKeyboardGrab) {
            effects->ungrabKeyboard();
            m_hasKeyboardGrab = false;
        }
        effects->addRepaintFull();
    }
}

void FlipSwitchEffect::toggleActiveAllDesktops()
{
    if (m_active) {
        if (m_stop) {
            // reactivate if stopping
            setActive(true, AllDesktopsMode);
        } else {
            // deactivate if not stopping
            setActive(false, AllDesktopsMode);
        }
    } else {
        setActive(true, AllDesktopsMode);
    }
}

void FlipSwitchEffect::toggleActiveCurrent()
{
    if (m_active) {
        if (m_stop) {
            // reactivate if stopping
            setActive(true, CurrentDesktopMode);
        } else {
            // deactivate if not stopping
            setActive(false, CurrentDesktopMode);
        }
    } else {
        setActive(true, CurrentDesktopMode);
    }
}

bool FlipSwitchEffect::borderActivated(ElectricBorder border)
{
    if (!m_borderActivate.contains(border) && !m_borderActivateAll.contains(border))
        return false;
    if (effects->activeFullScreenEffect() && effects->activeFullScreenEffect() != this)
        return true;
    if (m_borderActivate.contains(border))
        toggleActiveCurrent();
    else
        toggleActiveAllDesktops();
    return true;
}

//*************************************************************
// Helper function
//*************************************************************

bool FlipSwitchEffect::isSelectableWindow(EffectWindow* w) const
{
    // desktop windows might be included
    if ((w->isSpecialWindow() && !w->isDesktop()) || w->isUtility())
        return false;
    if (w->isDesktop())
        return (m_mode == TabboxMode && effects->currentTabBoxWindowList().contains(w));
    if (w->isDeleted())
        return false;
    if (!w->acceptsFocus())
        return false;
    switch(m_mode) {
    case TabboxMode:
        return effects->currentTabBoxWindowList().contains(w);
    case CurrentDesktopMode:
        return w->isOnCurrentDesktop();
    case AllDesktopsMode:
        //nothing special
        break;
    }
    return true;
}

void FlipSwitchEffect::scheduleAnimation(const SwitchingDirection& direction, int distance)
{
    if (m_start) {
        // start is still active so change the shape to have a nice transition
        m_startStopTimeLine.setCurveShape(QTimeLine::EaseInCurve);
    }
    if (!m_animation && !m_start) {
        m_animation = true;
        m_scheduledDirections.enqueue(direction);
        distance--;
        // reset shape just to make sure
        m_currentAnimationShape = QTimeLine::EaseInOutCurve;
        m_timeLine.setCurveShape(m_currentAnimationShape);
    }
    for (int i = 0; i < distance; i++) {
        if (m_scheduledDirections.count() > 1 && m_scheduledDirections.last() != direction)
            m_scheduledDirections.pop_back();
        else
            m_scheduledDirections.enqueue(direction);
        if (m_scheduledDirections.count() == m_windows.count() + 1) {
            SwitchingDirection temp = m_scheduledDirections.dequeue();
            m_scheduledDirections.clear();
            m_scheduledDirections.enqueue(temp);
        }
    }
    if (m_scheduledDirections.count() > 1) {
        QTimeLine::CurveShape newShape = QTimeLine::EaseInOutCurve;
        switch(m_currentAnimationShape) {
        case QTimeLine::EaseInOutCurve:
            newShape = QTimeLine::EaseInCurve;
            break;
        case QTimeLine::EaseOutCurve:
            newShape = QTimeLine::LinearCurve;
            break;
        default:
            newShape = m_currentAnimationShape;
        }
        if (newShape != m_currentAnimationShape) {
            m_currentAnimationShape = newShape;
            m_timeLine.setCurveShape(m_currentAnimationShape);
        }
    }
}

void FlipSwitchEffect::adjustWindowMultiScreen(const KWin::EffectWindow* w, WindowPaintData& data)
{
    if (effects->numScreens() <= 1)
        return;
    QRect clientRect = effects->clientArea(FullScreenArea, w->screen(), effects->currentDesktop());
    QRect rect = effects->clientArea(ScreenArea, m_activeScreen, effects->currentDesktop());
    QRect fullRect = effects->clientArea(FullArea, m_activeScreen, effects->currentDesktop());
    if (w->screen() == m_activeScreen) {
        if (clientRect.width() != fullRect.width() && clientRect.x() != fullRect.x()) {
            data.translate(- clientRect.x());
        }
        if (clientRect.height() != fullRect.height() && clientRect.y() != fullRect.y()) {
            data.translate(0.0, - clientRect.y());
        }
    } else {
        if (clientRect.width() != fullRect.width() && clientRect.x() < rect.x()) {
            data.translate(- (m_screenArea.x() - clientRect.x()));
        }
        if (clientRect.height() != fullRect.height() && clientRect.y() < m_screenArea.y()) {
            data.translate(0.0, - (m_screenArea.y() - clientRect.y()));
        }
    }
}

void FlipSwitchEffect::selectNextOrPreviousWindow(bool forward)
{
    if (!m_active || !m_selectedWindow) {
        return;
    }
    const int index = effects->currentTabBoxWindowList().indexOf(m_selectedWindow);
    int newIndex = index;
    if (forward) {
        ++newIndex;
    } else {
        --newIndex;
    }
    if (newIndex == effects->currentTabBoxWindowList().size()) {
        newIndex = 0;
    } else if (newIndex < 0) {
        newIndex = effects->currentTabBoxWindowList().size() -1;
    }
    if (index == newIndex) {
        return;
    }
    effects->setTabBoxWindow(effects->currentTabBoxWindowList().at(newIndex));
}


//*************************************************************
// Keyboard handling
//*************************************************************
void FlipSwitchEffect::globalShortcutChangedAll(QKeySequence shortcut)
{
    m_shortcutAll = KShortcut(shortcut);
}

void FlipSwitchEffect::globalShortcutChangedCurrent(QKeySequence shortcut)
{
    m_shortcutCurrent = KShortcut(shortcut);
}

void FlipSwitchEffect::grabbedKeyboardEvent(QKeyEvent* e)
{
    if (e->type() == QEvent::KeyPress) {
        // check for global shortcuts
        // HACK: keyboard grab disables the global shortcuts so we have to check for global shortcut (bug 156155)
        if (m_mode == CurrentDesktopMode && m_shortcutCurrent.contains(e->key() + e->modifiers())) {
            toggleActiveCurrent();
            return;
        }
        if (m_mode == AllDesktopsMode && m_shortcutAll.contains(e->key() + e->modifiers())) {
            toggleActiveAllDesktops();
            return;
        }

        switch(e->key()) {
        case Qt::Key_Escape:
            setActive(false, m_mode);
            return;
        case Qt::Key_Tab: {
            // find next window
            if (m_windows.isEmpty())
                return; // sanity check
            bool found = false;
            for (int i = effects->stackingOrder().indexOf(m_selectedWindow) - 1; i >= 0; i--) {
                if (isSelectableWindow(effects->stackingOrder().at(i))) {
                    m_selectedWindow = effects->stackingOrder().at(i);
                    found = true;
                    break;
                }
            }
            if (!found) {
                for (int i = effects->stackingOrder().count() - 1; i > effects->stackingOrder().indexOf(m_selectedWindow); i--) {
                    if (isSelectableWindow(effects->stackingOrder().at(i))) {
                        m_selectedWindow = effects->stackingOrder().at(i);
                        found = true;
                        break;
                    }
                }
            }
            if (found) {
                updateCaption();
                scheduleAnimation(DirectionForward);
            }
            break;
        }
        case Qt::Key_Backtab: {
            // find previous window
            if (m_windows.isEmpty())
                return; // sanity check
            bool found = false;
            for (int i = effects->stackingOrder().indexOf(m_selectedWindow) + 1; i < effects->stackingOrder().count(); i++) {
                if (isSelectableWindow(effects->stackingOrder().at(i))) {
                    m_selectedWindow = effects->stackingOrder().at(i);
                    found = true;
                    break;
                }
            }
            if (!found) {
                for (int i = 0; i < effects->stackingOrder().indexOf(m_selectedWindow); i++) {
                    if (isSelectableWindow(effects->stackingOrder().at(i))) {
                        m_selectedWindow = effects->stackingOrder().at(i);
                        found = true;
                        break;
                    }
                }
            }
            if (found) {
                updateCaption();
                scheduleAnimation(DirectionBackward);
            }
            break;
        }
        case Qt::Key_Return:
        case Qt::Key_Enter:
        case Qt::Key_Space:
            if (m_selectedWindow)
                effects->activateWindow(m_selectedWindow);
            setActive(false, m_mode);
            break;
        default:
            break;
        }
        effects->addRepaintFull();
    }
}

void FlipSwitchEffect::slotTabBoxKeyEvent(QKeyEvent *event)
{
    if (event->type() == QEvent::KeyPress) {
        switch (event->key()) {
        case Qt::Key_Up:
        case Qt::Key_Left:
            selectPreviousWindow();
            break;
        case Qt::Key_Down:
        case Qt::Key_Right:
            selectNextWindow();
            break;
        default:
            // nothing
            break;
        }
    }
}

bool FlipSwitchEffect::isActive() const
{
    return m_active;
}

void FlipSwitchEffect::updateCaption()
{
    if (!m_selectedWindow) {
        return;
    }
    if (m_selectedWindow->isDesktop()) {
        m_captionFrame->setText(i18nc("Special entry in alt+tab list for minimizing all windows",
                     "Show Desktop"));
        static QPixmap pix = KIcon("user-desktop").pixmap(m_captionFrame->iconSize());
        m_captionFrame->setIcon(pix);
    } else {
        m_captionFrame->setText(m_selectedWindow->caption());
        m_captionFrame->setIcon(m_selectedWindow->icon());
    }
}

//*************************************************************
// Mouse handling
//*************************************************************

void FlipSwitchEffect::windowInputMouseEvent(Window w, QEvent* e)
{
    assert(w == m_input);
    Q_UNUSED(w);
    if (e->type() != QEvent::MouseButtonPress)
        return;
    // we don't want click events during animations
    if (m_animation)
        return;
    QMouseEvent* event = static_cast< QMouseEvent* >(e);

    switch (event->button()) {
    case Qt::XButton1: // wheel up
        selectPreviousWindow();
        break;
    case Qt::XButton2: // wheel down
        selectNextWindow();
        break;
    case Qt::LeftButton:
    case Qt::RightButton:
    case Qt::MidButton:
    default:
        // TODO: Change window on mouse button click
        break;
    }
}

//*************************************************************
// Item Info
//*************************************************************
FlipSwitchEffect::ItemInfo::ItemInfo()
    : deleted(false)
    , opacity(0.0)
    , brightness(0.0)
    , saturation(0.0)
{
}

FlipSwitchEffect::ItemInfo::~ItemInfo()
{
}

} // namespace

#include "flipswitch.moc"
