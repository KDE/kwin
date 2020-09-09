/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2008, 2009 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "flipswitch.h"
// KConfigSkeleton
#include "flipswitchconfig.h"

#include <QAction>
#include <kwinconfig.h>
#include <QFont>
#include <QKeyEvent>
#include <QMatrix4x4>

#include <KGlobalAccel>
#include <KLocalizedString>

#include <kwinglutils.h>

#include <cmath>

namespace KWin
{

FlipSwitchEffect::FlipSwitchEffect()
    : m_selectedWindow(nullptr)
    , m_currentAnimationEasingCurve(QEasingCurve::InOutSine)
    , m_active(false)
    , m_start(false)
    , m_stop(false)
    , m_animation(false)
    , m_hasKeyboardGrab(false)
    , m_captionFrame(nullptr)
{
    initConfig<FlipSwitchConfig>();
    reconfigure(ReconfigureAll);

    // Caption frame
    m_captionFont.setBold(true);
    m_captionFont.setPointSize(m_captionFont.pointSize() * 2);

    QAction* flipSwitchCurrentAction = new QAction(this);
    flipSwitchCurrentAction->setObjectName(QStringLiteral("FlipSwitchCurrent"));
    flipSwitchCurrentAction->setText(i18n("Toggle Flip Switch (Current desktop)"));
    KGlobalAccel::self()->setShortcut(flipSwitchCurrentAction, QList<QKeySequence>());
    m_shortcutCurrent = KGlobalAccel::self()->shortcut(flipSwitchCurrentAction);
    effects->registerGlobalShortcut(QKeySequence(), flipSwitchCurrentAction);
    connect(flipSwitchCurrentAction, &QAction::triggered, this, &FlipSwitchEffect::toggleActiveCurrent);
    QAction* flipSwitchAllAction = new QAction(this);
    flipSwitchAllAction->setObjectName(QStringLiteral("FlipSwitchAll"));
    flipSwitchAllAction->setText(i18n("Toggle Flip Switch (All desktops)"));
    KGlobalAccel::self()->setShortcut(flipSwitchAllAction, QList<QKeySequence>());
    effects->registerGlobalShortcut(QKeySequence(), flipSwitchAllAction);
    m_shortcutAll = KGlobalAccel::self()->shortcut(flipSwitchAllAction);
    connect(flipSwitchAllAction, &QAction::triggered, this, &FlipSwitchEffect::toggleActiveAllDesktops);
    connect(KGlobalAccel::self(), &KGlobalAccel::globalShortcutChanged, this, &FlipSwitchEffect::globalShortcutChanged);
    connect(effects, &EffectsHandler::windowAdded, this, &FlipSwitchEffect::slotWindowAdded);
    connect(effects, &EffectsHandler::windowClosed, this, &FlipSwitchEffect::slotWindowClosed);
    connect(effects, &EffectsHandler::tabBoxAdded, this, &FlipSwitchEffect::slotTabBoxAdded);
    connect(effects, &EffectsHandler::tabBoxClosed, this, &FlipSwitchEffect::slotTabBoxClosed);
    connect(effects, &EffectsHandler::tabBoxUpdated, this, &FlipSwitchEffect::slotTabBoxUpdated);
    connect(effects, &EffectsHandler::tabBoxKeyEvent, this, &FlipSwitchEffect::slotTabBoxKeyEvent);
    connect(effects, &EffectsHandler::screenAboutToLock, this, [this]() {
        setActive(false, AllDesktopsMode);
        setActive(false, CurrentDesktopMode);
    });
}

FlipSwitchEffect::~FlipSwitchEffect()
{
    delete m_captionFrame;
}

bool FlipSwitchEffect::supported()
{
    return effects->isOpenGLCompositing() && effects->animationsSupported();
}

void FlipSwitchEffect::reconfigure(ReconfigureFlags)
{
    FlipSwitchConfig::self()->read();
    m_tabbox = FlipSwitchConfig::tabBox();
    m_tabboxAlternative = FlipSwitchConfig::tabBoxAlternative();
    const int duration = animationTime<FlipSwitchConfig>(200);
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

void FlipSwitchEffect::paintScreen(int mask, const QRegion &region, ScreenPaintData& data)
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
            EffectWindow* w = nullptr;
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
            EffectWindow* w = nullptr;
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


        int winMask = PAINT_WINDOW_TRANSFORMED | PAINT_WINDOW_TRANSLUCENT;
        // fade in/out one window at the end of the stack during animation
        if (m_animation && !m_scheduledDirections.isEmpty()) {
            EffectWindow* w = m_flipOrderedWindows.last();
            if (ItemInfo *info = m_windows.value(w,0)) {
                WindowPaintData data(w);
                if (effects->numScreens() > 1) {
                    data.setProjectionMatrix(m_projectionMatrix);
                    data.setModelViewMatrix(m_modelviewMatrix);
                }
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
            if (effects->numScreens() > 1) {
                data.setProjectionMatrix(m_projectionMatrix);
                data.setModelViewMatrix(m_modelviewMatrix);
            }
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
                    m_currentAnimationEasingCurve = QEasingCurve::OutSine;
                    m_timeLine.setEasingCurve(m_currentAnimationEasingCurve);
                } else {
                    m_currentAnimationEasingCurve = QEasingCurve::Linear;
                    m_timeLine.setEasingCurve(m_currentAnimationEasingCurve);
                }
            }
            effects->addRepaintFull();
        }
        if (m_stop && m_startStopTimeLine.currentValue() == 0.0f) {
            m_stop = false;
            m_active = false;
            m_captionFrame->free();
            effects->setActiveFullScreenEffect(nullptr);
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
                        m_currentAnimationEasingCurve = QEasingCurve::Linear;
                    else
                        m_currentAnimationEasingCurve = QEasingCurve::OutSine;
                } else {
                    m_currentAnimationEasingCurve = QEasingCurve::Linear;
                }
                m_timeLine.setEasingCurve(m_currentAnimationEasingCurve);
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
    effects->setShowingDesktop(false);
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
                if (m_selectedWindow != nullptr) {
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
        m_selectedWindow = nullptr;
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
        m_startStopTimeLine.setEasingCurve(QEasingCurve::InOutSine);
        m_activeScreen = effects->activeScreen();
        m_screenArea = effects->clientArea(ScreenArea, m_activeScreen, effects->currentDesktop());

        if (effects->numScreens() > 1) {
            // unfortunatelly we have to change the projection matrix in dual screen mode
            // code is copied from Coverswitch
            QRect fullRect = effects->clientArea(FullArea, m_activeScreen, effects->currentDesktop());
            float fovy = 60.0f;
            float aspect = 1.0f;
            float zNear = 0.1f;
            float zFar = 100.0f;

            float ymax = zNear * std::tan(fovy * M_PI / 360.0f);
            float ymin = -ymax;
            float xmin =  ymin * aspect;
            float xmax = ymax * aspect;

            if (m_screenArea.width() != fullRect.width()) {
                if (m_screenArea.x() == 0) {
                    // horizontal layout: left screen
                    xmin *= (float)m_screenArea.width() / (float)fullRect.width();
                    xmax *= (fullRect.width() - 0.5f * m_screenArea.width()) / (0.5f * fullRect.width());
                } else {
                    // horizontal layout: right screen
                    xmin *= (fullRect.width() - 0.5f * m_screenArea.width()) / (0.5f * fullRect.width());
                    xmax *= (float)m_screenArea.width() / (float)fullRect.width();
                }
            }
            if (m_screenArea.height() != fullRect.height()) {
                if (m_screenArea.y() == 0) {
                    // vertical layout: top screen
                    ymin *= (fullRect.height() - 0.5f * m_screenArea.height()) / (0.5f * fullRect.height());
                    ymax *= (float)m_screenArea.height() / (float)fullRect.height();
                } else {
                    // vertical layout: bottom screen
                    ymin *= (float)m_screenArea.height() / (float)fullRect.height();
                    ymax *= (fullRect.height() - 0.5f * m_screenArea.height()) / (0.5f * fullRect.height());
                }
            }

            m_projectionMatrix = QMatrix4x4();
            m_projectionMatrix.frustum(xmin, xmax, ymin, ymax, zNear, zFar);

            const float scaleFactor = 1.1f / zNear;

            // Create a second matrix that transforms screen coordinates
            // to world coordinates.
            QMatrix4x4 matrix;
            matrix.translate(xmin * scaleFactor, ymax * scaleFactor, -1.1);
            matrix.scale( (xmax - xmin) * scaleFactor / fullRect.width(),
                        -(ymax - ymin) * scaleFactor / fullRect.height(),
                        0.001);
            // Combine the matrices
            m_projectionMatrix *= matrix;

            m_modelviewMatrix = QMatrix4x4();
            m_modelviewMatrix.translate(m_screenArea.x(), m_screenArea.y(), 0.0);
        }

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
            effects->startMouseInterception(this, Qt::ArrowCursor);
            break;
        case CurrentDesktopMode:
            m_selectedWindow = effects->activeWindow();
            effects->startMouseInterception(this, Qt::BlankCursor);
            m_hasKeyboardGrab = effects->grabKeyboard(this);
            break;
        case AllDesktopsMode:
            m_selectedWindow = effects->activeWindow();
            effects->startMouseInterception(this, Qt::BlankCursor);
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
            m_startStopTimeLine.setEasingCurve(QEasingCurve::OutSine);
            if (m_scheduledDirections.count() == 1) {
                if (m_currentAnimationEasingCurve == QEasingCurve::InOutSine)
                    m_currentAnimationEasingCurve = QEasingCurve::InSine;
                else if (m_currentAnimationEasingCurve == QEasingCurve::OutSine)
                    m_currentAnimationEasingCurve = QEasingCurve::Linear;
                m_timeLine.setEasingCurve(m_currentAnimationEasingCurve);
            }
        } else
            m_startStopTimeLine.setEasingCurve(QEasingCurve::InOutSine);
        effects->stopMouseInterception(this);
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
        m_startStopTimeLine.setEasingCurve(QEasingCurve::InSine);
    }
    if (!m_animation && !m_start) {
        m_animation = true;
        m_scheduledDirections.enqueue(direction);
        distance--;
        // reset shape just to make sure
        m_currentAnimationEasingCurve = QEasingCurve::InOutSine;
        m_timeLine.setEasingCurve(m_currentAnimationEasingCurve);
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
        QEasingCurve curve;
        switch (m_currentAnimationEasingCurve.type()) {
        case QEasingCurve::InOutSine:
            curve = QEasingCurve::InSine;
            break;
        case QEasingCurve::OutSine:
            curve = QEasingCurve::Linear;
            break;
        default:
            curve = m_currentAnimationEasingCurve;
        }
        if (m_currentAnimationEasingCurve != curve) {
            m_currentAnimationEasingCurve = curve;
            m_timeLine.setEasingCurve(curve);
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
void FlipSwitchEffect::globalShortcutChanged(QAction *action, const QKeySequence &shortcut)
{
    if (action->objectName() == QStringLiteral("FlipSwitchAll")) {
        m_shortcutAll.clear();
        m_shortcutAll.append(shortcut);
    } else if (action->objectName() == QStringLiteral("FlipSwitchCurrent")) {
        m_shortcutCurrent.clear();
        m_shortcutCurrent.append(shortcut);
    }
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
    return m_active && !effects->isScreenLocked();
}

void FlipSwitchEffect::updateCaption()
{
    if (!m_selectedWindow) {
        return;
    }
    if (m_selectedWindow->isDesktop()) {
        m_captionFrame->setText(i18nc("Special entry in alt+tab list for minimizing all windows",
                     "Show Desktop"));
        static QPixmap pix = QIcon::fromTheme(QStringLiteral("user-desktop")).pixmap(m_captionFrame->iconSize());
        m_captionFrame->setIcon(pix);
    } else {
        m_captionFrame->setText(m_selectedWindow->caption());
        m_captionFrame->setIcon(m_selectedWindow->icon());
    }
}

//*************************************************************
// Mouse handling
//*************************************************************

void FlipSwitchEffect::windowInputMouseEvent(QEvent* e)
{
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
    case Qt::MiddleButton:
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

