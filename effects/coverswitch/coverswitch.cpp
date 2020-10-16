/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2008 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "coverswitch.h"
// KConfigSkeleton
#include "coverswitchconfig.h"

#include <kwinconfig.h>
#include <QFile>
#include <QFont>
#include <QIcon>
#include <QMatrix4x4>
#include <QMouseEvent>
#include <QFontMetrics>
#include <KLocalizedString>
#include <kcolorscheme.h>

#include <kwinglutils.h>
#include <kwinglplatform.h>

#include <cmath>

namespace KWin
{

CoverSwitchEffect::CoverSwitchEffect()
    : mActivated(0)
    , angle(60.0)
    , animation(false)
    , start(false)
    , stop(false)
    , stopRequested(false)
    , startRequested(false)
    , zPosition(900.0)
    , scaleFactor(0.0)
    , direction(Left)
    , selected_window(nullptr)
    , captionFrame(nullptr)
    , primaryTabBox(false)
    , secondaryTabBox(false)
{
    initConfig<CoverSwitchConfig>();
    reconfigure(ReconfigureAll);

    // Caption frame
    captionFont.setBold(true);
    captionFont.setPointSize(captionFont.pointSize() * 2);

    if (effects->compositingType() == OpenGL2Compositing) {
        m_reflectionShader = ShaderManager::instance()->generateShaderFromResources(ShaderTrait::MapTexture, QString(), QStringLiteral("coverswitch-reflection.glsl"));
    } else {
        m_reflectionShader = nullptr;
    }
    connect(effects, &EffectsHandler::windowClosed, this, &CoverSwitchEffect::slotWindowClosed);
    connect(effects, &EffectsHandler::tabBoxAdded, this, &CoverSwitchEffect::slotTabBoxAdded);
    connect(effects, &EffectsHandler::tabBoxClosed, this, &CoverSwitchEffect::slotTabBoxClosed);
    connect(effects, &EffectsHandler::tabBoxUpdated, this, &CoverSwitchEffect::slotTabBoxUpdated);
    connect(effects, &EffectsHandler::tabBoxKeyEvent, this, &CoverSwitchEffect::slotTabBoxKeyEvent);
}

CoverSwitchEffect::~CoverSwitchEffect()
{
    delete captionFrame;
    delete m_reflectionShader;
}

bool CoverSwitchEffect::supported()
{
    return effects->isOpenGLCompositing() && effects->animationsSupported();
}

void CoverSwitchEffect::reconfigure(ReconfigureFlags)
{
    CoverSwitchConfig::self()->read();
    animationDuration = std::chrono::milliseconds(
        animationTime<CoverSwitchConfig>(200));
    animateSwitch     = CoverSwitchConfig::animateSwitch();
    animateStart      = CoverSwitchConfig::animateStart();
    animateStop       = CoverSwitchConfig::animateStop();
    reflection        = CoverSwitchConfig::reflection();
    windowTitle       = CoverSwitchConfig::windowTitle();
    zPosition         = CoverSwitchConfig::zPosition();
    timeLine.setEasingCurve(QEasingCurve::InOutSine);
    timeLine.setDuration(animationDuration);

    // Defined outside the ui
    primaryTabBox     = CoverSwitchConfig::tabBox();
    secondaryTabBox   = CoverSwitchConfig::tabBoxAlternative();

    QColor tmp        = CoverSwitchConfig::mirrorFrontColor();
    mirrorColor[0][0] = tmp.redF();
    mirrorColor[0][1] = tmp.greenF();
    mirrorColor[0][2] = tmp.blueF();
    mirrorColor[0][3] = 1.0;
    tmp               = CoverSwitchConfig::mirrorRearColor();
    mirrorColor[1][0] = tmp.redF();
    mirrorColor[1][1] = tmp.greenF();
    mirrorColor[1][2] = tmp.blueF();
    mirrorColor[1][3] = -1.0;

}

void CoverSwitchEffect::prePaintScreen(ScreenPrePaintData& data, int time)
{
    if (mActivated || stop || stopRequested) {
        data.mask |= Effect::PAINT_SCREEN_WITH_TRANSFORMED_WINDOWS;
        if (animation || start || stop) {
            timeLine.update(std::chrono::milliseconds(time));
        }
        if (selected_window == nullptr)
            abort();
    }
    effects->prePaintScreen(data, time);
}

void CoverSwitchEffect::paintScreen(int mask, const QRegion &region, ScreenPaintData& data)
{
    effects->paintScreen(mask, region, data);

    if (mActivated || stop || stopRequested) {

        QList< EffectWindow* > tempList = currentWindowList;
        int index = tempList.indexOf(selected_window);
        if (animation || start || stop) {
            if (!start && !stop) {
                if (direction == Right)
                    index++;
                else
                    index--;
                if (index < 0)
                    index = tempList.count() + index;
                if (index >= tempList.count())
                    index = index % tempList.count();
            }
            foreach (Direction direction, scheduled_directions) {
                if (direction == Right)
                    index++;
                else
                    index--;
                if (index < 0)
                    index = tempList.count() + index;
                if (index >= tempList.count())
                    index = index % tempList.count();
            }
        }
        int leftIndex = index - 1;
        if (leftIndex < 0)
            leftIndex = tempList.count() - 1;
        int rightIndex = index + 1;
        if (rightIndex == tempList.count())
            rightIndex = 0;

        EffectWindow* frontWindow = tempList[ index ];
        leftWindows.clear();
        rightWindows.clear();

        bool evenWindows = (tempList.count() % 2 == 0) ? true : false;
        int leftWindowCount = 0;
        if (evenWindows)
            leftWindowCount = tempList.count() / 2 - 1;
        else
            leftWindowCount = (tempList.count() - 1) / 2;
        for (int i = 0; i < leftWindowCount; i++) {
            int tempIndex = (leftIndex - i);
            if (tempIndex < 0)
                tempIndex = tempList.count() + tempIndex;
            leftWindows.prepend(tempList[ tempIndex ]);
        }
        int rightWindowCount = 0;
        if (evenWindows)
            rightWindowCount = tempList.count() / 2;
        else
            rightWindowCount = (tempList.count() - 1) / 2;
        for (int i = 0; i < rightWindowCount; i++) {
            int tempIndex = (rightIndex + i) % tempList.count();
            rightWindows.prepend(tempList[ tempIndex ]);
        }

        if (reflection) {
            // no reflections during start and stop animation
            // except when using a shader
            if ((!start && !stop) || effects->compositingType() == OpenGL2Compositing)
                paintScene(frontWindow, leftWindows, rightWindows, true);
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            // we can use a huge scale factor (needed to calculate the rearground vertices)
            // as we restrict with a PaintClipper painting on the current screen
            float reflectionScaleFactor = 100000 * tan(60.0 * M_PI / 360.0f) / area.width();
            const float width = area.width();
            const float height = area.height();
            float vertices[] = {
                -width * 0.5f, height, 0.0,
                width * 0.5f, height, 0.0,
                width*reflectionScaleFactor, height, -5000,
                -width*reflectionScaleFactor, height, -5000
            };
            // foreground
            if (start) {
                mirrorColor[0][3] = timeLine.value();
            } else if (stop) {
                mirrorColor[0][3] = 1.0 - timeLine.value();
            } else {
                mirrorColor[0][3] = 1.0;
            }

            int y = 0;
            // have to adjust the y values to fit OpenGL
            // in OpenGL y==0 is at bottom, in Qt at top
            if (effects->numScreens() > 1) {
                QRect fullArea = effects->clientArea(FullArea, 0, 1);
                if (fullArea.height() != area.height()) {
                    if (area.y() == 0)
                        y = fullArea.height() - area.height();
                    else
                        y = fullArea.height() - area.y() - area.height();
                }
            }
            // use scissor to restrict painting of the reflection plane to current screen
            glScissor(area.x(), y, area.width(), area.height());
            glEnable(GL_SCISSOR_TEST);

            if (m_reflectionShader && m_reflectionShader->isValid()) {
                ShaderManager::instance()->pushShader(m_reflectionShader);
                QMatrix4x4 windowTransformation = data.projectionMatrix();
                windowTransformation.translate(area.x() + area.width() * 0.5f, 0.0, 0.0);
                m_reflectionShader->setUniform(GLShader::ModelViewProjectionMatrix, windowTransformation);
                m_reflectionShader->setUniform("u_frontColor", QVector4D(mirrorColor[0][0], mirrorColor[0][1], mirrorColor[0][2], mirrorColor[0][3]));
                m_reflectionShader->setUniform("u_backColor", QVector4D(mirrorColor[1][0], mirrorColor[1][1], mirrorColor[1][2], mirrorColor[1][3]));
                // TODO: make this one properly
                QVector<float> verts;
                QVector<float> texcoords;
                verts.reserve(18);
                texcoords.reserve(12);
                texcoords << 1.0 << 0.0;
                verts << vertices[6] << vertices[7] << vertices[8];
                texcoords << 1.0 << 0.0;
                verts << vertices[9] << vertices[10] << vertices[11];
                texcoords << 0.0 << 0.0;
                verts << vertices[0] << vertices[1] << vertices[2];
                texcoords << 0.0 << 0.0;
                verts << vertices[0] << vertices[1] << vertices[2];
                texcoords << 0.0 << 0.0;
                verts << vertices[3] << vertices[4] << vertices[5];
                texcoords << 1.0 << 0.0;
                verts << vertices[6] << vertices[7] << vertices[8];
                GLVertexBuffer *vbo = GLVertexBuffer::streamingBuffer();
                vbo->reset();
                vbo->setData(6, 3, verts.data(), texcoords.data());
                vbo->render(GL_TRIANGLES);

                ShaderManager::instance()->popShader();
            }
            glDisable(GL_SCISSOR_TEST);
            glDisable(GL_BLEND);
        }
        paintScene(frontWindow, leftWindows, rightWindows);

        // Render the caption frame
        if (windowTitle) {
            double opacity = 1.0;
            if (start)
                opacity = timeLine.value();
            else if (stop)
                opacity = 1.0 - timeLine.value();
            if (animation)
                captionFrame->setCrossFadeProgress(timeLine.value());
            captionFrame->render(region, opacity);
        }
    }
}

void CoverSwitchEffect::postPaintScreen()
{
    if ((mActivated && (animation || start)) || stop || stopRequested) {
        if (timeLine.done()) {
            timeLine.reset();
            if (stop) {
                stop = false;
                effects->setActiveFullScreenEffect(nullptr);
                foreach (EffectWindow * window, referrencedWindows) {
                    window->unrefWindow();
                }
                referrencedWindows.clear();
                currentWindowList.clear();
                if (startRequested) {
                    startRequested = false;
                    mActivated = true;
                    effects->refTabBox();
                    currentWindowList = effects->currentTabBoxWindowList();
                    if (animateStart) {
                        start = true;
                    }
                }
            } else if (!scheduled_directions.isEmpty()) {
                direction = scheduled_directions.dequeue();
                if (start) {
                    animation = true;
                    start = false;
                }
            } else {
                animation = false;
                start = false;
                if (stopRequested) {
                    stopRequested = false;
                    stop = true;
                }
            }
        }
        effects->addRepaintFull();
    }
    effects->postPaintScreen();
}

void CoverSwitchEffect::paintScene(EffectWindow* frontWindow, const EffectWindowList& leftWindows,
                                   const EffectWindowList& rightWindows, bool reflectedWindows)
{
    // LAYOUT
    // one window in the front. Other windows left and right rotated
    // for odd number of windows: left: (n-1)/2; front: 1; right: (n-1)/2
    // for even number of windows: left: n/2; front: 1; right: n/2 -1
    //
    // ANIMATION
    // forward (alt+tab)
    // all left windows are moved to next position
    // top most left window is rotated and moved to front window position
    // front window is rotated and moved to next right window position
    // right windows are moved to next position
    // last right window becomes totally transparent in half the time
    // appears transparent on left side and becomes totally opaque again
    // backward (alt+shift+tab) same as forward but opposite direction
    int width = area.width();
    int leftWindowCount = leftWindows.count();
    int rightWindowCount = rightWindows.count();


    // Problem during animation: a window which is painted after another window
    // appears in front of the other
    // so during animation the painting order has to be rearreanged
    // paint sequence no animation: left, right, front
    // paint sequence forward animation: right, front, left

    if (!animation) {
        paintWindows(leftWindows, true, reflectedWindows);
        paintWindows(rightWindows, false, reflectedWindows);
        paintFrontWindow(frontWindow, width, leftWindowCount, rightWindowCount, reflectedWindows);
    } else {
        if (direction == Right) {
            if (timeLine.value() < 0.5) {
                // paint in normal way
                paintWindows(leftWindows, true, reflectedWindows);
                paintWindows(rightWindows, false, reflectedWindows);
                paintFrontWindow(frontWindow, width, leftWindowCount, rightWindowCount, reflectedWindows);
            } else {
                paintWindows(rightWindows, false, reflectedWindows);
                paintFrontWindow(frontWindow, width, leftWindowCount, rightWindowCount, reflectedWindows);
                paintWindows(leftWindows, true, reflectedWindows, rightWindows.at(0));
            }
        } else {
            paintWindows(leftWindows, true, reflectedWindows);
            if (timeLine.value() < 0.5) {
                paintWindows(rightWindows, false, reflectedWindows);
                paintFrontWindow(frontWindow, width, leftWindowCount, rightWindowCount, reflectedWindows);
            } else {
                EffectWindow* leftWindow;
                if (leftWindowCount > 0) {
                    leftWindow = leftWindows.at(0);
                    paintFrontWindow(frontWindow, width, leftWindowCount, rightWindowCount, reflectedWindows);
                } else
                    leftWindow = frontWindow;
                paintWindows(rightWindows, false, reflectedWindows, leftWindow);
            }
        }
    }
}

void CoverSwitchEffect::paintWindow(EffectWindow* w, int mask, QRegion region, WindowPaintData& data)
{
    if (mActivated || stop || stopRequested) {
        if (!(mask & PAINT_WINDOW_TRANSFORMED) && !w->isDesktop()) {
            if ((start || stop) && w->isDock()) {
                data.setOpacity(1.0 - timeLine.value());
                if (stop)
                    data.setOpacity(timeLine.value());
            } else
                return;
        }
    }
    if ((start || stop) && (!w->isOnCurrentDesktop() || w->isMinimized())) {
        if (stop)  // Fade out windows not on the current desktop
            data.setOpacity(1.0 - timeLine.value());
        else // Fade in Windows from other desktops when animation is started
            data.setOpacity(timeLine.value());
    }
    effects->paintWindow(w, mask, region, data);
}

void CoverSwitchEffect::slotTabBoxAdded(int mode)
{
    if (effects->activeFullScreenEffect() && effects->activeFullScreenEffect() != this)
        return;
    if (!mActivated) {
        effects->setShowingDesktop(false);
        // only for windows mode
        if (((mode == TabBoxWindowsMode && primaryTabBox) ||
                (mode == TabBoxWindowsAlternativeMode && secondaryTabBox) ||
                (mode == TabBoxCurrentAppWindowsMode && primaryTabBox) ||
                (mode == TabBoxCurrentAppWindowsAlternativeMode && secondaryTabBox))
                && effects->currentTabBoxWindowList().count() > 0) {
            effects->startMouseInterception(this, Qt::ArrowCursor);
            activeScreen = effects->activeScreen();
            if (!stop && !stopRequested) {
                effects->refTabBox();
                effects->setActiveFullScreenEffect(this);
                scheduled_directions.clear();
                selected_window = effects->currentTabBoxWindow();
                currentWindowList = effects->currentTabBoxWindowList();
                direction = Left;
                mActivated = true;
                if (animateStart) {
                    start = true;
                }

                // Calculation of correct area
                area = effects->clientArea(FullScreenArea, activeScreen, effects->currentDesktop());
                const QSize screenSize = effects->virtualScreenSize();
                scaleFactor = (zPosition + 1100) * 2.0 * tan(60.0 * M_PI / 360.0f) / screenSize.width();
                if (screenSize.width() - area.width() != 0) {
                    // one of the screens is smaller than the other (horizontal)
                    if (area.width() < screenSize.width() - area.width())
                        scaleFactor *= (float)area.width() / (float)(screenSize.width() - area.width());
                    else if (area.width() != screenSize.width() - area.width()) {
                        // vertical layout with different width
                        // but we don't want to catch screens with same width and different height
                        if (screenSize.height() != area.height())
                            scaleFactor *= (float)area.width() / (float)(screenSize.width());
                    }
                }

                if (effects->numScreens() > 1) {
                    // unfortunatelly we have to change the projection matrix in dual screen mode
                    // code is adapted from SceneOpenGL2::createProjectionMatrix()
                    QRect fullRect = effects->clientArea(FullArea, activeScreen, effects->currentDesktop());
                    float fovy = 60.0f;
                    float aspect = 1.0f;
                    float zNear = 0.1f;
                    float zFar = 100.0f;

                    float ymax = zNear * std::tan(fovy * M_PI / 360.0f);
                    float ymin = -ymax;
                    float xmin =  ymin * aspect;
                    float xmax = ymax * aspect;

                    if (area.width() != fullRect.width()) {
                        if (area.x() == 0) {
                            // horizontal layout: left screen
                            xmin *= (float)area.width() / (float)fullRect.width();
                            xmax *= (fullRect.width() - 0.5f * area.width()) / (0.5f * fullRect.width());
                        } else {
                            // horizontal layout: right screen
                            xmin *= (fullRect.width() - 0.5f * area.width()) / (0.5f * fullRect.width());
                            xmax *= (float)area.width() / (float)fullRect.width();
                        }
                    }
                    if (area.height() != fullRect.height()) {
                        if (area.y() == 0) {
                            // vertical layout: top screen
                            ymin *= (fullRect.height() - 0.5f * area.height()) / (0.5f * fullRect.height());
                            ymax *= (float)area.height() / (float)fullRect.height();
                        } else {
                            // vertical layout: bottom screen
                            ymin *= (float)area.height() / (float)fullRect.height();
                            ymax *= (fullRect.height() - 0.5f * area.height()) / (0.5f * fullRect.height());
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
                    m_modelviewMatrix.translate(area.x(), area.y(), 0.0);
                }

                // Setup caption frame geometry
                if (windowTitle) {
                    QRect frameRect = QRect(area.width() * 0.25f + area.x(),
                                            area.height() * 0.9f + area.y(),
                                            area.width() * 0.5f,
                                            QFontMetrics(captionFont).height());
                    if (!captionFrame) {
                        captionFrame = effects->effectFrame(EffectFrameStyled);
                        captionFrame->setFont(captionFont);
                        captionFrame->enableCrossFade(true);
                    }
                    captionFrame->setGeometry(frameRect);
                    captionFrame->setIconSize(QSize(frameRect.height(), frameRect.height()));
                    // And initial contents
                    updateCaption();
                }

                effects->addRepaintFull();
            } else {
                startRequested = true;
            }
        }
    }
}

void CoverSwitchEffect::slotTabBoxClosed()
{
    if (mActivated) {
        if (animateStop) {
            if (!animation && !start) {
                stop = true;
            } else if (start && scheduled_directions.isEmpty()) {
                start = false;
                stop = true;
                timeLine.setElapsed(timeLine.duration() - timeLine.elapsed());
            } else {
                stopRequested = true;
            }
        } else {
            effects->setActiveFullScreenEffect(nullptr);
            start = false;
            animation = false;
            timeLine.reset();
        }
        mActivated = false;
        effects->unrefTabBox();
        effects->stopMouseInterception(this);
        effects->addRepaintFull();
    }
}

void CoverSwitchEffect::slotTabBoxUpdated()
{
    if (mActivated) {
        if (animateSwitch && currentWindowList.count() > 1) {
            // determine the switch direction
            if (selected_window != effects->currentTabBoxWindow()) {
                if (selected_window != nullptr) {
                    int old_index = currentWindowList.indexOf(selected_window);
                    int new_index = effects->currentTabBoxWindowList().indexOf(effects->currentTabBoxWindow());
                    Direction new_direction;
                    int distance = new_index - old_index;
                    if (distance > 0)
                        new_direction = Left;
                    if (distance < 0)
                        new_direction = Right;
                    if (effects->currentTabBoxWindowList().count() == 2) {
                        new_direction = Left;
                        distance = 1;
                    }
                    if (distance != 0) {
                        distance = abs(distance);
                        int tempDistance = effects->currentTabBoxWindowList().count() - distance;
                        if (tempDistance < abs(distance)) {
                            distance = tempDistance;
                            if (new_direction == Left)
                                new_direction = Right;
                            else
                                new_direction = Left;
                        }
                        if (!animation && !start) {
                            animation = true;
                            direction = new_direction;
                            distance--;
                        }
                        for (int i = 0; i < distance; i++) {
                            if (!scheduled_directions.isEmpty() && scheduled_directions.last() != new_direction)
                                scheduled_directions.pop_back();
                            else
                                scheduled_directions.enqueue(new_direction);
                            if (scheduled_directions.count() == effects->currentTabBoxWindowList().count())
                                scheduled_directions.clear();
                        }
                    }
                }
                selected_window = effects->currentTabBoxWindow();
                currentWindowList = effects->currentTabBoxWindowList();
                updateCaption();
            }
        }
        effects->addRepaintFull();
    }
}

void CoverSwitchEffect::paintWindowCover(EffectWindow* w, bool reflectedWindow, WindowPaintData& data)
{
    QRect windowRect = w->geometry();
    data.setYTranslation(area.height() - windowRect.y() - windowRect.height());
    data.setZTranslation(-zPosition);
    if (start) {
        if (w->isMinimized()) {
            data.multiplyOpacity(timeLine.value());
        } else {
            const QVector3D translation = data.translation() * timeLine.value();
            data.setXTranslation(translation.x());
            data.setYTranslation(translation.y());
            data.setZTranslation(translation.z());
            if (effects->numScreens() > 1) {
                QRect clientRect = effects->clientArea(FullScreenArea, w->screen(), effects->currentDesktop());
                QRect fullRect = effects->clientArea(FullArea, activeScreen, effects->currentDesktop());
                if (w->screen() == activeScreen) {
                    if (clientRect.width() != fullRect.width() && clientRect.x() != fullRect.x()) {
                        data.translate(- clientRect.x() * (1.0f - timeLine.value()));
                    }
                    if (clientRect.height() != fullRect.height() && clientRect.y() != fullRect.y()) {
                        data.translate(0.0, - clientRect.y() * (1.0f - timeLine.value()));
                    }
                } else {
                    if (clientRect.width() != fullRect.width() && clientRect.x() < area.x()) {
                        data.translate(- clientRect.width() * (1.0f - timeLine.value()));
                    }
                    if (clientRect.height() != fullRect.height() && clientRect.y() < area.y()) {
                        data.translate(0.0, - clientRect.height() * (1.0f - timeLine.value()));
                    }
                }
            }
            data.setRotationAngle(data.rotationAngle() * timeLine.value());
        }
    }
    if (stop) {
        if (w->isMinimized() && w != effects->activeWindow()) {
            data.multiplyOpacity(1.0 - timeLine.value());
        } else {
            const QVector3D translation = data.translation() * (1.0 - timeLine.value());
            data.setXTranslation(translation.x());
            data.setYTranslation(translation.y());
            data.setZTranslation(translation.z());
            if (effects->numScreens() > 1) {
                QRect clientRect = effects->clientArea(FullScreenArea, w->screen(), effects->currentDesktop());
                QRect rect = effects->clientArea(FullScreenArea, activeScreen, effects->currentDesktop());
                QRect fullRect = effects->clientArea(FullArea, activeScreen, effects->currentDesktop());
                if (w->screen() == activeScreen) {
                    if (clientRect.width() != fullRect.width() && clientRect.x() != fullRect.x()) {
                        data.translate(- clientRect.x() * timeLine.value());
                    }
                    if (clientRect.height() != fullRect.height() && clientRect.y() != fullRect.y()) {
                        data.translate(0.0, - clientRect.y() * timeLine.value());
                    }
                } else {
                    if (clientRect.width() != fullRect.width() && clientRect.x() < rect.x()) {
                        data.translate(- clientRect.width() * timeLine.value());
                    }
                    if (clientRect.height() != fullRect.height() && clientRect.y() < area.y()) {
                        data.translate(0.0, - clientRect.height() * timeLine.value());
                    }
                }
            }
            data.setRotationAngle(data.rotationAngle() * (1.0 - timeLine.value()));
        }
    }

    if (reflectedWindow) {
        QMatrix4x4 reflectionMatrix;
        reflectionMatrix.scale(1.0, -1.0, 1.0);
        data.setModelViewMatrix(reflectionMatrix*data.modelViewMatrix());
        data.setYTranslation(- area.height() - windowRect.y() - windowRect.height());
        if (start) {
            data.multiplyOpacity(timeLine.value());
        } else if (stop) {
            data.multiplyOpacity(1.0 - timeLine.value());
        }
        effects->drawWindow(w,
                                PAINT_WINDOW_TRANSFORMED,
                                infiniteRegion(), data);
    } else {
        effects->paintWindow(w,
                             PAINT_WINDOW_TRANSFORMED,
                             infiniteRegion(), data);
    }
}

void CoverSwitchEffect::paintFrontWindow(EffectWindow* frontWindow, int width, int leftWindows, int rightWindows, bool reflectedWindow)
{
    if (frontWindow == nullptr)
        return;
    bool specialHandlingForward = false;
    WindowPaintData data(frontWindow);
    if (effects->numScreens() > 1) {
        data.setProjectionMatrix(m_projectionMatrix);
        data.setModelViewMatrix(m_modelviewMatrix);
    }
    data.setXTranslation(area.width() * 0.5 - frontWindow->geometry().x() - frontWindow->geometry().width() * 0.5);
    if (leftWindows == 0) {
        leftWindows = 1;
        if (!start && !stop)
            specialHandlingForward = true;
    }
    if (rightWindows == 0) {
        rightWindows = 1;
    }
    if (animation) {
      float distance = 0.0;
      const QSize screenSize = effects->virtualScreenSize();
      if (direction == Right) {
            // move to right
            distance = -frontWindow->geometry().width() * 0.5f + area.width() * 0.5f +
                       (((float)screenSize.width() * 0.5 * scaleFactor) - (float)area.width() * 0.5f) / rightWindows;
            data.translate(distance * timeLine.value());
            data.setRotationAxis(Qt::YAxis);
            data.setRotationAngle(-angle * timeLine.value());
            data.setRotationOrigin(QVector3D(frontWindow->geometry().width(), 0.0, 0.0));
        } else {
            // move to left
            distance = frontWindow->geometry().width() * 0.5f - area.width() * 0.5f +
                       ((float)width * 0.5f - ((float)screenSize.width() * 0.5 * scaleFactor)) / leftWindows;
            float factor = 1.0;
            if (specialHandlingForward)
                factor = 2.0;
            data.translate(distance * timeLine.value() * factor);
            data.setRotationAxis(Qt::YAxis);
            data.setRotationAngle(angle * timeLine.value());
        }
    }
    if (specialHandlingForward && timeLine.value() < 0.5) {
        data.multiplyOpacity(1.0 - timeLine.value() * 2.0);
    }
    paintWindowCover(frontWindow, reflectedWindow, data);
}

void CoverSwitchEffect::paintWindows(const EffectWindowList& windows, bool left, bool reflectedWindows, EffectWindow* additionalWindow)
{
    int width = area.width();
    int windowCount = windows.count();
    EffectWindow* window;

    int rotateFactor = 1;
    if (!left) {
        rotateFactor = -1;
    }

    const QSize screenSize = effects->virtualScreenSize();
    float xTranslate = -((float)(width) * 0.5f - ((float)screenSize.width() * 0.5 * scaleFactor));
    if (!left)
        xTranslate = ((float)screenSize.width() * 0.5 * scaleFactor) - (float)width * 0.5f;
    // handling for additional window from other side
    // has to appear on this side after half of the time
    if (animation && timeLine.value() >= 0.5 && additionalWindow != nullptr) {
        WindowPaintData data(additionalWindow);
        if (effects->numScreens() > 1) {
            data.setProjectionMatrix(m_projectionMatrix);
            data.setModelViewMatrix(m_modelviewMatrix);
        }
        data.setRotationAxis(Qt::YAxis);
        data.setRotationAngle(angle * rotateFactor);
        if (left) {
            data.translate(-xTranslate - additionalWindow->geometry().x());
        }
        else {
            data.translate(xTranslate + area.width() -
                           additionalWindow->geometry().x() - additionalWindow->geometry().width());
            data.setRotationOrigin(QVector3D(additionalWindow->geometry().width(), 0.0, 0.0));
        }
        data.multiplyOpacity((timeLine.value() - 0.5) * 2.0);
        paintWindowCover(additionalWindow, reflectedWindows, data);
    }
    // normal behaviour
    for (int i = 0; i < windows.count(); i++) {
        window = windows.at(i);
        if (window == nullptr || window->isDeleted()) {
            continue;
        }
        WindowPaintData data(window);
        if (effects->numScreens() > 1) {
            data.setProjectionMatrix(m_projectionMatrix);
            data.setModelViewMatrix(m_modelviewMatrix);
        }
        data.setRotationAxis(Qt::YAxis);
        data.setRotationAngle(angle);
        if (left)
            data.translate(-xTranslate + xTranslate * i / windowCount - window->geometry().x());
        else
            data.translate(xTranslate + width - xTranslate * i / windowCount - window->geometry().x() - window->geometry().width());
        if (animation) {
            if (direction == Right) {
                if ((i == windowCount - 1) && left) {
                    // right most window on left side -> move to front
                    // have to move one window distance plus half the difference between the window and the desktop size
                    data.translate((xTranslate / windowCount + (width - window->geometry().width()) * 0.5f) * timeLine.value());
                    data.setRotationAngle(angle - angle * timeLine.value());
                }
                // right most window does not have to be moved
                else if (!left && (i == 0));     // do nothing
                else {
                    // all other windows - move to next position
                    data.translate(xTranslate / windowCount * timeLine.value());
                }
            } else {
                if ((i == windowCount - 1) && !left) {
                    // left most window on right side -> move to front
                    data.translate(- (xTranslate / windowCount + (width - window->geometry().width()) * 0.5f) * timeLine.value());
                    data.setRotationAngle(angle - angle * timeLine.value());
                }
                // left most window does not have to be moved
                else if (i == 0 && left); // do nothing
                else {
                    // all other windows - move to next position
                    data.translate(- xTranslate / windowCount * timeLine.value());
                }
            }
        }
        if (!left)
            data.setRotationOrigin(QVector3D(window->geometry().width(), 0.0, 0.0));
        data.setRotationAngle(data.rotationAngle() * rotateFactor);
        // make window most to edge transparent if animation
        if (animation && i == 0 && ((direction == Left && left) || (direction == Right && !left))) {
            // only for the first half of the animation
            if (timeLine.value() < 0.5) {
                data.multiplyOpacity((1.0 - timeLine.value() * 2.0));
                paintWindowCover(window, reflectedWindows, data);
            }
        } else {
            paintWindowCover(window, reflectedWindows, data);
        }
    }
}

void CoverSwitchEffect::windowInputMouseEvent(QEvent* e)
{
    if (e->type() != QEvent::MouseButtonPress)
        return;
    // we don't want click events during animations
    if (animation)
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
        QPoint pos = event->pos();

        // determine if a window has been clicked
        // not interested in events above a fullscreen window (ignoring panel size)
        if (pos.y() < (area.height()*scaleFactor - area.height()) * 0.5f *(1.0f / scaleFactor))
            return;

        // if there is no selected window (that is no window at all) we cannot click it
        if (!selected_window)
            return;

        if (pos.x() < (area.width()*scaleFactor - selected_window->width()) * 0.5f *(1.0f / scaleFactor)) {
            float availableSize = (area.width() * scaleFactor - area.width()) * 0.5f * (1.0f / scaleFactor);
            for (int i = 0; i < leftWindows.count(); i++) {
                int windowPos = availableSize / leftWindows.count() * i;
                if (pos.x() < windowPos)
                    continue;
                if (i + 1 < leftWindows.count()) {
                    if (pos.x() > availableSize / leftWindows.count()*(i + 1))
                        continue;
                }

                effects->setTabBoxWindow(leftWindows[i]);
                return;
            }
        }

        if (pos.x() > area.width() - (area.width()*scaleFactor - selected_window->width()) * 0.5f *(1.0f / scaleFactor)) {
            float availableSize = (area.width() * scaleFactor - area.width()) * 0.5f * (1.0f / scaleFactor);
            for (int i = 0; i < rightWindows.count(); i++) {
                int windowPos = area.width() - availableSize / rightWindows.count() * i;
                if (pos.x() > windowPos)
                    continue;
                if (i + 1 < rightWindows.count()) {
                    if (pos.x() < area.width() - availableSize / rightWindows.count()*(i + 1))
                        continue;
                }

                effects->setTabBoxWindow(rightWindows[i]);
                return;
            }
        }
        break;
    }
}

void CoverSwitchEffect::abort()
{
    // it's possible that abort is called after tabbox has been closed
    // in this case the cleanup is already done (see bug 207554)
    if (mActivated) {
        effects->unrefTabBox();
        effects->stopMouseInterception(this);
    }
    effects->setActiveFullScreenEffect(nullptr);
    timeLine.reset();
    mActivated = false;
    stop = false;
    stopRequested = false;
    effects->addRepaintFull();
    captionFrame->free();
}

void CoverSwitchEffect::slotWindowClosed(EffectWindow* c)
{
    if (c == selected_window)
        selected_window = nullptr;
    // if the list is not empty, the effect is active
    if (!currentWindowList.isEmpty()) {
        c->refWindow();
        referrencedWindows.append(c);
        currentWindowList.removeAll(c);
        leftWindows.removeAll(c);
        rightWindows.removeAll(c);
    }
}

bool CoverSwitchEffect::isActive() const
{
    return (mActivated || stop || stopRequested) && !effects->isScreenLocked();
}

void CoverSwitchEffect::updateCaption()
{
    if (!selected_window || !windowTitle) {
        return;
    }
    if (selected_window->isDesktop()) {
        captionFrame->setText(i18nc("Special entry in alt+tab list for minimizing all windows",
                     "Show Desktop"));
        static QPixmap pix = QIcon::fromTheme(QStringLiteral("user-desktop")).pixmap(captionFrame->iconSize());
        captionFrame->setIcon(pix);
    } else {
        captionFrame->setText(selected_window->caption());
        captionFrame->setIcon(selected_window->icon());
    }
}

void CoverSwitchEffect::slotTabBoxKeyEvent(QKeyEvent *event)
{
    if (event->type() == QEvent::KeyPress) {
        switch (event->key()) {
        case Qt::Key_Left:
            selectPreviousWindow();
            break;
        case Qt::Key_Right:
            selectNextWindow();
            break;
        default:
            // nothing
            break;
        }
    }
}

void CoverSwitchEffect::selectNextOrPreviousWindow(bool forward)
{
    if (!mActivated || !selected_window) {
        return;
    }
    const int index = effects->currentTabBoxWindowList().indexOf(selected_window);
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

} // namespace
