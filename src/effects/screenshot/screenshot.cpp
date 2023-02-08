/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2010 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2010 Nokia Corporation and /or its subsidiary(-ies)
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "screenshot.h"
#include "screenshotdbusinterface1.h"
#include "screenshotdbusinterface2.h"

#include <input.h>
#include <kwinglplatform.h>
#include <kwinglutils.h>
#include <qevent.h>
#include <qlist.h>
#include <workspace.h>

#include <QKeyEvent>
#include <QKeySequence>
#include <QPainter>
#include <QQuickItem>
#include <QQuickWindow>
#include <qnamespace.h>
#include <utility>

namespace KWin
{

struct ScreenShotWindowData
{
    QFutureInterface<QImage> promise;
    ScreenShotFlags flags;
    EffectWindow *window = nullptr;
};

struct ScreenShotAreaData
{
    QFutureInterface<QImage> promise;
    ScreenShotFlags flags;
    QRect area;
    QImage result;
    QList<EffectScreen *> screens;
};

struct ScreenShotScreenData
{
    QFutureInterface<QImage> promise;
    ScreenShotFlags flags;
    EffectScreen *screen = nullptr;
};

static void convertFromGLImage(QImage &img, int w, int h)
{
    // from QtOpenGL/qgl.cpp
    // SPDX-FileCopyrightText: 2010 Nokia Corporation and /or its subsidiary(-ies)
    // see https://github.com/qt/qtbase/blob/dev/src/opengl/qgl.cpp
    if (QSysInfo::ByteOrder == QSysInfo::BigEndian) {
        // OpenGL gives RGBA; Qt wants ARGB
        uint *p = reinterpret_cast<uint *>(img.bits());
        uint *end = p + w * h;
        while (p < end) {
            uint a = *p << 24;
            *p = (*p >> 8) | a;
            p++;
        }
    } else {
        // OpenGL gives ABGR (i.e. RGBA backwards); Qt wants ARGB
        for (int y = 0; y < h; y++) {
            uint *q = reinterpret_cast<uint *>(img.scanLine(y));
            for (int x = 0; x < w; ++x) {
                const uint pixel = *q;
                *q = ((pixel << 16) & 0xff0000) | ((pixel >> 16) & 0xff)
                    | (pixel & 0xff00ff00);

                q++;
            }
        }
    }
    img = img.mirrored();
}

bool ScreenShotEffect::supported()
{
    return effects->isOpenGLCompositing() && GLFramebuffer::supported();
}

ScreenShotEffect::ScreenShotEffect()
    : m_dbusInterface1(new ScreenShotDBusInterface1(this))
    , m_dbusInterface2(new ScreenShotDBusInterface2(this))
{
    connect(effects, &EffectsHandler::screenAdded, this, &ScreenShotEffect::handleScreenAdded);
    connect(effects, &EffectsHandler::screenRemoved, this, &ScreenShotEffect::handleScreenRemoved);
    connect(effects, &EffectsHandler::windowClosed, this, &ScreenShotEffect::handleWindowClosed);
    connect(effects, &EffectsHandler::screenAboutToLock, this, &ScreenShotEffect::cancel);
}

ScreenShotEffect::~ScreenShotEffect()
{
    cancelWindowScreenShots();
    cancelAreaScreenShots();
    cancelScreenScreenShots();
}

void ScreenShotEffect::startWindowPicker(std::function<void(EffectWindow *)> callback)
{
    if (!m_windowScreenShots.isEmpty()) {
        return;
    }
    cancel();
    m_pickerMode = WindowPicker;
    m_windowsCallback = callback;
    setSource(QUrl::fromLocalFile(QStandardPaths::locate(QStandardPaths::GenericDataLocation, QStringLiteral("kwin/effects/screenshot/qml/WindowPicker.qml"))));
    activatePicker();
}

void ScreenShotEffect::startScreenPicker(std::function<void(EffectScreen *)> callback)
{
    if (!m_screenScreenShots.isEmpty()) {
        return;
    }
    cancel();
    m_pickerMode = ScreenPicker;
    m_screensCallback = callback;
    setSource(QUrl::fromLocalFile(QStandardPaths::locate(QStandardPaths::GenericDataLocation, QStringLiteral("kwin/effects/screenshot/qml/ScreenPicker.qml"))));
    activatePicker();
}

void ScreenShotEffect::accept()
{
    setRunning(false);
    if (m_windowsCallback) {
        QList<EffectWindow *> list;
        for (auto window : std::as_const(m_selectedWindows)) {
            list.append(window);
        }
        m_windowsCallback(list.first());
    } else if (m_screensCallback) {
        m_screensCallback(m_selectedScreens.first());
    }
    clearPicker();
}

void ScreenShotEffect::cancel()
{
    setRunning(false);
    if (m_windowsCallback) {
        m_windowsCallback(nullptr);
    } else if (m_screensCallback) {
        m_screensCallback(nullptr);
    }
    clearPicker();
}

QVariantMap ScreenShotEffect::initialProperties(EffectScreen *screen)
{
    if (m_pickerMode == WindowPicker) {
        QList<QRectF> windowRects;
        for (const auto *window : std::as_const(Workspace::self()->stackingOrder())) {
            auto windowRect = window->frameGeometry(); // excludes shadows
            if (!window->isHiddenInternal() && !window->isMinimized()
                && screen->geometry().intersects(windowRect.toRect())
            ) {
                windowRect.moveTopLeft(screen->mapFromGlobal(windowRect.topLeft()));
                windowRects.append(windowRect);
            }
        }
        return QVariantMap {
            {QStringLiteral("effect"), QVariant::fromValue(this)},
            {QStringLiteral("targetScreen"), QVariant::fromValue(screen)},
            {QStringLiteral("windowRects"), QVariant::fromValue(windowRects)},
        };
    } else if (m_pickerMode == ScreenPicker) {
        return QVariantMap {
            {QStringLiteral("effect"), QVariant::fromValue(this)},
        };
    }
    return QVariantMap();
}

void ScreenShotEffect::activatePicker()
{
    if (effects->isScreenLocked()) {
        return;
    }

    setRunning(true);
}

void ScreenShotEffect::clearPicker()
{
    m_selectedWindows.clear();
    m_selectedScreens.clear();
    m_windowsCallback = nullptr;
    m_screensCallback = nullptr;
    m_acceptReleaseKeyEvent = 0;
    m_pickerMode = NoPicker;
    setSource({});
}

EffectScreen *ScreenShotEffect::nextScreenInFocusChain(bool forward)
{
    EffectScreen *nextScreen = nullptr;
    const auto &screens = effects->screens();
    const int totalScreens = screens.size();
    if (totalScreens > 1) {
        auto *const activeScreen = effects->activeScreen();
        if (forward) {
            for (int i = 0; i < totalScreens; ++i) {
                int next = i + 1;
                if (activeScreen == screens[i] && next < totalScreens) {
                    // nextScreen is next screen if possible
                    nextScreen = screens[next];
                    break;
                } else if (activeScreen != screens[0] && next >= totalScreens) {
                    // otherwise, wrap back to the first screen
                    nextScreen = screens[0];
                    break;
                }
            }
        } else {
            const int last = totalScreens - 1;
            for (int i = last; i >= 0; --i) {
                int previous = i - 1;
                if (activeScreen == screens[i] && previous >= 0) {
                    // nextScreen is previous screen if possible
                    nextScreen = screens[previous];
                    break;
                } else if (activeScreen != screens[last] && previous < 0) {
                    // otherwise, wrap back to the last screen
                    nextScreen = screens[last];
                    break;
                }
            }
        }
    }
    return nextScreen;
}

void ScreenShotEffect::windowInputMouseEvent(QEvent *event)
{
    if (event->type() == QEvent::MouseButtonRelease) {
        auto mouseEvent = static_cast<QMouseEvent *>(event);
        const auto &buttons = mouseEvent->buttons();
        if (buttons & Qt::LeftButton) {
            // screenPos() is globalPos(), but QPointF instead of QPoint.
            const auto &screenPos = mouseEvent->screenPos();
            if (m_pickerMode == WindowPicker) {
                auto window = InputRedirection::self()->findToplevel(screenPos);
                auto effectWindow = window ? window->effectWindow() : nullptr;
                if (effectWindow) {
                    if (m_selectedWindows.contains(effectWindow)) {
                        m_selectedWindows.removeOne(effectWindow);
                    } else {
                        m_selectedWindows.append(effectWindow);
                    }
                }
            } else if (m_pickerMode == ScreenPicker) {
                // screenAt() only accepts QPoint.
                auto screen = effects->screenAt(screenPos.toPoint());
                if (screen) {
                    if (m_selectedScreens.contains(screen)) {
                        m_selectedScreens.removeOne(screen);
                    } else {
                        m_selectedScreens.append(screen);
                    }
                }
            }
        } else if (buttons & Qt::RightButton) {
            event->accept();
            cancel();
        }
    }

    if (!event->isAccepted()) {
        QuickSceneEffect::windowInputMouseEvent(event);
    }
}

void ScreenShotEffect::grabbedKeyboardEvent(QKeyEvent *keyEvent)
{
    const uint keys = keyEvent->key() | keyEvent->modifiers();
    const auto &type = keyEvent->type();

    // accept or cancel
    if (keyEvent) {
        keyEvent->accept();
        accept();
        return;
    } else if (keyEvent->matches(QKeySequence::Cancel)
        || keyEvent->matches(QKeySequence::Close)
        || keyEvent->matches(QKeySequence::Quit)) {
        keyEvent->accept();
        cancel();
        return;
    }

    const bool tab = keys == Qt::Key_Tab;
    const bool backtab = keys == Qt::Key_Backtab
                      || keys == (uint(Qt::Key_Tab) | uint(Qt::ShiftModifier));
    auto *const activeView = this->activeView();

    if (!activeView) {
        return;
    }

    if (m_pickerMode == WindowPicker) {
        auto focusedItem = activeView->window()->activeFocusItem();
        // tab to next view/backtab to previous view
        if ((tab || backtab) && type == QEvent::KeyPress) {
            auto nextItem = focusedItem ? focusedItem->nextItemInFocusChain(tab) : nullptr;
            EffectScreen *nextScreen = nullptr;
            if (nextItem && nextItem == activeView->rootItem()->nextItemInFocusChain(!tab)) {
                nextScreen = nextScreenInFocusChain(tab);
            }
            if (nextScreen) {
                keyEvent->accept();
                m_acceptReleaseKeyEvent = keys;
                activateView(viewForScreen(nextScreen));
            }
        } else if (focusedItem && keys == Qt::Key_Space && type == QEvent::KeyRelease) {
            QPointF itemCenter = focusedItem->position()
                               + QPointF(focusedItem->width(), focusedItem->height()) / 2;
            itemCenter = focusedItem->mapToGlobal(itemCenter);
            auto window = InputRedirection::self()->findToplevel(itemCenter);
            auto effectWindow = window ? window->effectWindow() : nullptr;
            if (effectWindow) {
                if (m_selectedWindows.contains(effectWindow)) {
                    m_selectedWindows.removeOne(effectWindow);
                } else {
                    m_selectedWindows.append(effectWindow);
                }
            }
        }
    } else if (m_pickerMode == ScreenPicker) {
        // tab to next view/backtab to previous view
        if ((tab || backtab) && type == QEvent::KeyPress) {
            auto nextScreen = nextScreenInFocusChain(tab);
            if (nextScreen) {
                keyEvent->accept();
                m_acceptReleaseKeyEvent = keys;
                activateView(viewForScreen(nextScreen));
            }
        } else if (keys == Qt::Key_Space && type == QEvent::KeyRelease) {
            auto screen = activeView->screen();
            if (screen) {
                if (m_selectedScreens.contains(screen)) {
                    m_selectedScreens.removeOne(screen);
                } else {
                    m_selectedScreens.append(screen);
                }
            }
        }
    }

    // It is valid to a bitwise OR with a key and modifiers
    if (m_acceptReleaseKeyEvent == keys && type == QEvent::KeyRelease) {
        keyEvent->accept();
        m_acceptReleaseKeyEvent = 0;
    }

    if (!keyEvent->isAccepted()) {
        QuickSceneEffect::grabbedKeyboardEvent(keyEvent);
    }
}

// bool ScreenShotEffect::touchDown(qint32 id, const QPointF &pos, std::chrono::microseconds time)
// {
//     return QuickSceneEffect::touchDown(id, pos, time);
// }
// 
// bool ScreenShotEffect::touchMotion(qint32 id, const QPointF &pos, std::chrono::microseconds time)
// {
//     return QuickSceneEffect::touchMotion(id, pos, time);
// }
// 
// bool ScreenShotEffect::touchUp(qint32 id, std::chrono::microseconds time)
// {
//     return QuickSceneEffect::touchUp(id, time);
// }

QFuture<QImage> ScreenShotEffect::scheduleScreenShot(EffectScreen *screen, ScreenShotFlags flags)
{
    for (ScreenShotScreenData &data : m_screenScreenShots) {
        if (data.screen == screen && data.flags == flags) {
            return data.promise.future();
        }
    }

    ScreenShotScreenData data;
    data.screen = screen;
    data.flags = flags;

    m_screenScreenShots.append(data);
    effects->addRepaint(screen->geometry());

    data.promise.reportStarted();
    return data.promise.future();
}

QFuture<QImage> ScreenShotEffect::scheduleScreenShot(const QRect &area, ScreenShotFlags flags)
{
    for (ScreenShotAreaData &data : m_areaScreenShots) {
        if (data.area == area && data.flags == flags) {
            return data.promise.future();
        }
    }

    ScreenShotAreaData data;
    data.area = area;
    data.flags = flags;

    const QList<EffectScreen *> screens = effects->screens();
    for (EffectScreen *screen : screens) {
        if (screen->geometry().intersects(area)) {
            data.screens.append(screen);
        }
    }

    qreal devicePixelRatio = 1.0;
    if (flags & ScreenShotNativeResolution) {
        for (const EffectScreen *screen : std::as_const(data.screens)) {
            if (screen->devicePixelRatio() > devicePixelRatio) {
                devicePixelRatio = screen->devicePixelRatio();
            }
        }
    }

    data.result = QImage(area.size() * devicePixelRatio, QImage::Format_ARGB32_Premultiplied);
    data.result.fill(Qt::transparent);
    data.result.setDevicePixelRatio(devicePixelRatio);

    m_areaScreenShots.append(data);
    effects->addRepaint(area);

    data.promise.reportStarted();
    return data.promise.future();
}

QFuture<QImage> ScreenShotEffect::scheduleScreenShot(EffectWindow *window, ScreenShotFlags flags)
{
    for (ScreenShotWindowData &data : m_windowScreenShots) {
        if (data.window == window && data.flags == flags) {
            return data.promise.future();
        }
    }

    ScreenShotWindowData data;
    data.window = window;
    data.flags = flags;

    m_windowScreenShots.append(data);
    window->addRepaintFull();

    data.promise.reportStarted();
    return data.promise.future();
}

void ScreenShotEffect::cancelWindowScreenShots()
{
    while (!m_windowScreenShots.isEmpty()) {
        ScreenShotWindowData screenshot = m_windowScreenShots.takeLast();
        screenshot.promise.reportCanceled();
    }
}

void ScreenShotEffect::cancelAreaScreenShots()
{
    while (!m_areaScreenShots.isEmpty()) {
        ScreenShotAreaData screenshot = m_areaScreenShots.takeLast();
        screenshot.promise.reportCanceled();
    }
}

void ScreenShotEffect::cancelScreenScreenShots()
{
    while (!m_screenScreenShots.isEmpty()) {
        ScreenShotScreenData screenshot = m_screenScreenShots.takeLast();
        screenshot.promise.reportCanceled();
    }
}

void ScreenShotEffect::paintScreen(int mask, const QRegion &region, ScreenPaintData &data)
{
    m_paintedScreen = data.screen();
    effects->paintScreen(mask, region, data);

    while (!m_windowScreenShots.isEmpty()) {
        ScreenShotWindowData screenshot = m_windowScreenShots.takeLast();
        takeScreenShot(&screenshot);
    }

    for (int i = m_areaScreenShots.count() - 1; i >= 0; --i) {
        if (takeScreenShot(&m_areaScreenShots[i])) {
            m_areaScreenShots.removeAt(i);
        }
    }

    for (int i = m_screenScreenShots.count() - 1; i >= 0; --i) {
        if (takeScreenShot(&m_screenScreenShots[i])) {
            m_screenScreenShots.removeAt(i);
        }
    }
}

void ScreenShotEffect::takeScreenShot(ScreenShotWindowData *screenshot)
{
    EffectWindow *window = screenshot->window;

    WindowPaintData d;
    QRectF geometry = window->expandedGeometry();
    qreal devicePixelRatio = 1;
    if (window->hasDecoration() && !(screenshot->flags & ScreenShotIncludeDecoration)) {
        geometry = window->clientGeometry();
    }
    if (screenshot->flags & ScreenShotNativeResolution) {
        if (const EffectScreen *screen = window->screen()) {
            devicePixelRatio = screen->devicePixelRatio();
        }
    }
    bool validTarget = true;
    std::unique_ptr<GLTexture> offscreenTexture;
    std::unique_ptr<GLFramebuffer> target;
    if (effects->isOpenGLCompositing()) {
        offscreenTexture.reset(new GLTexture(GL_RGBA8, QSizeF(geometry.size() * devicePixelRatio).toSize()));
        offscreenTexture->setFilter(GL_LINEAR);
        offscreenTexture->setWrapMode(GL_CLAMP_TO_EDGE);
        target.reset(new GLFramebuffer(offscreenTexture.get()));
        validTarget = target->valid();
    }
    if (validTarget) {
        d.setXTranslation(-geometry.x());
        d.setYTranslation(-geometry.y());
        d.setRenderTargetScale(devicePixelRatio);

        // render window into offscreen texture
        int mask = PAINT_WINDOW_TRANSFORMED | PAINT_WINDOW_TRANSLUCENT;
        QImage img;
        if (effects->isOpenGLCompositing()) {
            GLFramebuffer::pushFramebuffer(target.get());
            glClearColor(0.0, 0.0, 0.0, 0.0);
            glClear(GL_COLOR_BUFFER_BIT);
            glClearColor(0.0, 0.0, 0.0, 1.0);

            QMatrix4x4 projection;
            projection.ortho(QRect(0, 0, geometry.width() * devicePixelRatio, geometry.height() * devicePixelRatio));
            d.setProjectionMatrix(projection);

            effects->drawWindow(window, mask, infiniteRegion(), d);

            // copy content from framebuffer into image
            img = QImage(offscreenTexture->size(), QImage::Format_ARGB32);
            img.setDevicePixelRatio(devicePixelRatio);
            glReadnPixels(0, 0, img.width(), img.height(), GL_RGBA, GL_UNSIGNED_BYTE, img.sizeInBytes(),
                          static_cast<GLvoid *>(img.bits()));
            GLFramebuffer::popFramebuffer();
            convertFromGLImage(img, img.width(), img.height());
        }

        if (screenshot->flags & ScreenShotIncludeCursor) {
            grabPointerImage(img, geometry.x(), geometry.y());
        }

        screenshot->promise.reportResult(img);
        screenshot->promise.reportFinished();
    } else {
        screenshot->promise.reportCanceled();
    }
}

bool ScreenShotEffect::takeScreenShot(ScreenShotAreaData *screenshot)
{
    if (!effects->waylandDisplay()) {
        // On X11, all screens are painted simultaneously and there is no native HiDPI support.
        QImage snapshot = blitScreenshot(screenshot->area);
        if (screenshot->flags & ScreenShotIncludeCursor) {
            grabPointerImage(snapshot, screenshot->area.x(), screenshot->area.y());
        }
        screenshot->promise.reportResult(snapshot);
        screenshot->promise.reportFinished();
    } else {
        if (!screenshot->screens.contains(m_paintedScreen)) {
            return false;
        }
        screenshot->screens.removeOne(m_paintedScreen);

        const QRect sourceRect = screenshot->area & m_paintedScreen->geometry();
        qreal sourceDevicePixelRatio = 1.0;
        if (screenshot->flags & ScreenShotNativeResolution) {
            sourceDevicePixelRatio = m_paintedScreen->devicePixelRatio();
        }

        const QImage snapshot = blitScreenshot(sourceRect, sourceDevicePixelRatio);
        const QRect nativeArea(screenshot->area.topLeft(),
                               screenshot->area.size() * screenshot->result.devicePixelRatio());

        QPainter painter(&screenshot->result);
        painter.setWindow(nativeArea);
        painter.drawImage(sourceRect, snapshot);
        painter.end();

        if (screenshot->screens.isEmpty()) {
            if (screenshot->flags & ScreenShotIncludeCursor) {
                grabPointerImage(screenshot->result, screenshot->area.x(), screenshot->area.y());
            }
            screenshot->promise.reportResult(screenshot->result);
            screenshot->promise.reportFinished();
        }
    }

    return screenshot->promise.isFinished();
}

bool ScreenShotEffect::takeScreenShot(ScreenShotScreenData *screenshot)
{
    if (!m_paintedScreen || screenshot->screen == m_paintedScreen) {
        qreal devicePixelRatio = 1.0;
        if (screenshot->flags & ScreenShotNativeResolution) {
            devicePixelRatio = screenshot->screen->devicePixelRatio();
        }

        QImage snapshot = blitScreenshot(screenshot->screen->geometry(), devicePixelRatio);
        if (screenshot->flags & ScreenShotIncludeCursor) {
            const int xOffset = screenshot->screen->geometry().x();
            const int yOffset = screenshot->screen->geometry().y();
            grabPointerImage(snapshot, xOffset, yOffset);
        }

        screenshot->promise.reportResult(snapshot);
        screenshot->promise.reportFinished();
    }

    return screenshot->promise.isFinished();
}

QImage ScreenShotEffect::blitScreenshot(const QRect &geometry, qreal devicePixelRatio) const
{
    QImage image;

    if (effects->isOpenGLCompositing()) {
        const QSize nativeSize = geometry.size() * devicePixelRatio;

        if (GLFramebuffer::blitSupported() && !GLPlatform::instance()->isGLES()) {
            image = QImage(nativeSize.width(), nativeSize.height(), QImage::Format_ARGB32);
            GLTexture texture(GL_RGBA8, nativeSize.width(), nativeSize.height());
            GLFramebuffer target(&texture);
            target.blitFromFramebuffer(effects->mapToRenderTarget(QRectF(geometry)).toRect());
            // copy content from framebuffer into image
            texture.bind();
            glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                          static_cast<GLvoid *>(image.bits()));
            texture.unbind();
        } else {
            image = QImage(nativeSize.width(), nativeSize.height(), QImage::Format_ARGB32);
            glReadPixels(0, 0, nativeSize.width(), nativeSize.height(), GL_RGBA,
                         GL_UNSIGNED_BYTE, static_cast<GLvoid *>(image.bits()));
        }
        convertFromGLImage(image, nativeSize.width(), nativeSize.height());
    }

    image.setDevicePixelRatio(devicePixelRatio);
    return image;
}

void ScreenShotEffect::grabPointerImage(QImage &snapshot, int xOffset, int yOffset) const
{
    if (effects->isCursorHidden()) {
        return;
    }

    const PlatformCursorImage cursor = effects->cursorImage();
    if (cursor.image().isNull()) {
        return;
    }

    QPainter painter(&snapshot);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    painter.drawImage(effects->cursorPos() - cursor.hotSpot() - QPoint(xOffset, yOffset), cursor.image());
}

bool ScreenShotEffect::isActive() const
{
    const bool processing = !m_windowScreenShots.isEmpty()
                         || !m_areaScreenShots.isEmpty()
                         || !m_screenScreenShots.isEmpty();
    return (processing || QuickSceneEffect::isActive()) && !effects->isScreenLocked();
}

int ScreenShotEffect::requestedEffectChainPosition() const
{
    return 0;
}

void ScreenShotEffect::handleScreenAdded()
{
    cancelAreaScreenShots();
}

void ScreenShotEffect::handleScreenRemoved(EffectScreen *screen)
{
    cancelAreaScreenShots();

    for (int i = m_screenScreenShots.count() - 1; i >= 0; --i) {
        if (m_screenScreenShots[i].screen == screen) {
            m_screenScreenShots[i].promise.reportCanceled();
            m_screenScreenShots.removeAt(i);
        }
    }
}

void ScreenShotEffect::handleWindowClosed(EffectWindow *window)
{
    for (int i = m_windowScreenShots.count() - 1; i >= 0; --i) {
        if (m_windowScreenShots[i].window == window) {
            m_windowScreenShots[i].promise.reportCanceled();
            m_windowScreenShots.removeAt(i);
        }
    }
}

} // namespace KWin
