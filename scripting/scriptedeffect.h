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

class ScriptedEffect : public KWin::AnimationEffect
{
    Q_OBJECT
    Q_ENUMS(DataRole)
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

public Q_SLOTS:
    void animate(KWin::EffectWindow *w, Attribute a, int ms, KWin::FPx2 to, KWin::FPx2 from = KWin::FPx2(), uint meta = 0, QEasingCurve curve = QEasingCurve(), int delay = 0);

Q_SIGNALS:
    /**
     * Signal emitted whenever the effect's config changed.
     **/
    void configChanged();

private Q_SLOTS:
    void signalHandlerException(const QScriptValue &value);
private:
    ScriptedEffect();
    bool init(const QString &effectName, const QString &pathToScript);
    QScriptEngine *m_engine;
    QString m_effectName;
    QString m_scriptFile;
};

}

#endif // KWIN_SCRIPTEDEFFECT_H
