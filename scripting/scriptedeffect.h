/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

 Copyright (C) 2012 Martin Gräßlin <mgraesslin@kde.org>

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

#ifndef KWIN_SCRIPTEDEFFECT_H
#define KWIN_SCRIPTEDEFFECT_H

#include <kwinanimationeffect.h>

class QScriptEngine;
class QScriptValue;

namespace KWin
{
class ScriptedEffect;
class AnimationData : public QObject
{
    Q_OBJECT
    Q_ENUMS(Axis)
    Q_PROPERTY(KWin::AnimationEffect::Anchor sourceAnchor READ sourceAnchor WRITE setSourceAnchor)
    Q_PROPERTY(KWin::AnimationEffect::Anchor targetAnchor READ targetAnchor WRITE setTargetAnchor)
    Q_PROPERTY(int relativeSourceX READ relativeSourceX WRITE setRelativeSourceX)
    Q_PROPERTY(int relativeSourceY READ relativeSourceY WRITE setRelativeSourceY)
    Q_PROPERTY(int relativeTargetX READ relativeTargetX WRITE setRelativeTargetX)
    Q_PROPERTY(int relativeTargetY READ relativeTargetY WRITE setRelativeTargetY)
    Q_PROPERTY(Axis axis READ axis WRITE setAxis)
public:
    enum Axis {
        XAxis = 1,
        YAxis,
        ZAxis
    };
    explicit AnimationData(QObject* parent = 0);

    // getter
    AnimationEffect::Anchor sourceAnchor() const;
    AnimationEffect::Anchor targetAnchor() const;
    int relativeSourceX() const;
    int relativeSourceY() const;
    int relativeTargetX() const;
    int relativeTargetY() const;
    Axis axis() const;

    // setter
    void setSourceAnchor(AnimationEffect::Anchor sourceAnchor);
    void setTargetAnchor(AnimationEffect::Anchor targetAnchor);
    void setRelativeSourceX(int relativeSourceX);
    void setRelativeSourceY(int relativeSourceY);
    void setRelativeTargetX(int relativeTargetX);
    void setRelativeTargetY(int relativeTargetY);
    void setAxis(Axis axis);

private:
    AnimationEffect::Anchor m_sourceAnchor;
    AnimationEffect::Anchor m_targetAnchor;
    int m_relativeSourceX;
    int m_relativeSourceY;
    int m_relativeTargetX;
    int m_relativeTargetY;
    Axis m_axis;
};

class ScriptedEffect : public KWin::AnimationEffect
{
    Q_OBJECT
    Q_ENUMS(DataRole)
    Q_ENUMS(KWin::AnimationData::Axis)
    Q_ENUMS(Anchor)
    Q_ENUMS(MetaType)
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
        LanczosCacheRole
    };
    const QString &scriptFile() const {
        return m_scriptFile;
    }
    virtual void reconfigure(ReconfigureFlags flags);
    QString activeConfig() const;
    void setActiveConfig(const QString &name);
    static ScriptedEffect *create(const QString &effectName, const QString &pathToScript);
    virtual ~ScriptedEffect();
    /**
     * Whether another effect has grabbed the @p w with the given @p grabRole.
     * @param w The window to check
     * @param grabRole The grab role to check
     * @returns @c true if another window has grabbed the effect, @c false otherwise
     **/
    Q_SCRIPTABLE bool isGrabbed(KWin::EffectWindow *w, DataRole grabRole);
    /**
     * Reads the value from the configuration data for the given key.
     * @param key The key to search for
     * @param defaultValue The value to return if the key is not found
     * @returns The config value if present
     **/
    Q_SCRIPTABLE QVariant readConfig(const QString &key, const QVariant defaultValue = QVariant());
    void registerShortcut(QAction *a, QScriptValue callback);
    const QHash<QAction*, QScriptValue> &shortcutCallbacks() const {
        return m_shortcutCallbacks;
    }
    QHash<int, QList<QScriptValue > > &screenEdgeCallbacks() {
        return m_screenEdgeCallbacks;
    }

public Q_SLOTS:
    void animate(KWin::EffectWindow *w, Attribute a, int ms, KWin::FPx2 to, KWin::FPx2 from = KWin::FPx2(), KWin::AnimationData *data = NULL, QEasingCurve curve = QEasingCurve(), int delay = 0);

Q_SIGNALS:
    /**
     * Signal emitted whenever the effect's config changed.
     **/
    void configChanged();

private Q_SLOTS:
    void signalHandlerException(const QScriptValue &value);
    void globalShortcutTriggered();
    void slotBorderActivated(ElectricBorder edge);
private:
    ScriptedEffect();
    bool init(const QString &effectName, const QString &pathToScript);
    QScriptEngine *m_engine;
    QString m_effectName;
    QString m_scriptFile;
    QHash<QAction*, QScriptValue> m_shortcutCallbacks;
    QHash<int, QList<QScriptValue> > m_screenEdgeCallbacks;
};

}

#endif // KWIN_SCRIPTEDEFFECT_H
