/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "core/output.h"
#include "effect/effect.h"
#include "effect/offscreenquickview.h"

#include <QQmlComponent>

namespace KWin
{

class QuickSceneEffect;
class QuickSceneEffectPrivate;

/*!
 * \qmltype SceneView
 * \inqmlmodule org.kde.kwin
 *
 * \brief Represents a QtQuick scene view on a particular screen.
 *
 * The root QML object must be a QQuickItem or a subclass of QQuickItem, other
 * cases are unsupported.
 *
 * \sa QuickSceneEffect, OffscreenQuickView
 */

/*!
 * \class KWin::QuickSceneView
 * \inmodule KWin
 * \inheaderfile effect/quickeffect.h
 *
 * \brief Represents a QtQuick scene view on a particular screen.
 *
 * \sa QuickSceneEffect, OffscreenQuickView
 */
class KWIN_EXPORT QuickSceneView : public OffscreenQuickView
{
    Q_OBJECT

    /*!
     * \qmlattachedproperty QuickSceneEffect SceneView::effect
     */

    /*!
     * \property KWin::QuickSceneView::effect
     */
    Q_PROPERTY(QuickSceneEffect *effect READ effect CONSTANT)

    /*!
     * \qmlattachedproperty LogicalOutput SceneView::screen
     */

    /*!
     * \property KWin::QuickSceneView::screen
     */
    Q_PROPERTY(LogicalOutput *screen READ screen CONSTANT)

    /*!
     * \qmlattachedproperty Item SceneView::rootItem
     */

    /*!
     * \property KWin::QuickSceneView::rootItem
     */
    Q_PROPERTY(QQuickItem *rootItem READ rootItem CONSTANT)

public:
    /*!
     * Constructs a scene view for the given \a effect and \a screen.
     */
    explicit QuickSceneView(QuickSceneEffect *effect, LogicalOutput *screen);
    ~QuickSceneView() override;

    QuickSceneEffect *effect() const;
    LogicalOutput *screen() const;

    QQuickItem *rootItem() const;
    void setRootItem(QQuickItem *item);

    /*!
     * \internal
     */
    bool isDirty() const;

    /*!
     * \internal
     */
    void markDirty();

    /*!
     * \internal
     */
    void resetDirty();

    /*!
     * Returns the scene view for the given \a item or \c null if no view is associated
     * with the given item.
     */
    static QuickSceneView *findView(QQuickItem *item);

    static QuickSceneView *qmlAttachedProperties(QObject *object);

public Q_SLOTS:
    /*!
     * \qmlattachedmethod void SceneView::scheduleRepaint
     */

    /*!
     * Schedules a repaint. The view will be repainted at the next available opportunity.
     */
    void scheduleRepaint();

private:
    QuickSceneEffect *m_effect;
    LogicalOutput *m_screen;
    std::unique_ptr<QQuickItem> m_rootItem;
    bool m_dirty = false;
};

/*!
 * \qmltype QuickSceneEffect
 * \inqmlmodule org.kde.kwin
 *
 * \brief Provides a convenient way to write fullscreen
 * QtQuick-based effects.
 *
 * QuickSceneView objects are managed internally.
 *
 * The QuickSceneEffect takes care of forwarding input events to QuickSceneView and
 * rendering. You can override relevant hooks from the Effect class to customize input
 * handling or rendering, although it's highly recommended that you avoid doing that.
 *
 * This type is not directly creatable from QML.
 *
 * \sa QuickSceneView
 */

/*!
 * \class KWin::QuickSceneEffect
 * \inmodule KWin
 * \inheaderfile effect/quickeffect.h
 *
 * \brief Provides a convenient way to write fullscreen
 * QtQuick-based effects.
 */
class KWIN_EXPORT QuickSceneEffect : public Effect
{
    Q_OBJECT

    /*!
     * \qmlproperty SceneView QuickSceneEffect::activeView
     */

    /*!
     * \property KWin::QuickSceneEffect::activateView
     */
    Q_PROPERTY(QuickSceneView *activeView READ activeView NOTIFY activeViewChanged)

    /*!
     * \qmlproperty Component QuickSceneEffect::delegate
     *
     * The delegate provides a template defining the contents of each instantiated screen view
     */

    /*!
     * \property KWin::QuickSceneEffect::delegate
     *
     * The delegate provides a template defining the contents of each instantiated screen view
     */
    Q_PROPERTY(QQmlComponent *delegate READ delegate WRITE setDelegate NOTIFY delegateChanged)

public:
    /*!
     * Constructs a QtQuick scene effect with the given \a parent.
     */
    explicit QuickSceneEffect(QObject *parent = nullptr);
    ~QuickSceneEffect() override;

    /*!
     * Returns \c true if the effect is running; otherwise returns \c false.
     */
    bool isRunning() const;

    /*!
     * Starts or stops the effect depending on \a running.
     */
    void setRunning(bool running);

    /*!
     * Returns the active view. The active view is usually a view that the user interacted with
     * last time.
     */
    QuickSceneView *activeView() const;

    /*!
     * \qmlmethod SceneView QuickSceneEffect::viewForScreen(LogicalOutput screen)
     * Returns the scene view on the specified screen
     */

    /*!
     * Returns the scene view on the specified screen
     */
    Q_INVOKABLE QuickSceneView *viewForScreen(LogicalOutput *screen) const;

    /*!
     * \qmlmethod SceneView QuickSceneEffect::viewAt(point pos)
     * Returns the scene view on the specified screen
     */

    /*!
     * Returns the view at the specified \a pos in the global screen coordinates.
     */
    Q_INVOKABLE QuickSceneView *viewAt(const QPoint &pos) const;

    /*!
     * \qmlmethod SceneView QuickSceneEffect::getView(Qt.Edge edge)
     * Get a view at the given direction from the active view
     *
     * Returns null if no other views exist in the given direction
     */

    /*!
     * Get a view at the given direction from the active view
     *
     * Returns null if no other views exist in the given direction
     */
    Q_INVOKABLE KWin::QuickSceneView *getView(Qt::Edge edge);

    /*!
     * \qmlmethod void activateView(SceneView view)
     * Sets the given \a view as active.
     *
     * It will get a focusin event and all the other views will be set as inactive
     */

    /*!
     * Sets the given \a view as active.
     *
     * It will get a focusin event and all the other views will be set as inactive
     */
    Q_INVOKABLE void activateView(QuickSceneView *view);

    QQmlComponent *delegate() const;
    void setDelegate(QQmlComponent *delegate);

    /*!
     * Returns the source URL.
     */
    QUrl source() const;

    /*!
     * Sets the source url to \a url.
     *
     * Note that the QML component will be loaded the next
     * time the effect is started.
     *
     * While the effect is running, the source url cannot be changed.
     *
     * In order to provide your custom initial properties, you need to override
     * the initialProperties() function.
     */
    void setSource(const QUrl &url);

    /*!
     * Use the QML component identified by \a uri and \a typename.
     *
     * Note that the QML component will
     * be loaded the next time the effect is started.
     *
     * Cannot be called while the effect is running.
     *
     * In order to provide your custom initial properties, you need to override
     * the initialProperties() function.
     */
    void loadFromModule(const QString &uri, const QString &typeName);

    bool eventFilter(QObject *watched, QEvent *event) override;

    void prePaintScreen(ScreenPrePaintData &data, std::chrono::milliseconds presentTime) override;
    void paintScreen(const RenderTarget &renderTarget, const RenderViewport &viewport, int mask, const Region &logicalRegion, LogicalOutput *screen) override;
    bool isActive() const override;

    void grabbedKeyboardEvent(QKeyEvent *keyEvent) override;

    void pointerMotion(PointerMotionEvent *event) override;
    void pointerButton(PointerButtonEvent *event) override;
    void pointerAxis(PointerAxisEvent *event) override;

    bool touchDown(qint32 id, const QPointF &pos, std::chrono::microseconds time) override;
    bool touchMotion(qint32 id, const QPointF &pos, std::chrono::microseconds time) override;
    bool touchUp(qint32 id, std::chrono::microseconds time) override;
    void touchCancel() override;

    static bool supported();

    Q_INVOKABLE void checkItemDraggedOutOfScreen(QQuickItem *item);

    Q_INVOKABLE void checkItemDroppedOutOfScreen(const QPointF &globalPos, QQuickItem *item);

Q_SIGNALS:
    void itemDraggedOutOfScreen(QQuickItem *item, QList<LogicalOutput *> screens);
    void itemDroppedOutOfScreen(const QPointF &globalPos, QQuickItem *item, LogicalOutput *screen);
    void activeViewChanged(KWin::QuickSceneView *view);
    void delegateChanged();

protected:
    /*!
     * Reimplement this function to provide your initial properties for the scene view
     * on the specified \a screen.
     *
     * \sa QQmlComponent::createWithInitialProperties()
     */
    virtual QVariantMap initialProperties(LogicalOutput *screen);

private:
    void handleScreenAdded(LogicalOutput *screen);
    void handleScreenRemoved(LogicalOutput *screen);

    void addScreen(LogicalOutput *screen);
    void removeScreen(LogicalOutput *screen);
    void startInternal();
    void stopInternal();

    std::unique_ptr<QuickSceneEffectPrivate> d;
    friend class QuickSceneEffectPrivate;
};

} // namespace KWin

QML_DECLARE_TYPEINFO(KWin::QuickSceneView, QML_HAS_ATTACHED_PROPERTIES)
