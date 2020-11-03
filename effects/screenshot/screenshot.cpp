/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2010 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2010 Nokia Corporation and /or its subsidiary(-ies)

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "screenshot.h"
#include <kwinglplatform.h>
#include <kwinglutils.h>
#include <kwinxrenderutils.h>
#include <QtConcurrentRun>
#include <QDataStream>
#include <QTemporaryFile>
#include <QDir>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusReply>
#include <QVarLengthArray>
#include <QPainter>
#include <QMatrix4x4>
#include <xcb/xcb_image.h>
#include <QPoint>

#include <KLocalizedString>
#include <KNotification>

#include <unistd.h>
#include "../service_utils.h"

class ComparableQPoint : public QPoint
{
public:
    ComparableQPoint(QPoint& point): QPoint(point.x(), point.y())
    {}

    ComparableQPoint(QPoint point): QPoint(point.x(), point.y())
    {}

    ComparableQPoint(): QPoint()
    {}

    // utility class that allows using QMap to sort its keys when they are QPoint
    // so that the bottom and right points are after the top left ones
    bool operator<(const ComparableQPoint &other) const {
        return x() < other.x() || y() < other.y();
    }
};


namespace KWin
{

const static QString s_errorAlreadyTaking = QStringLiteral("org.kde.kwin.Screenshot.Error.AlreadyTaking");
const static QString s_errorAlreadyTakingMsg = QStringLiteral("A screenshot is already been taken");
const static QString s_errorNotAuthorized = QStringLiteral("org.kde.kwin.Screenshot.Error.NoAuthorized");
const static QString s_errorNotAuthorizedMsg = QStringLiteral("The process is not authorized to take a screenshot");
const static QString s_errorFd = QStringLiteral("org.kde.kwin.Screenshot.Error.FileDescriptor");
const static QString s_errorFdMsg = QStringLiteral("No valid file descriptor");
const static QString s_errorCancelled = QStringLiteral("org.kde.kwin.Screenshot.Error.Cancelled");
const static QString s_errorCancelledMsg = QStringLiteral("Screenshot got cancelled");
const static QString s_errorInvalidArea = QStringLiteral("org.kde.kwin.Screenshot.Error.InvalidArea");
const static QString s_errorInvalidAreaMsg = QStringLiteral("Invalid area requested");
const static QString s_errorInvalidScreen = QStringLiteral("org.kde.kwin.Screenshot.Error.InvalidScreen");
const static QString s_errorInvalidScreenMsg = QStringLiteral("Invalid screen requested");
const static QString s_dbusInterfaceName = QStringLiteral("org.kde.kwin.Screenshot");

bool ScreenShotEffect::supported()
{
    return  effects->compositingType() == XRenderCompositing ||
            (effects->isOpenGLCompositing() && GLRenderTarget::supported());
}

ScreenShotEffect::ScreenShotEffect()
    : m_scheduledScreenshot(nullptr)
{
    connect(effects, &EffectsHandler::windowClosed, this, &ScreenShotEffect::windowClosed);
    QDBusConnection::sessionBus().registerObject(QStringLiteral("/Screenshot"), this, QDBusConnection::ExportScriptableContents);
}

ScreenShotEffect::~ScreenShotEffect()
{
    QDBusConnection::sessionBus().unregisterObject(QStringLiteral("/Screenshot"));
}

#ifdef KWIN_HAVE_XRENDER_COMPOSITING
static QImage xPictureToImage(xcb_render_picture_t srcPic, const QRect &geometry, xcb_image_t **xImage)
{
    xcb_connection_t *c = effects->xcbConnection();
    xcb_pixmap_t xpix = xcb_generate_id(c);
    xcb_create_pixmap(c, 32, xpix, effects->x11RootWindow(), geometry.width(), geometry.height());
    XRenderPicture pic(xpix, 32);
    xcb_render_composite(c, XCB_RENDER_PICT_OP_SRC, srcPic, XCB_RENDER_PICTURE_NONE, pic,
                         geometry.x(), geometry.y(), 0, 0, 0, 0, geometry.width(), geometry.height());
    xcb_flush(c);
    *xImage = xcb_image_get(c, xpix, 0, 0, geometry.width(), geometry.height(), ~0, XCB_IMAGE_FORMAT_Z_PIXMAP);
    QImage img((*xImage)->data, (*xImage)->width, (*xImage)->height, (*xImage)->stride, QImage::Format_ARGB32_Premultiplied);
    // TODO: byte order might need swapping
    xcb_free_pixmap(c, xpix);
    return img.copy();
}
#endif

static QSize pickWindowSize(const QImage &image)
{
    xcb_connection_t *c = effects->xcbConnection();

    // This will implicitly enable BIG-REQUESTS extension.
    const uint32_t maximumRequestSize = xcb_get_maximum_request_length(c);
    const xcb_setup_t *setup = xcb_get_setup(c);

    uint32_t requestSize = sizeof(xcb_put_image_request_t);

    // With BIG-REQUESTS extension an additional 32-bit field is inserted into
    // the request so we better take it into account.
    if (setup->maximum_request_length < maximumRequestSize) {
        requestSize += 4;
    }

    const uint32_t maximumDataSize = 4 * maximumRequestSize - requestSize;
    const uint32_t bytesPerPixel = image.depth() >> 3;
    const uint32_t bytesPerLine = image.bytesPerLine();

    if (image.sizeInBytes() <= maximumDataSize) {
        return image.size();
    }

    if (maximumDataSize < bytesPerLine) {
        return QSize(maximumDataSize / bytesPerPixel, 1);
    }

    return QSize(image.width(), maximumDataSize / bytesPerLine);
}

static xcb_pixmap_t xpixmapFromImage(const QImage &image)
{
    xcb_connection_t *c = effects->xcbConnection();

    xcb_pixmap_t pixmap = xcb_generate_id(c);
    xcb_gcontext_t gc = xcb_generate_id(c);

    xcb_create_pixmap(c, image.depth(), pixmap, effects->x11RootWindow(),
        image.width(), image.height());
    xcb_create_gc(c, gc, pixmap, 0, nullptr);

    const int bytesPerPixel = image.depth() >> 3;

    // Figure out how much data we can send with one invocation of xcb_put_image.
    // In contrast to XPutImage, xcb_put_image doesn't implicitly split the data.
    const QSize window = pickWindowSize(image);

    for (int i = 0; i < image.height(); i += window.height()) {
        const int targetHeight = qMin(image.height() - i, window.height());
        const uint8_t *line = image.scanLine(i);

        for (int j = 0; j < image.width(); j += window.width()) {
            const int targetWidth = qMin(image.width() - j, window.width());
            const uint8_t *bytes = line + j * bytesPerPixel;
            const uint32_t byteCount = targetWidth * targetHeight * bytesPerPixel;

            xcb_put_image(c, XCB_IMAGE_FORMAT_Z_PIXMAP, pixmap,
                gc, targetWidth, targetHeight, j, i, 0, image.depth(),
                byteCount, bytes);
        }
    }

    xcb_flush(c);
    xcb_free_gc(c, gc);

    return pixmap;
}

void ScreenShotEffect::paintScreen(int mask, const QRegion &region, ScreenPaintData &data)
{
    m_cachedOutputGeometry = data.outputGeometry();
    // When taking a non-nativeSize fullscreenshot, pretend we have a uniform 1.0 ratio
    // so the screenshot size will match the virtualGeometry
    m_cachedScale = m_nativeSize ? data.screenScale() : 1.0;
    effects->paintScreen(mask, region, data);
}

/**
 * Translates the Point coordinates in m_cacheOutputsImages keys from the virtualGeometry
 * To a new geometry taking into account the real size of the images (virtualSize * dpr),
 * moving the siblings images on the right bottom of the image as necessary
 */
void ScreenShotEffect::computeCoordinatesAfterScaling()
{
    // prepare a translation table that will store oldPoint -> newPoint in the new coordinates
    QMap<ComparableQPoint, ComparableQPoint> translationMap;
    for (auto i = m_cacheOutputsImages.keyBegin(); i != m_cacheOutputsImages.keyEnd(); ++i) {
        translationMap.insert(*i, *i);
    }

    for (auto i = m_cacheOutputsImages.constBegin(); i != m_cacheOutputsImages.constEnd(); ++i) {
        const auto p = i.key();
        const auto img = i.value();
        const auto dpr = img.devicePixelRatio();
        if (!qFuzzyCompare(dpr, 1.0)) {
            // must update all coordinates of next rects
            const int deltaX = img.width() - (img.width() / dpr);
            const int deltaY = img.height() - (img.height() / dpr);

            // for the next images on the right or bottom
            // thanks to ComparableQPoint
            for (auto i2 = m_cacheOutputsImages.constFind(p);
                 i2 != m_cacheOutputsImages.constEnd(); ++i2) {

                const auto point = i2.key();
                auto finalPoint = translationMap.value(point);

                if (point.x() >= img.width() + p.x() - deltaX) {
                    finalPoint.setX(finalPoint.x() + deltaX);
                }
                if (point.y() >= img.height() + p.y() - deltaY) {
                    finalPoint.setY(finalPoint.y() + deltaY);
                }
                // update final position point with the necessary deltas
                translationMap.insert(point, finalPoint);
            }
        }
    }

    // make the new coordinates effective
    for (auto i = translationMap.keyBegin(); i != translationMap.keyEnd(); ++i) {
        const auto key = *i;
        const auto img = m_cacheOutputsImages.take(key);
        m_cacheOutputsImages.insert(translationMap.value(key), img);
    }
}

void ScreenShotEffect::postPaintScreen()
{
    effects->postPaintScreen();
    if (m_scheduledScreenshot) {
        WindowPaintData d(m_scheduledScreenshot);
        double left = 0;
        double top = 0;
        double right = m_scheduledScreenshot->width();
        double bottom = m_scheduledScreenshot->height();
        if (m_scheduledScreenshot->hasDecoration() && m_type & INCLUDE_DECORATION) {
            for (const WindowQuad &quad : qAsConst(d.quads)) {
                // we need this loop to include the decoration padding
                left   = qMin(left, quad.left());
                top    = qMin(top, quad.top());
                right  = qMax(right, quad.right());
                bottom = qMax(bottom, quad.bottom());
            }
        } else if (m_scheduledScreenshot->hasDecoration()) {
            WindowQuadList newQuads;
            left = m_scheduledScreenshot->width();
            top = m_scheduledScreenshot->height();
            right = 0;
            bottom = 0;
            for (const WindowQuad &quad : qAsConst(d.quads)) {
                if (quad.type() == WindowQuadContents) {
                    newQuads << quad;
                    left   = qMin(left, quad.left());
                    top    = qMin(top, quad.top());
                    right  = qMax(right, quad.right());
                    bottom = qMax(bottom, quad.bottom());
                }
            }
            d.quads = newQuads;
        }
        const int width = right - left;
        const int height = bottom - top;
        bool validTarget = true;
        QScopedPointer<GLTexture> offscreenTexture;
        QScopedPointer<GLRenderTarget> target;
        if (effects->isOpenGLCompositing()) {
            offscreenTexture.reset(new GLTexture(GL_RGBA8, width, height));
            offscreenTexture->setFilter(GL_LINEAR);
            offscreenTexture->setWrapMode(GL_CLAMP_TO_EDGE);
            target.reset(new GLRenderTarget(*offscreenTexture));
            validTarget = target->valid();
        }
        if (validTarget) {
            d.setXTranslation(-m_scheduledScreenshot->x() - left);
            d.setYTranslation(-m_scheduledScreenshot->y() - top);

            // render window into offscreen texture
            int mask = PAINT_WINDOW_TRANSFORMED | PAINT_WINDOW_TRANSLUCENT;
            QImage img;
            if (effects->isOpenGLCompositing()) {
                GLRenderTarget::pushRenderTarget(target.data());
                glClearColor(0.0, 0.0, 0.0, 0.0);
                glClear(GL_COLOR_BUFFER_BIT);
                glClearColor(0.0, 0.0, 0.0, 1.0);

                QMatrix4x4 projection;
                projection.ortho(QRect(0, 0, offscreenTexture->width(), offscreenTexture->height()));
                d.setProjectionMatrix(projection);

                effects->drawWindow(m_scheduledScreenshot, mask, infiniteRegion(), d);

                // copy content from framebuffer into image
                img = QImage(QSize(width, height), QImage::Format_ARGB32);
                glReadnPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, img.sizeInBytes(), (GLvoid*)img.bits());
                GLRenderTarget::popRenderTarget();
                ScreenShotEffect::convertFromGLImage(img, width, height);
            }
#ifdef KWIN_HAVE_XRENDER_COMPOSITING
            xcb_image_t *xImage = nullptr;
            if (effects->compositingType() == XRenderCompositing) {
                setXRenderOffscreen(true);
                effects->drawWindow(m_scheduledScreenshot, mask, QRegion(0, 0, width, height), d);
                if (xRenderOffscreenTarget()) {
                    img = xPictureToImage(xRenderOffscreenTarget(), QRect(0, 0, width, height), &xImage);
                }
                setXRenderOffscreen(false);
            }
#endif

            if (m_type & INCLUDE_CURSOR) {
                grabPointerImage(img, m_scheduledScreenshot->x() + left, m_scheduledScreenshot->y() + top);
            }

            if (m_windowMode == WindowMode::Xpixmap) {
                const xcb_pixmap_t xpix = xpixmapFromImage(img);
                emit screenshotCreated(xpix);
                m_windowMode = WindowMode::NoCapture;
            } else if (m_windowMode == WindowMode::File || m_windowMode == WindowMode::FileDescriptor) {
                sendReplyImage(img);
            }
#ifdef KWIN_HAVE_XRENDER_COMPOSITING
            if (xImage) {
                xcb_image_destroy(xImage);
            }
#endif
        }
        m_scheduledScreenshot = nullptr;
    }

    if (!m_scheduledGeometry.isNull()) {
        if (!m_cachedOutputGeometry.isNull()) {
            // special handling for per-output geometry rendering
            const QRect intersection = m_scheduledGeometry.intersected(m_cachedOutputGeometry);
            if (intersection.isEmpty()) {
                // doesn't intersect, not going onto this screenshot
                return;
            }
            QImage img = blitScreenshot(intersection, m_cachedScale);
            if (img.size() == (m_scheduledGeometry.size() * m_cachedScale)) {
                // we are done
                sendReplyImage(img);
                return;
            }
            img.setDevicePixelRatio(m_cachedScale);

            m_cacheOutputsImages.insert(ComparableQPoint(m_cachedOutputGeometry.topLeft()), img);

            m_multipleOutputsRendered = m_multipleOutputsRendered.united(intersection);
            if (m_multipleOutputsRendered.boundingRect() == m_scheduledGeometry) {

                // Recompute coordinates
                if (m_nativeSize) {
                    computeCoordinatesAfterScaling();
                }

                // find the output image size
                int width = 0;
                int height = 0;
                QMap<ComparableQPoint, QImage>::const_iterator i;
                for (i = m_cacheOutputsImages.constBegin(); i != m_cacheOutputsImages.constEnd(); ++i) {
                    const auto pos = i.key();
                    const auto img = i.value();

                    width = qMax(width, pos.x() + img.width());
                    height = qMax(height, pos.y() + img.height());
                }

                QImage multipleOutputsImage = QImage(width, height, QImage::Format_ARGB32);

                QPainter p;
                p.begin(&multipleOutputsImage);

                // reassemble images together
                for (i = m_cacheOutputsImages.constBegin(); i != m_cacheOutputsImages.constEnd(); ++i) {
                    auto pos = i.key();
                    auto img = i.value();
                    // disable dpr rendering, we already took care of this
                    img.setDevicePixelRatio(1.0);
                    p.drawImage(pos, img);
                }
                p.end();

                sendReplyImage(multipleOutputsImage);
            }

        } else {
            const QImage img = blitScreenshot(m_scheduledGeometry);
            sendReplyImage(img);
        }
    }
}

void ScreenShotEffect::sendReplyImage(const QImage &img)
{
    if (m_fd != -1) {
        QtConcurrent::run(
            [] (int fd, const QImage &img) {
                QFile file;
                if (file.open(fd, QIODevice::WriteOnly, QFileDevice::AutoCloseHandle)) {
                    QDataStream ds(&file);
                    ds << img;
                    file.close();
                } else {
                    close(fd);
                }
            }, m_fd, img);
        m_fd = -1;
    } else {
        QDBusConnection::sessionBus().send(m_replyMessage.createReply(saveTempImage(img)));
    }
    m_scheduledGeometry = QRect();
    m_multipleOutputsRendered = QRegion();
    m_captureCursor = false;
    m_windowMode = WindowMode::NoCapture;
    m_cacheOutputsImages.clear();
    m_cachedOutputGeometry = QRect();
    m_nativeSize = false;
}

QString ScreenShotEffect::saveTempImage(const QImage &img)
{
    if (img.isNull()) {
        return QString();
    }
    QTemporaryFile temp(QDir::tempPath() + QDir::separator() + QLatin1String("kwin_screenshot_XXXXXX.png"));
    temp.setAutoRemove(false);
    if (!temp.open()) {
        return QString();
    }
    img.save(&temp);
    temp.close();
    KNotification::event(KNotification::Notification,
                        i18nc("Notification caption that a screenshot got saved to file", "Screenshot"),
                        i18nc("Notification with path to screenshot file", "Screenshot saved to %1", temp.fileName()),
                        QStringLiteral("spectacle"));
    return temp.fileName();
}

void ScreenShotEffect::screenshotWindowUnderCursor(int mask)
{
    if (isTakingScreenshot()) {
        sendErrorReply(s_errorAlreadyTaking, s_errorAlreadyTakingMsg);
        return;
    }
    m_type = (ScreenShotType)mask;
    const QPoint cursor = effects->cursorPos();
    EffectWindowList order = effects->stackingOrder();
    EffectWindowList::const_iterator it = order.constEnd(), first = order.constBegin();
    while( it != first ) {
        m_scheduledScreenshot = *(--it);
        if (m_scheduledScreenshot->isOnCurrentDesktop() &&
            !m_scheduledScreenshot->isMinimized() && !m_scheduledScreenshot->isDeleted() &&
            m_scheduledScreenshot->geometry().contains(cursor))
            break;
        m_scheduledScreenshot = nullptr;
    }
    if (m_scheduledScreenshot) {
        m_windowMode = WindowMode::Xpixmap;
        m_scheduledScreenshot->addRepaintFull();
    }
}

void ScreenShotEffect::screenshotForWindow(qulonglong winid, int mask)
{
    m_type = (ScreenShotType) mask;
    EffectWindow* w = effects->findWindow(winid);
    if(w && !w->isMinimized() && !w->isDeleted()) {
        m_windowMode = WindowMode::Xpixmap;
        m_scheduledScreenshot = w;
        m_scheduledScreenshot->addRepaintFull();
    }
}

bool ScreenShotEffect::checkCall() const
{
    if (!calledFromDBus()) {
        return false;
    }

    const QDBusReply<uint> reply = connection().interface()->servicePid(message().service());
    if (reply.isValid()) {
        const uint pid = reply.value();
        const auto interfaces = KWin::fetchRestrictedDBusInterfacesFromPid(pid);
        if (!interfaces.contains(s_dbusInterfaceName)) {
            sendErrorReply(s_errorNotAuthorized, s_errorNotAuthorizedMsg);
            qCWarning(KWINEFFECTS) << "Process " << pid << " tried to take a screenshot without being granted to DBus interface" << s_dbusInterfaceName;
            return false;
        }
    } else {
        return false;
    }

    if (isTakingScreenshot()) {
        sendErrorReply(s_errorAlreadyTaking, s_errorAlreadyTakingMsg);
        return false;
    }
    return true;
}

QString ScreenShotEffect::interactive(int mask)
{
    if (!calledFromDBus()) {
        return QString();
    }
    if (isTakingScreenshot()) {
        sendErrorReply(s_errorAlreadyTaking, s_errorAlreadyTakingMsg);
        return QString();
    }

    m_type = (ScreenShotType) mask;
    m_windowMode = WindowMode::File;
    m_replyMessage = message();
    setDelayedReply(true);
    effects->startInteractiveWindowSelection(
        [this] (EffectWindow *w) {
            hideInfoMessage();
            if (!w) {
                QDBusConnection::sessionBus().send(m_replyMessage.createErrorReply(s_errorCancelled, s_errorCancelledMsg));
                m_windowMode = WindowMode::NoCapture;
                return;
            } else {
                m_scheduledScreenshot = w;
                m_scheduledScreenshot->addRepaintFull();
            }
    });

    showInfoMessage(InfoMessageMode::Window);
    return QString();
}

void ScreenShotEffect::interactive(QDBusUnixFileDescriptor fd, int mask)
{
    if (!calledFromDBus()) {
        return;
    }
    if (isTakingScreenshot()) {
        sendErrorReply(s_errorAlreadyTaking, s_errorAlreadyTakingMsg);
        return;
    }

    m_fd = dup(fd.fileDescriptor());
    if (m_fd == -1) {
        sendErrorReply(s_errorFd, s_errorFdMsg);
        return;
    }
    m_type = (ScreenShotType) mask;
    m_windowMode = WindowMode::FileDescriptor;

    effects->startInteractiveWindowSelection(
        [this] (EffectWindow *w) {
            hideInfoMessage();
            if (!w) {
                close(m_fd);
                m_fd = -1;
                m_windowMode = WindowMode::NoCapture;
                return;
            } else {
                m_scheduledScreenshot = w;
                m_scheduledScreenshot->addRepaintFull();
            }
    });

    showInfoMessage(InfoMessageMode::Window);
}

void ScreenShotEffect::showInfoMessage(InfoMessageMode mode)
{
    QString text;
    switch (mode) {
    case InfoMessageMode::Window:
        text = i18n("Select window to screen shot with left click or enter.\nEscape or right click to cancel.");
        break;
    case InfoMessageMode::Screen:
        text = i18n("Create screen shot with left click or enter.\nEscape or right click to cancel.");
        break;
    }
    effects->showOnScreenMessage(text, QStringLiteral("spectacle"));
}

void ScreenShotEffect::hideInfoMessage()
{
    effects->hideOnScreenMessage(EffectsHandler::OnScreenMessageHideFlag::SkipsCloseAnimation);
}

QString ScreenShotEffect::screenshotFullscreen(bool captureCursor)
{
    if (!checkCall()) {
        return QString();
    }
    m_replyMessage = message();
    setDelayedReply(true);
    m_scheduledGeometry = effects->virtualScreenGeometry();
    m_captureCursor = captureCursor;
    effects->addRepaintFull();
    return QString();
}

void ScreenShotEffect::screenshotFullscreen(QDBusUnixFileDescriptor fd, bool captureCursor, bool shouldReturnNativeSize)
{
    if (!checkCall()) {
        return;
    }
    m_fd = dup(fd.fileDescriptor());
    if (m_fd == -1) {
        sendErrorReply(s_errorFd, s_errorFdMsg);
        return;
    }
    m_captureCursor = captureCursor;
    m_nativeSize = shouldReturnNativeSize;

    m_scheduledGeometry = effects->virtualScreenGeometry();
    effects->addRepaint(m_scheduledGeometry);
}

QString ScreenShotEffect::screenshotScreen(int screen, bool captureCursor)
{
    if (!checkCall()) {
        return QString();
    }
    m_scheduledGeometry = effects->clientArea(FullScreenArea, screen, 0);
    if (m_scheduledGeometry.isNull()) {
        sendErrorReply(s_errorInvalidScreen, s_errorInvalidScreenMsg);
        return QString();
    }
    m_captureCursor = captureCursor;
    m_nativeSize = true;
    m_replyMessage = message();
    setDelayedReply(true);
    effects->addRepaint(m_scheduledGeometry);
    return QString();
}

void ScreenShotEffect::screenshotScreen(QDBusUnixFileDescriptor fd, bool captureCursor)
{
    if (!checkCall()) {
        return;
    }
    m_fd = dup(fd.fileDescriptor());
    if (m_fd == -1) {
        sendErrorReply(s_errorFd, s_errorFdMsg);
        return;
    }
    m_captureCursor = captureCursor;
    m_nativeSize = true;

    showInfoMessage(InfoMessageMode::Screen);
    effects->startInteractivePositionSelection(
        [this] (const QPoint &p) {
            hideInfoMessage();
            if (p == QPoint(-1, -1)) {
                // error condition
                close(m_fd);
                m_fd = -1;
            } else {
                m_scheduledGeometry = effects->clientArea(FullScreenArea, effects->screenNumber(p), 0);
                if (m_scheduledGeometry.isNull()) {
                    close(m_fd);
                    m_fd = -1;
                    return;
                }
                effects->addRepaint(m_scheduledGeometry);
            }
        }
    );
}

QString ScreenShotEffect::screenshotArea(int x, int y, int width, int height, bool captureCursor)
{
    if (!checkCall()) {
        return QString();
    }
    m_scheduledGeometry = QRect(x, y, width, height);
    if (m_scheduledGeometry.isNull() || m_scheduledGeometry.isEmpty()) {
        m_scheduledGeometry = QRect();
        sendErrorReply(s_errorInvalidArea, s_errorInvalidAreaMsg);
        return QString();
    }
    m_captureCursor = captureCursor;
    m_replyMessage = message();
    setDelayedReply(true);
    effects->addRepaint(m_scheduledGeometry);
    return QString();
}

QImage ScreenShotEffect::blitScreenshot(const QRect &geometry, const qreal scale)
{
    QImage img;
    if (effects->isOpenGLCompositing())
    {
        const QSize nativeSize = geometry.size() * scale;

        if (GLRenderTarget::blitSupported() && !GLPlatform::instance()->isGLES()) {

            img = QImage(nativeSize.width(), nativeSize.height(), QImage::Format_ARGB32);
            GLTexture tex(GL_RGBA8, nativeSize.width(), nativeSize.height());
            GLRenderTarget target(tex);
            target.blitFromFramebuffer(geometry);
            // copy content from framebuffer into image
            tex.bind();
            glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, static_cast<GLvoid*>(img.bits()));
            tex.unbind();
        } else {
            img = QImage(nativeSize.width(), nativeSize.height(), QImage::Format_ARGB32);
            glReadPixels(0, 0, nativeSize.width(), nativeSize.height(), GL_RGBA, GL_UNSIGNED_BYTE, (GLvoid*)img.bits());
        }
        ScreenShotEffect::convertFromGLImage(img, nativeSize.width(), nativeSize.height());
    }

#ifdef KWIN_HAVE_XRENDER_COMPOSITING
    if (effects->compositingType() == XRenderCompositing) {
    xcb_image_t *xImage = nullptr;
        img = xPictureToImage(effects->xrenderBufferPicture(), geometry, &xImage);
        if (xImage) {
            xcb_image_destroy(xImage);
        }
    }
#endif

    if (m_captureCursor) {
        grabPointerImage(img, geometry.x() * scale, geometry.y() * scale);
    }

    return img;
}

void ScreenShotEffect::grabPointerImage(QImage& snapshot, int offsetx, int offsety)
{
    const auto cursor = effects->cursorImage();
    if (cursor.image().isNull())
        return;

    QPainter painter(&snapshot);
    painter.drawImage(effects->cursorPos() - cursor.hotSpot() - QPoint(offsetx, offsety), cursor.image());
}

void ScreenShotEffect::convertFromGLImage(QImage &img, int w, int h)
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

bool ScreenShotEffect::isActive() const
{
    return (m_scheduledScreenshot != nullptr || !m_scheduledGeometry.isNull()) && !effects->isScreenLocked();
}

void ScreenShotEffect::windowClosed( EffectWindow* w )
{
    if (w == m_scheduledScreenshot) {
        m_scheduledScreenshot = nullptr;
        screenshotWindowUnderCursor(m_type);
    }
}

bool ScreenShotEffect::isTakingScreenshot() const
{
    if (!m_scheduledGeometry.isNull()) {
        return true;
    }
    if (m_windowMode != WindowMode::NoCapture) {
        return true;
    }
    if (m_fd != -1) {
        return true;
    }
    return false;
}

} // namespace
