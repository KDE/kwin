/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

 Copyright (C) 2008 Martin Gräßlin <ubuntu@martin-graesslin.com>

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
#include "coverswitch.h"

#include <kwinconfig.h>
#include <QFont>
#include <QMatrix4x4>
#include <QMouseEvent>
#include <klocale.h>
#include <kapplication.h>
#include <kcolorscheme.h>
#include <kconfiggroup.h>
#include <kglobal.h>
#include <kstandarddirs.h>
#include <KDE/KIcon>

#include <kwinglutils.h>

#include <math.h>

#include <kdebug.h>

#include "../boxswitch/boxswitch_proxy.h"

namespace KWin
{

KWIN_EFFECT(coverswitch, CoverSwitchEffect)
KWIN_EFFECT_SUPPORTED(coverswitch, CoverSwitchEffect::supported())

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
    , selected_window(0)
    , captionFrame(NULL)
    , primaryTabBox(false)
    , secondaryTabBox(false)
{
    reconfigure(ReconfigureAll);

    // Caption frame
    captionFont.setBold(true);
    captionFont.setPointSize(captionFont.pointSize() * 2);

    const QString fragmentshader = KGlobal::dirs()->findResource("data", "kwin/coverswitch-reflection.glsl");
    m_reflectionShader = ShaderManager::instance()->loadFragmentShader(ShaderManager::GenericShader, fragmentshader);
    connect(effects, SIGNAL(windowClosed(KWin::EffectWindow*)), this, SLOT(slotWindowClosed(KWin::EffectWindow*)));
    connect(effects, SIGNAL(tabBoxAdded(int)), this, SLOT(slotTabBoxAdded(int)));
    connect(effects, SIGNAL(tabBoxClosed()), this, SLOT(slotTabBoxClosed()));
    connect(effects, SIGNAL(tabBoxUpdated()), this, SLOT(slotTabBoxUpdated()));
    connect(effects, SIGNAL(tabBoxKeyEvent(QKeyEvent*)), this, SLOT(slotTabBoxKeyEvent(QKeyEvent*)));
}

CoverSwitchEffect::~CoverSwitchEffect()
{
    delete captionFrame;
    delete m_reflectionShader;
}

bool CoverSwitchEffect::supported()
{
    return effects->compositingType() == OpenGLCompositing;
}

void CoverSwitchEffect::reconfigure(ReconfigureFlags)
{
    KConfigGroup conf = effects->effectConfig("CoverSwitch");
    animationDuration = animationTime(conf, "Duration", 200);
    animateSwitch     = conf.readEntry("AnimateSwitch", true);
    animateStart      = conf.readEntry("AnimateStart", true);
    animateStop       = conf.readEntry("AnimateStop", true);
    reflection        = conf.readEntry("Reflection", true);
    windowTitle       = conf.readEntry("WindowTitle", true);
    zPosition         = conf.readEntry("ZPosition", 900.0);
    thumbnails        = conf.readEntry("Thumbnails", true);
    dynamicThumbnails = conf.readEntry("DynamicThumbnails", true);
    thumbnailWindows  = conf.readEntry("ThumbnailWindows", 8);
    timeLine.setCurveShape(QTimeLine::EaseInOutCurve);
    timeLine.setDuration(animationDuration);
    primaryTabBox = conf.readEntry("TabBox", false);
    secondaryTabBox = conf.readEntry("TabBoxAlternative", false);
    QColor tmp        = conf.readEntry("MirrorFrontColor", QColor(0, 0, 0));
    mirrorColor[0][0] = tmp.redF();
    mirrorColor[0][1] = tmp.greenF();
    mirrorColor[0][2] = tmp.blueF();
    mirrorColor[0][3] = 1.0;
    tmp               = conf.readEntry("MirrorRearColor", QColor(0, 0, 0));
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
            timeLine.setCurrentTime(timeLine.currentTime() + time);
        }
        if (selected_window == NULL)
            abort();
    }
    effects->prePaintScreen(data, time);
}

void CoverSwitchEffect::paintScreen(int mask, QRegion region, ScreenPaintData& data)
{
    effects->paintScreen(mask, region, data);

    if (mActivated || stop || stopRequested) {
        QMatrix4x4 origProjection;
        QMatrix4x4 origModelview;
        ShaderManager *shaderManager = ShaderManager::instance();
        if (effects->numScreens() > 1) {
            // unfortunatelly we have to change the projection matrix in dual screen mode
            QRect fullRect = effects->clientArea(FullArea, activeScreen, effects->currentDesktop());
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
            if (area.x() == 0 && area.width() != fullRect.width()) {
                // horizontal layout: left screen
                xminFactor = (float)area.width() / (float)fullRect.width();
                xmaxFactor = ((float)fullRect.width() - (float)area.width() * 0.5f) / ((float)fullRect.width() * 0.5f);
                xTranslate = (float)fullRect.width() * 0.5f - (float)area.width() * 0.5f;
            }
            if (area.x() != 0 && area.width() != fullRect.width()) {
                // horizontal layout: right screen
                xminFactor = ((float)fullRect.width() - (float)area.width() * 0.5f) / ((float)fullRect.width() * 0.5f);
                xmaxFactor = (float)area.width() / (float)fullRect.width();
                xTranslate = (float)fullRect.width() * 0.5f - (float)area.width() * 0.5f;
            }
            if (area.y() == 0 && area.height() != fullRect.height()) {
                // vertical layout: top screen
                yminFactor = ((float)fullRect.height() - (float)area.height() * 0.5f) / ((float)fullRect.height() * 0.5f);
                ymaxFactor = (float)area.height() / (float)fullRect.height();
                yTranslate = (float)fullRect.height() * 0.5f - (float)area.height() * 0.5f;
            }
            if (area.y() != 0 && area.height() != fullRect.height()) {
                // vertical layout: bottom screen
                yminFactor = (float)area.height() / (float)fullRect.height();
                ymaxFactor = ((float)fullRect.height() - (float)area.height() * 0.5f) / ((float)fullRect.height() * 0.5f);
                yTranslate = (float)fullRect.height() * 0.5f - (float)area.height() * 0.5f;
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
            if ((!start && !stop) || ShaderManager::instance()->isValid())
                paintScene(frontWindow, leftWindows, rightWindows, true);
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
#ifndef KWIN_HAVE_OPENGLES
            glPolygonMode(GL_FRONT, GL_FILL);
#endif
            QRect fullRect = effects->clientArea(FullArea, activeScreen, effects->currentDesktop());
            // we can use a huge scale factor (needed to calculate the rearground vertices)
            // as we restrict with a PaintClipper painting on the current screen
            float reflectionScaleFactor = 100000 * tan(60.0 * M_PI / 360.0f) / area.width();
            float vertices[] = {
                -area.width() * 0.5f, area.height(), 0.0,
                area.width() * 0.5f, area.height(), 0.0,
                (float)area.width()*reflectionScaleFactor, area.height(), -5000,
                -(float)area.width()*reflectionScaleFactor, area.height(), -5000
            };
            // foreground
            if (start) {
                mirrorColor[0][3] = timeLine.currentValue();
            } else if (stop) {
                mirrorColor[0][3] = 1.0 - timeLine.currentValue();
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

            if (shaderManager->isValid() && m_reflectionShader->isValid()) {
                shaderManager->pushShader(m_reflectionShader);
                QMatrix4x4 windowTransformation;
                windowTransformation.translate(area.x() + area.width() * 0.5f, 0.0, 0.0);
                m_reflectionShader->setUniform("windowTransformation", windowTransformation);
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

                shaderManager->popShader();
            } else {
#ifndef KWIN_HAVE_OPENGLES
                glPushMatrix();
                if (effects->numScreens() > 1 && area.x() != fullRect.x()) {
                    // have to change the reflection area in horizontal layout and right screen
                    glTranslatef(-area.x(), 0.0, 0.0);
                }
                glTranslatef(area.x() + area.width() * 0.5f, 0.0, 0.0);
                glColor4fv(mirrorColor[0]);
                glBegin(GL_POLYGON);
                glVertex3f(vertices[0], vertices[1], vertices[2]);
                glVertex3f(vertices[3], vertices[4], vertices[5]);
                // rearground
                glColor4fv(mirrorColor[1]);
                glVertex3f(vertices[6], vertices[7], vertices[8]);
                glVertex3f(vertices[9], vertices[10], vertices[11]);
                glEnd();

                glPopMatrix();
#endif
            }
            glDisable(GL_SCISSOR_TEST);
            glDisable(GL_BLEND);
        }
        paintScene(frontWindow, leftWindows, rightWindows);

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

        // Render the caption frame
        if (windowTitle) {
            double opacity = 1.0;
            if (start)
                opacity = timeLine.currentValue();
            else if (stop)
                opacity = 1.0 - timeLine.currentValue();
            if (animation)
                captionFrame->setCrossFadeProgress(timeLine.currentValue());
            captionFrame->render(region, opacity);
        }

        if ((thumbnails && (!dynamicThumbnails ||
                           (dynamicThumbnails && currentWindowList.size() >= thumbnailWindows)))
                && !(start || stop)) {
            BoxSwitchEffectProxy *proxy =
                static_cast<BoxSwitchEffectProxy*>(effects->getProxy("boxswitch"));
            if (proxy)
                proxy->paintWindowsBox(region);
        }
    }
}

void CoverSwitchEffect::postPaintScreen()
{
    if ((mActivated && (animation || start)) || stop || stopRequested) {
        if (timeLine.currentValue() == 1.0) {
            timeLine.setCurrentTime(0);
            if (stop) {
                stop = false;
                effects->setActiveFullScreenEffect(0);
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
    RotationData rot;
    rot.axis = RotationData::YAxis;


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
            if (timeLine.currentValue() < 0.5) {
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
            if (timeLine.currentValue() < 0.5) {
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
                data.opacity = 1.0 - timeLine.currentValue();
                if (stop)
                    data.opacity = timeLine.currentValue();
            } else
                return;
        }
    }
    if ((start || stop) && (!w->isOnCurrentDesktop() || w->isMinimized())) {
        if (stop)  // Fade out windows not on the current desktop
            data.opacity = (1.0 - timeLine.currentValue());
        else // Fade in Windows from other desktops when animation is started
            data.opacity = timeLine.currentValue();
    }
    effects->paintWindow(w, mask, region, data);
}

void CoverSwitchEffect::slotTabBoxAdded(int mode)
{
    if (effects->activeFullScreenEffect() && effects->activeFullScreenEffect() != this)
        return;
    if (!mActivated) {
        // only for windows mode
        if (((mode == TabBoxWindowsMode && primaryTabBox) ||
                (mode == TabBoxWindowsAlternativeMode && secondaryTabBox))
                && effects->currentTabBoxWindowList().count() > 0) {
            input = effects->createFullScreenInputWindow(this, Qt::ArrowCursor);
            activeScreen = effects->activeScreen();
            BoxSwitchEffectProxy *proxy =
                static_cast<BoxSwitchEffectProxy*>(effects->getProxy("boxswitch"));
            if (proxy)
                proxy->activate(mode, true, false, 0.05f);
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
                scaleFactor = (zPosition + 1100) * 2.0 * tan(60.0 * M_PI / 360.0f) / displayWidth();
                if (displayWidth() - area.width() != 0) {
                    // one of the screens is smaller than the other (horizontal)
                    if (area.width() < displayWidth() - area.width())
                        scaleFactor *= (float)area.width() / (float)(displayWidth() - area.width());
                    else if (area.width() != displayWidth() - area.width()) {
                        // vertical layout with different width
                        // but we don't want to catch screens with same width and different height
                        if (displayHeight() != area.height())
                            scaleFactor *= (float)area.width() / (float)(displayWidth());
                    }
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
                timeLine.setCurrentTime(timeLine.duration() - timeLine.currentValue());
            } else {
                stopRequested = true;
            }
        } else
            effects->setActiveFullScreenEffect(0);
        mActivated = false;
        effects->unrefTabBox();
        effects->destroyInputWindow(input);
        effects->addRepaintFull();
    }
}

void CoverSwitchEffect::slotTabBoxUpdated()
{
    if (mActivated) {
        if (animateSwitch && currentWindowList.count() > 1) {
            // determine the switch direction
            if (selected_window != effects->currentTabBoxWindow()) {
                if (selected_window != NULL) {
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
    data.yTranslate = area.height() - windowRect.y() - windowRect.height();
    data.zTranslate = -zPosition;
    if (start) {
        if (w->isMinimized()) {
            data.opacity *= timeLine.currentValue();
        } else {
            data.xTranslate *= timeLine.currentValue();
            data.yTranslate *= timeLine.currentValue();
            if (effects->numScreens() > 1) {
                QRect clientRect = effects->clientArea(FullScreenArea, w->screen(), effects->currentDesktop());
                QRect fullRect = effects->clientArea(FullArea, activeScreen, effects->currentDesktop());
                if (w->screen() == activeScreen) {
                    if (clientRect.width() != fullRect.width() && clientRect.x() != fullRect.x()) {
                        data.xTranslate -= clientRect.x() * (1.0f - timeLine.currentValue());
                    }
                    if (clientRect.height() != fullRect.height() && clientRect.y() != fullRect.y()) {
                        data.yTranslate -= clientRect.y() * (1.0f - timeLine.currentValue());
                    }
                } else {
                    if (clientRect.width() != fullRect.width() && clientRect.x() < area.x()) {
                        data.xTranslate -= clientRect.width() * (1.0f - timeLine.currentValue());
                    }
                    if (clientRect.height() != fullRect.height() && clientRect.y() < area.y()) {
                        data.yTranslate -= clientRect.height() * (1.0f - timeLine.currentValue());
                    }
                }
            }
            data.zTranslate *= timeLine.currentValue();
            if (data.rotation)
                data.rotation->angle *= timeLine.currentValue();
        }
    }
    if (stop) {
        if (w->isMinimized() && w != effects->activeWindow()) {
            data.opacity *= (1.0 - timeLine.currentValue());
        } else {
            data.xTranslate *= (1.0 - timeLine.currentValue());
            data.yTranslate *= (1.0 - timeLine.currentValue());
            if (effects->numScreens() > 1) {
                QRect clientRect = effects->clientArea(FullScreenArea, w->screen(), effects->currentDesktop());
                QRect rect = effects->clientArea(FullScreenArea, activeScreen, effects->currentDesktop());
                QRect fullRect = effects->clientArea(FullArea, activeScreen, effects->currentDesktop());
                if (w->screen() == activeScreen) {
                    if (clientRect.width() != fullRect.width() && clientRect.x() != fullRect.x()) {
                        data.xTranslate -= clientRect.x() * timeLine.currentValue();
                    }
                    if (clientRect.height() != fullRect.height() && clientRect.y() != fullRect.y()) {
                        data.yTranslate -= clientRect.y() * timeLine.currentValue();
                    }
                } else {
                    if (clientRect.width() != fullRect.width() && clientRect.x() < rect.x()) {
                        data.xTranslate -= clientRect.width() * timeLine.currentValue();
                    }
                    if (clientRect.height() != fullRect.height() && clientRect.y() < area.y()) {
                        data.yTranslate -= clientRect.height() * timeLine.currentValue();
                    }
                }
            }
            data.zTranslate *= (1.0 - timeLine.currentValue());
            if (data.rotation)
                data.rotation->angle *= (1.0 - timeLine.currentValue());
        }
    }

    QRect thumbnail = infiniteRegion();

    if (reflectedWindow) {
        if (ShaderManager::instance()->isValid()) {
            GLShader *shader = ShaderManager::instance()->pushShader(ShaderManager::GenericShader);
            QMatrix4x4 origMatrix = shader->getUniformMatrix4x4("screenTransformation");
            QMatrix4x4 reflectionMatrix;
            reflectionMatrix.scale(1.0, -1.0, 1.0);
            shader->setUniform("screenTransformation", origMatrix * reflectionMatrix);
            data.yTranslate = - area.height() - windowRect.y() - windowRect.height();
            if (start) {
                data.opacity *= timeLine.currentValue();
            } else if (stop) {
                data.opacity *= 1.0 - timeLine.currentValue();
            }
            effects->drawWindow(w,
                                 PAINT_WINDOW_TRANSFORMED,
                                 infiniteRegion(), data);
            shader->setUniform("screenTransformation", origMatrix);
            ShaderManager::instance()->popShader();
        } else {
#ifndef KWIN_HAVE_OPENGLES
            glPushMatrix();
            glScalef(1.0, -1.0, 1.0);
            data.yTranslate = - area.height() - windowRect.y() - windowRect.height();
            effects->paintWindow(w,
                                 PAINT_WINDOW_TRANSFORMED,
                                 infiniteRegion(), data);
            glPopMatrix();
#endif
        }
    } else {
        effects->paintWindow(w,
                             PAINT_WINDOW_TRANSFORMED,
                             infiniteRegion(), data);
    }
}

void CoverSwitchEffect::paintFrontWindow(EffectWindow* frontWindow, int width, int leftWindows, int rightWindows, bool reflectedWindow)
{
    if (frontWindow == NULL)
        return;
    bool specialHandlingForward = false;
    WindowPaintData data(frontWindow);
    data.xTranslate = area.width() * 0.5 - frontWindow->geometry().x() - frontWindow->geometry().width() * 0.5;
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
      if (direction == Right) {
            // move to right
            distance = -frontWindow->geometry().width() * 0.5f + area.width() * 0.5f +
                       (((float)displayWidth() * 0.5 * scaleFactor) - (float)area.width() * 0.5f) / rightWindows;
            data.xTranslate += distance * timeLine.currentValue();
            RotationData rot;
            rot.axis = RotationData::YAxis;
            rot.angle = -angle * timeLine.currentValue();
            rot.xRotationPoint = frontWindow->geometry().width();
            data.rotation = &rot;
        } else {
            // move to left
            distance = frontWindow->geometry().width() * 0.5f - area.width() * 0.5f +
                       ((float)width * 0.5f - ((float)displayWidth() * 0.5 * scaleFactor)) / leftWindows;
            float factor = 1.0;
            if (specialHandlingForward)
                factor = 2.0;
            data.xTranslate += distance * timeLine.currentValue() * factor;
            RotationData rot;
            rot.axis = RotationData::YAxis;
            rot.angle = angle * timeLine.currentValue();
            data.rotation = &rot;
        }
    }
    if (specialHandlingForward) {
        data.opacity *= (1.0 - timeLine.currentValue() * 2.0);
        paintWindowCover(frontWindow, reflectedWindow, data);
    } else
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

    float xTranslate = -((float)(width) * 0.5f - ((float)displayWidth() * 0.5 * scaleFactor));
    if (!left)
        xTranslate = ((float)displayWidth() * 0.5 * scaleFactor) - (float)width * 0.5f;
    // handling for additional window from other side
    // has to appear on this side after half of the time
    if (animation && timeLine.currentValue() >= 0.5 && additionalWindow != NULL) {
        RotationData rot;
        rot.axis = RotationData::YAxis;
        rot.angle = angle;
        rot.angle = angle * rotateFactor;
        WindowPaintData data(additionalWindow);
        if (left)
            data.xTranslate += -xTranslate - additionalWindow->geometry().x();
        else {
            data.xTranslate += xTranslate + area.width() -
                               additionalWindow->geometry().x() - additionalWindow->geometry().width();
            rot.xRotationPoint = additionalWindow->geometry().width();
        }
        data.rotation = &rot;
        data.opacity *= (timeLine.currentValue() - 0.5) * 2.0;
        paintWindowCover(additionalWindow, reflectedWindows, data);
    }
    RotationData rot;
    rot.axis = RotationData::YAxis;
    // normal behaviour
    for (int i = 0; i < windows.count(); i++) {
        window = windows.at(i);
        if (window == NULL || window->isDeleted()) {
            continue;
        }
        WindowPaintData data(window);
        rot.angle = angle;
        if (left)
            data.xTranslate += -xTranslate + xTranslate * i / windowCount - window->geometry().x();
        else
            data.xTranslate += xTranslate + width - xTranslate * i / windowCount - window->geometry().x() - window->geometry().width();
        if (animation) {
            if (direction == Right) {
                if ((i == windowCount - 1) && left) {
                    // right most window on left side -> move to front
                    // have to move one window distance plus half the difference between the window and the desktop size
                    data.xTranslate += (xTranslate / windowCount + (width - window->geometry().width()) * 0.5f) * timeLine.currentValue();
                    rot.angle = (angle - angle * timeLine.currentValue());
                }
                // right most window does not have to be moved
                else if (!left && (i == 0));     // do nothing
                else {
                    // all other windows - move to next position
                    data.xTranslate += xTranslate / windowCount * timeLine.currentValue();
                }
            } else {
                if ((i == windowCount - 1) && !left) {
                    // left most window on right side -> move to front
                    data.xTranslate -= (xTranslate / windowCount + (width - window->geometry().width()) * 0.5f) * timeLine.currentValue();
                    rot.angle = (angle - angle * timeLine.currentValue());
                }
                // left most window does not have to be moved
                else if (i == 0 && left); // do nothing
                else {
                    // all other windows - move to next position
                    data.xTranslate -= xTranslate / windowCount * timeLine.currentValue();
                }
            }
        }
        if (left)
            rot.xRotationPoint = 0.0;
        else
            rot.xRotationPoint = window->geometry().width();
        rot.angle *= rotateFactor;
        data.rotation = &rot;
        // make window most to edge transparent if animation
        if (animation && i == 0 && ((direction == Left && left) || (direction == Right && !left))) {
            // only for the first half of the animation
            if (timeLine.currentValue() < 0.5) {
                data.opacity *= (1.0 - timeLine.currentValue() * 2.0);
                paintWindowCover(window, reflectedWindows, data);
            }
        } else {
            paintWindowCover(window, reflectedWindows, data);
        }
    }
}

void CoverSwitchEffect::windowInputMouseEvent(Window w, QEvent* e)
{
    assert(w == input);
    Q_UNUSED(w);
    if (e->type() != QEvent::MouseButtonPress)
        return;
    // we don't want click events during animations
    if (animation)
        return;
    QPoint pos = static_cast< QMouseEvent* >(e)->pos();

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
}

void CoverSwitchEffect::abort()
{
    // it's possible that abort is called after tabbox has been closed
    // in this case the cleanup is already done (see bug 207554)
    if (mActivated) {
        effects->unrefTabBox();
        effects->destroyInputWindow(input);
    }
    effects->setActiveFullScreenEffect(0);
    mActivated = false;
    stop = false;
    stopRequested = false;
    effects->addRepaintFull();
    captionFrame->free();
}

void CoverSwitchEffect::slotWindowClosed(EffectWindow* c)
{
    if (c == selected_window)
        selected_window = 0;
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
    return mActivated || stop || stopRequested;
}

void CoverSwitchEffect::updateCaption()
{
    if (!selected_window || !windowTitle) {
        return;
    }
    if (selected_window->isDesktop()) {
        captionFrame->setText(i18nc("Special entry in alt+tab list for minimizing all windows",
                     "Show Desktop"));
        static QPixmap pix = KIcon("user-desktop").pixmap(captionFrame->iconSize());
        captionFrame->setIcon(pix);
    } else {
        captionFrame->setText(selected_window->caption());
        captionFrame->setIcon(selected_window->icon());
    }
}

void CoverSwitchEffect::slotTabBoxKeyEvent(QKeyEvent *event)
{
    if (!mActivated || !selected_window) {
        return;
    }
    const int index = effects->currentTabBoxWindowList().indexOf(selected_window);
    int newIndex = index;
    if (event->type() == QEvent::KeyPress) {
        switch (event->key()) {
        case Qt::Key_Left:
            newIndex = (index - 1);
            break;
        case Qt::Key_Right:
            newIndex = (index + 1);
            break;
        default:
            // nothing
            break;
        }
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
