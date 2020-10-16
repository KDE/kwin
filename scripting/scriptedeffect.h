/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2012 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef KWIN_SCRIPTEDEFFECT_H
#define KWIN_SCRIPTEDEFFECT_H

#include <kwinanimationeffect.h>

class KConfigLoader;
class KPluginMetaData;
class QScriptEngine;
class QScriptValue;

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
        LanczosCacheRole
    };
    enum EasingCurve {
        GaussianCurve = 128
    };
    const QString &scriptFile() const {
        return m_scriptFile;
    }
    void reconfigure(ReconfigureFlags flags) override;
    int requestedEffectChainPosition() const override {
        return m_chainPosition;
    }
    QString activeConfig() const;
    void setActiveConfig(const QString &name);
    static ScriptedEffect *create(const QString &effectName, const QString &pathToScript, int chainPosition);
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
    Q_SCRIPTABLE QVariant readConfig(const QString &key, const QVariant defaultValue = QVariant());
    void registerShortcut(QAction *a, QScriptValue callback);
    const QHash<QAction*, QScriptValue> &shortcutCallbacks() const {
        return m_shortcutCallbacks;
    }
    QHash<int, QList<QScriptValue > > &screenEdgeCallbacks() {
        return m_screenEdgeCallbacks;
    }

    QString pluginId() const;

    bool isActiveFullScreenEffect() const;

    bool registerTouchScreenCallback(int edge, QScriptValue callback);
    bool unregisterTouchScreenCallback(int edge);

public Q_SLOTS:
    //curve should be of type QEasingCurve::type or ScriptedEffect::EasingCurve
    quint64 animate(KWin::EffectWindow *w, Attribute a, int ms, KWin::FPx2 to, KWin::FPx2 from = KWin::FPx2(), uint metaData = 0, int curve = QEasingCurve::Linear, int delay = 0, bool fullScreen = false, bool keepAlive = true);
    quint64 set(KWin::EffectWindow *w, Attribute a, int ms, KWin::FPx2 to, KWin::FPx2 from = KWin::FPx2(), uint metaData = 0, int curve = QEasingCurve::Linear, int delay = 0, bool fullScreen = false, bool keepAlive = true);
    bool retarget(quint64 animationId, KWin::FPx2 newTarget, int newRemainingTime = -1);
    bool redirect(quint64 animationId, Direction direction, TerminationFlags terminationFlags = TerminateAtSource);
    bool complete(quint64 animationId);
    bool cancel(quint64 animationId) { return AnimationEffect::cancel(animationId); }
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
    QScriptEngine *engine() const;
    bool init(const QString &effectName, const QString &pathToScript);
    void animationEnded(KWin::EffectWindow *w, Attribute a, uint meta) override;

private Q_SLOTS:
    void signalHandlerException(const QScriptValue &value);
    void globalShortcutTriggered();
private:
    QScriptEngine *m_engine;
    QString m_effectName;
    QString m_scriptFile;
    QHash<QAction*, QScriptValue> m_shortcutCallbacks;
    QHash<int, QList<QScriptValue> > m_screenEdgeCallbacks;
    KConfigLoader *m_config;
    int m_chainPosition;
    QHash<int, QAction*> m_touchScreenEdgeCallbacks;
    Effect *m_activeFullScreenEffect = nullptr;
};

}

#endif // KWIN_SCRIPTEDEFFECT_H
