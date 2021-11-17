/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "kwinoffscreenquickview.h"
#include "kwineffects.h"

namespace KWin
{

class QuickSceneEffect;
class QuickSceneEffectPrivate;

/**
 * The QuickSceneView represents a QtQuick scene view on a particular screen.
 *
 * The root QML object must be a QQuickItem or a subclass of QQuickItem, other
 * cases are unsupported.
 *
 * @see QuickSceneEffect, OffscreenQuickView
 */
class KWINEFFECTS_EXPORT QuickSceneView : public OffscreenQuickView
{
    Q_OBJECT

public:
    explicit QuickSceneView(QuickSceneEffect *effect, EffectScreen *screen);
    ~QuickSceneView() override;

    QuickSceneEffect *effect() const;
    EffectScreen *screen() const;

    QQuickItem *rootItem() const;
    void setRootItem(QQuickItem *item);

    bool isDirty() const;
    void markDirty();
    void resetDirty();

public Q_SLOTS:
    void scheduleRepaint();

private:
    QuickSceneEffect *m_effect;
    EffectScreen *m_screen;
    QScopedPointer<QQuickItem> m_rootItem;
    bool m_dirty = false;
};

/**
 * The QuickSceneEffect class provides a convenient way to write fullscreen
 * QtQuick-based effects.
 *
 * QuickSceneView objects are managed internally.
 *
 * The QuickSceneEffect takes care of forwarding input events to QuickSceneView and
 * rendering. You can override relevant hooks from the Effect class to customize input
 * handling or rendering, although it's highly recommended that you avoid doing that.
 *
 * @see QuickSceneView
 */
class KWINEFFECTS_EXPORT QuickSceneEffect : public Effect
{
    Q_OBJECT

public:
    explicit QuickSceneEffect(QObject *parent = nullptr);
    ~QuickSceneEffect() override;

    /**
     * Returns @c true if the effect is running; otherwise returns @c false.
     */
    bool isRunning() const;

    /**
     * Starts or stops the effect depending on @a running.
     */
    void setRunning(bool running);

    /**
     * Returns all scene views managed by this effect. If the effect is not running,
     * this function returns an empty QHash.
     */
    QHash<EffectScreen *, QuickSceneView *> views() const;

    /**
     * Returns the view at the specified @a pos in the global screen coordinates.
     */
    QuickSceneView *viewAt(const QPoint &pos) const;

    /**
     * Returns the source URL.
     */
    QUrl source() const;

    /**
     * Sets the source url to @a url. Note that the QML component will be loaded the next
     * time the effect is started.
     *
     * While the effect is running, the source url cannot be changed.
     *
     * In order to provide your custom initial properties, you need to override
     * the initialProperties() function.
     */
    void setSource(const QUrl &url);

    bool eventFilter(QObject *watched, QEvent *event) override;

    void paintScreen(int mask, const QRegion &region, ScreenPaintData &data) override;
    void postPaintScreen() override;
    bool isActive() const override;

    void windowInputMouseEvent(QEvent *event) override;
    void grabbedKeyboardEvent(QKeyEvent *keyEvent) override;

    bool touchDown(qint32 id, const QPointF &pos, quint32 time) override;
    bool touchMotion(qint32 id, const QPointF &pos, quint32 time) override;
    bool touchUp(qint32 id, quint32 time) override;

    static bool supported();

protected:
    /**
     * Reimplement this function to provide your initial properties for the scene view
     * on the specified @a screen.
     *
     * @see QQmlComponent::createWithInitialProperties()
     */
    virtual QVariantMap initialProperties(EffectScreen *screen);

private:
    void handleScreenAdded(EffectScreen *screen);
    void handleScreenRemoved(EffectScreen *screen);

    void addScreen(EffectScreen *screen);
    void startInternal();
    void stopInternal();

    QScopedPointer<QuickSceneEffectPrivate> d;
    friend class QuickSceneEffectPrivate;
};

} // namespace KWin
