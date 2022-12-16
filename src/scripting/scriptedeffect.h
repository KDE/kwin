/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2012 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2018 David Edmundson <davidedmundson@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <kwinanimationeffect.h>

#include <QJSEngine>
#include <QJSValue>

class KConfigLoader;
class KPluginMetaData;

namespace KWin
{

class KWIN_EXPORT ScriptedEffect : public KWin::AnimationEffect
{
    Q_OBJECT
    Q_ENUMS(DataRole)
    Q_ENUMS(Qt::Axis)
    Q_ENUMS(Anchor)
    Q_ENUMS(MetaType)
    Q_ENUMS(EasingCurve)
    Q_ENUMS(SessionState)
    Q_ENUMS(ElectricBorder)
    Q_ENUMS(ShaderTrait)
    /**
     * The plugin ID of the effect
     */
    Q_PROPERTY(QString pluginId READ pluginId CONSTANT)
    /**
     * True if we are the active fullscreen effect
     */
    Q_PROPERTY(bool isActiveFullScreenEffect READ isActiveFullScreenEffect NOTIFY isActiveFullScreenEffectChanged)

public:
    // copied from kwineffects.h
    enum DataRole {
        // Grab roles are used to force all other animations to ignore the window.
        // The value of the data is set to the Effect's `this` value.
        WindowAddedGrabRole = 1,
        WindowClosedGrabRole,
        WindowMinimizedGrabRole,
        WindowUnminimizedGrabRole,
        WindowForceBlurRole, ///< For fullscreen effects to enforce blurring of windows,
        WindowBlurBehindRole, ///< For single windows to blur behind
        WindowForceBackgroundContrastRole, ///< For fullscreen effects to enforce the background contrast,
        WindowBackgroundContrastRole, ///< For single windows to enable Background contrast
    };
    enum EasingCurve {
        GaussianCurve = 128
    };
    // copied from kwinglutils.h
    enum class ShaderTrait {
        MapTexture = (1 << 0),
        UniformColor = (1 << 1),
        Modulate = (1 << 2),
        AdjustSaturation = (1 << 3),
    };

    const QString &scriptFile() const
    {
        return m_scriptFile;
    }
    void reconfigure(ReconfigureFlags flags) override;
    int requestedEffectChainPosition() const override
    {
        return m_chainPosition;
    }
    QString activeConfig() const;
    void setActiveConfig(const QString &name);
    static ScriptedEffect *create(const QString &effectName, const QString &pathToScript, int chainPosition, const QString &exclusiveCategory);
    static ScriptedEffect *create(const KPluginMetaData &effect);
    static bool supported();
    ~ScriptedEffect() override;
    /**
     * Whether another effect has grabbed the @p w with the given @p grabRole.
     * @param w The window to check
     * @param grabRole The grab role to check
     * @returns @c true if another window has grabbed the effect, @c false otherwise
     */
    Q_SCRIPTABLE bool isGrabbed(KWin::EffectWindow *w, DataRole grabRole);

    /**
     * Grabs the window with the specified role.
     *
     * @param w The window.
     * @param grabRole The grab role.
     * @param force By default, if the window is already grabbed by another effect,
     *   then that window won't be grabbed by effect that called this method. If you
     *   would like to grab a window even if it's grabbed by another effect, then
     *   pass @c true.
     * @returns @c true if the window was grabbed successfully, otherwise @c false.
     */
    Q_SCRIPTABLE bool grab(KWin::EffectWindow *w, DataRole grabRole, bool force = false);

    /**
     * Ungrabs the window with the specified role.
     *
     * @param w The window.
     * @param grabRole The grab role.
     * @returns @c true if the window was ungrabbed successfully, otherwise @c false.
     */
    Q_SCRIPTABLE bool ungrab(KWin::EffectWindow *w, DataRole grabRole);

    /**
     * Reads the value from the configuration data for the given key.
     * @param key The key to search for
     * @param defaultValue The value to return if the key is not found
     * @returns The config value if present
     */
    Q_SCRIPTABLE QJSValue readConfig(const QString &key, const QJSValue &defaultValue = QJSValue());

    Q_SCRIPTABLE int displayWidth() const;
    Q_SCRIPTABLE int displayHeight() const;
    Q_SCRIPTABLE int animationTime(int defaultTime) const;

    Q_SCRIPTABLE void registerShortcut(const QString &objectName, const QString &text,
                                       const QString &keySequence, const QJSValue &callback);
    Q_SCRIPTABLE bool registerScreenEdge(int edge, const QJSValue &callback);
    Q_SCRIPTABLE bool registerRealtimeScreenEdge(int edge, const QJSValue &callback);
    Q_SCRIPTABLE bool unregisterScreenEdge(int edge);
    Q_SCRIPTABLE bool registerTouchScreenEdge(int edge, const QJSValue &callback);
    Q_SCRIPTABLE bool unregisterTouchScreenEdge(int edge);

    Q_SCRIPTABLE quint64 animate(KWin::EffectWindow *window, Attribute attribute, int ms,
                                 const QJSValue &to, const QJSValue &from = QJSValue(),
                                 uint metaData = 0, int curve = QEasingCurve::Linear, int delay = 0,
                                 bool fullScreen = false, bool keepAlive = true, uint shaderId = 0);
    Q_SCRIPTABLE QJSValue animate(const QJSValue &object);

    Q_SCRIPTABLE quint64 set(KWin::EffectWindow *window, Attribute attribute, int ms,
                             const QJSValue &to, const QJSValue &from = QJSValue(),
                             uint metaData = 0, int curve = QEasingCurve::Linear, int delay = 0,
                             bool fullScreen = false, bool keepAlive = true, uint shaderId = 0);
    Q_SCRIPTABLE QJSValue set(const QJSValue &object);

    Q_SCRIPTABLE bool retarget(quint64 animationId, const QJSValue &newTarget,
                               int newRemainingTime = -1);
    Q_SCRIPTABLE bool retarget(const QList<quint64> &animationIds, const QJSValue &newTarget,
                               int newRemainingTime = -1);
    Q_SCRIPTABLE bool freezeInTime(quint64 animationId, qint64 frozenTime);
    Q_SCRIPTABLE bool freezeInTime(const QList<quint64> &animationIds, qint64 frozenTime);

    Q_SCRIPTABLE bool redirect(quint64 animationId, Direction direction,
                               TerminationFlags terminationFlags = TerminateAtSource);
    Q_SCRIPTABLE bool redirect(const QList<quint64> &animationIds, Direction direction,
                               TerminationFlags terminationFlags = TerminateAtSource);

    Q_SCRIPTABLE bool complete(quint64 animationId);
    Q_SCRIPTABLE bool complete(const QList<quint64> &animationIds);

    Q_SCRIPTABLE bool cancel(quint64 animationId);
    Q_SCRIPTABLE bool cancel(const QList<quint64> &animationIds);

    Q_SCRIPTABLE QList<int> touchEdgesForAction(const QString &action) const;

    Q_SCRIPTABLE uint addFragmentShader(ShaderTrait traits, const QString &fragmentShaderFile = {});

    Q_SCRIPTABLE void setUniform(uint shaderId, const QString &name, const QJSValue &value);

    QHash<int, QJSValueList> &screenEdgeCallbacks()
    {
        return m_screenEdgeCallbacks;
    }

    QHash<int, QJSValueList> &realtimeScreenEdgeCallbacks()
    {
        return m_realtimeScreenEdgeCallbacks;
    }

    QString pluginId() const;
    bool isActiveFullScreenEffect() const;

public Q_SLOTS:
    bool borderActivated(ElectricBorder border) override;

Q_SIGNALS:
    /**
     * Signal emitted whenever the effect's config changed.
     */
    void configChanged();
    void animationEnded(KWin::EffectWindow *w, quint64 animationId);
    void isActiveFullScreenEffectChanged();

protected:
    ScriptedEffect();
    QJSEngine *engine() const;
    bool init(const QString &effectName, const QString &pathToScript);
    void animationEnded(KWin::EffectWindow *w, Attribute a, uint meta) override;

private:
    enum class AnimationType {
        Animate,
        Set
    };

    QJSValue animate_helper(const QJSValue &object, AnimationType animationType);

    GLShader *findShader(uint shaderId) const;

    QJSEngine *m_engine;
    QString m_effectName;
    QString m_scriptFile;
    QString m_exclusiveCategory;
    QHash<int, QJSValueList> m_screenEdgeCallbacks;
    QHash<int, QJSValueList> m_realtimeScreenEdgeCallbacks;
    KConfigLoader *m_config;
    int m_chainPosition;
    QHash<int, QAction *> m_touchScreenEdgeCallbacks;
    Effect *m_activeFullScreenEffect = nullptr;
    std::map<uint, std::unique_ptr<GLShader>> m_shaders;
    uint m_nextShaderId{1u};
};
}
