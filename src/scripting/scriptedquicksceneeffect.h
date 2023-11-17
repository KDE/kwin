/*
    SPDX-FileCopyrightText: 2023 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "effect/quickeffect.h"

#include <KConfigPropertyMap>

#include <QTimer>

class KConfigLoader;
class KConfigPropertyMap;

namespace KWin
{

/**
 * The SceneEffect type provides a way to implement effects that replace the default scene with
 * a custom one.
 *
 * Example usage:
 * @code
 * SceneEffect {
 *     id: root
 *
 *     delegate: Rectangle {
 *         color: "blue"
 *     }
 *
 *     ShortcutHandler {
 *         name: "Toggle Effect"
 *         text: i18n("Toggle Effect")
 *         sequence: "Meta+E"
 *         onActivated: root.visible = !root.visible;
 *     }
 * }
 * @endcode
 */
class ScriptedQuickSceneEffect : public QuickSceneEffect
{
    Q_OBJECT
    Q_PROPERTY(QQmlListProperty<QObject> data READ data)
    Q_CLASSINFO("DefaultProperty", "data")

    /**
     * The key-value store with the effect settings.
     */
    Q_PROPERTY(KConfigPropertyMap *configuration READ configuration NOTIFY configurationChanged)

    /**
     * Whether the effect is shown. Setting this property to @c true activates the effect; setting
     * this property to @c false will deactivate the effect and the screen views will be unloaded at
     * the next available time.
     */
    Q_PROPERTY(bool visible READ isVisible WRITE setVisible NOTIFY visibleChanged)

public:
    explicit ScriptedQuickSceneEffect();
    ~ScriptedQuickSceneEffect() override;

    void setMetaData(const KPluginMetaData &metaData);

    void reconfigure(ReconfigureFlags flags) override;
    int requestedEffectChainPosition() const override;

    bool isVisible() const;
    void setVisible(bool visible);

    QQmlListProperty<QObject> data();
    KConfigPropertyMap *configuration() const;

    static void data_append(QQmlListProperty<QObject> *objects, QObject *object);
    static qsizetype data_count(QQmlListProperty<QObject> *objects);
    static QObject *data_at(QQmlListProperty<QObject> *objects, qsizetype index);
    static void data_clear(QQmlListProperty<QObject> *objects);

Q_SIGNALS:
    void visibleChanged();
    void configurationChanged();

private:
    KConfigLoader *m_configLoader = nullptr;
    KConfigPropertyMap *m_configuration = nullptr;
    QObjectList m_children;
    QTimer m_visibleTimer;
    bool m_isVisible = false;
    int m_requestedEffectChainPosition = 0;
};

} // namespace KWin
