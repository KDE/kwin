/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2019 David Edmundson <davidedmundson@kde.org>

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

#pragma once

#include <QObject>
#include <QUrl>
#include <QRect>

#include <kwineffects_export.h>

//#include "kwineffects.h"

#include <memory>

class QKeyEvent;
class QMouseEvent;
class QOpenGLContext;

class QMouseEvent;
class QKeyEvent;

class QQmlContext;
class QQuickItem;

namespace KWin
{
class GLTexture;

class EffectQuickView;

/**
 * @brief The KwinQuickView class provides a convenient API for exporting
 * QtQuick scenes as buffers that can be composited in any other fashion.
 *
 * Contents can be fetched as a GL Texture or as a QImage
 * If data is to be fetched as an image, it should be specified upfront as
 * blitting is performed when we update our FBO to keep kwin's render loop
 * as fast as possible.
 */
class KWINEFFECTS_EXPORT EffectQuickView : public QObject
{
    Q_OBJECT

public:
    static void setShareContext(std::unique_ptr<QOpenGLContext> context);

    enum class ExportMode {
        /** The contents will be available as a texture in the shared contexts. Image will be blank*/
        Texture,
        /** The contents will be blit during the update into a QImage buffer. */
        Image
    };

    /**
     * Construct a new KWinQuickView
     * Export mode will be determined by the current effectsHandler
     */
    EffectQuickView(QObject *parent);

    /**
     * Construct a new KWinQuickView explicitly stating an export mode
     */
    EffectQuickView(QObject *parent, ExportMode exportMode);

    /**
     * Note that this may change the current GL Context
     */
    ~EffectQuickView();

    QSize size() const;

    /**
     * The geometry of the current view
     * This may be out of sync with the current buffer size if an update is pending
     */
    void setGeometry(const QRect &rect);
    QRect geometry() const;

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

    /**
     * @brief Marks the window as visible/invisible
     * This can be used to release resources used by the window
     * The default is true.
     */
    void setVisible(bool visible);
    bool isVisible() const;

    void show();
    void hide();

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

Q_SIGNALS:
    /**
     * The frame buffer has changed, contents need re-rendering on screen
     */
    void repaintNeeded();
    void geometryChanged(const QRect &oldGeometry, const QRect &newGeometry);

private:
    class Private;
    QScopedPointer<Private> d;
};

/**
 * The KWinQuickScene class extends KWinQuickView
 * adding QML support. This will represent a context
 * powered by an engine
 */
class KWINEFFECTS_EXPORT EffectQuickScene : public EffectQuickView
{
public:
    EffectQuickScene(QObject *parent);
    EffectQuickScene(QObject *parent, ExportMode exportMode);
    ~EffectQuickScene();

    QQmlContext *rootContext() const;
    /** top level item in the given source*/
    QQuickItem *rootItem() const;

    void setSource(const QUrl &source);

private:
    class Private;
    QScopedPointer<Private> d;
};

}
