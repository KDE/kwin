/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2019 David Edmundson <davidedmundson@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "kwin_export.h"

#include <QObject>
#include <QRect>
#include <QUrl>

#include <memory>

class QKeyEvent;
class QMouseEvent;

class QMouseEvent;
class QKeyEvent;

class QQmlContext;
class QQuickItem;
class QQuickWindow;

namespace KWin
{
class GLTexture;

class OffscreenQuickView;

/**
 * @brief The KwinQuickView class provides a convenient API for exporting
 * QtQuick scenes as buffers that can be composited in any other fashion.
 *
 * Contents can be fetched as a GL Texture or as a QImage
 * If data is to be fetched as an image, it should be specified upfront as
 * blitting is performed when we update our FBO to keep kwin's render loop
 * as fast as possible.
 */
class KWIN_EXPORT OffscreenQuickView : public QObject
{
    Q_OBJECT

public:
    enum class ExportMode {
        /** The contents will be available as a texture in the shared contexts. Image will be blank*/
        Texture,
        /** The contents will be blit during the update into a QImage buffer. */
        Image
    };

    /**
     * Construct a new KWinQuickView explicitly stating an export mode. \a alpha indicates
     * whether the view is translucent or not.
     */
    explicit OffscreenQuickView(ExportMode exportMode = ExportMode::Texture, bool alpha = true);

    /**
     * Note that this may change the current GL Context
     */
    ~OffscreenQuickView();

    QSize size() const;

    /**
     * The geometry of the current view
     * This may be out of sync with the current buffer size if an update is pending
     */
    void setGeometry(const QRect &rect);
    QRect geometry() const;

    void setOpacity(qreal opacity);
    qreal opacity() const;
    bool hasAlphaChannel() const;

    /**
     * Render the current scene graph into the FBO.
     * This is typically done automatically when the scene changes
     * albeit deffered by a timer
     *
     * It can be manually invoked to update the contents immediately.
     * Note this will change the GL context
     */
    void update();

    /** The invisble root item of the window*/
    QQuickItem *contentItem() const;
    QQuickWindow *window() const;

    /**
     * @brief Marks the window as visible/invisible
     * This can be used to release resources used by the window
     * The default is true.
     */
    void setVisible(bool visible);
    bool isVisible() const;

    void show();
    void hide();

    bool automaticRepaint() const;
    void setAutomaticRepaint(bool set);

    void setDevicePixelRatio(qreal dpr);

    /**
     * Returns the current output of the scene graph
     * @note The render context must valid at the time of calling
     */
    GLTexture *bufferAsTexture();

    /**
     * Returns the current output of the scene graph
     */
    QImage bufferAsImage() const;

    /**
     * Inject any mouse event into the QQuickWindow.
     * Local co-ordinates are transformed
     * If it is handled the event will be accepted
     */
    void forwardMouseEvent(QEvent *mouseEvent);
    /**
     * Inject a key event into the window.
     * If it is handled the event will be accepted
     */
    void forwardKeyEvent(QKeyEvent *keyEvent);

    bool forwardTouchDown(qint32 id, const QPointF &pos, std::chrono::microseconds time);
    bool forwardTouchMotion(qint32 id, const QPointF &pos, std::chrono::microseconds time);
    bool forwardTouchUp(qint32 id, std::chrono::microseconds time);
    void forwardTouchCancel();

Q_SIGNALS:
    /**
     * The frame buffer has changed, contents need re-rendering on screen
     */
    void repaintNeeded();
    void geometryChanged(const QRect &oldGeometry, const QRect &newGeometry);
    void renderRequested();
    void sceneChanged();

private:
    void handleRenderRequested();
    void handleSceneChanged();

    class Private;
    std::unique_ptr<Private> d;
};

/**
 * The KWinQuickScene class extends KWinQuickView
 * adding QML support. This will represent a context
 * powered by an engine
 */
class KWIN_EXPORT OffscreenQuickScene : public OffscreenQuickView
{
public:
    explicit OffscreenQuickScene(ExportMode exportMode = ExportMode::Texture, bool alpha = true);
    ~OffscreenQuickScene();

    /** top level item in the given source*/
    QQuickItem *rootItem() const;

    void setSource(const QUrl &source);
    void setSource(const QUrl &source, const QVariantMap &initialProperties);

private:
    class Private;
    std::unique_ptr<Private> d;
};

}
