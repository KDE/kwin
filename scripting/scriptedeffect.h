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
public:
    const QString &scriptFile() const {
        return m_scriptFile;
    }
    static ScriptedEffect *create(const QString &pathToScript);

public Q_SLOTS:
    void animate(KWin::EffectWindow *w, Attribute a, int ms, float to, uint meta = 0, QEasingCurve curve = QEasingCurve(), int delay = 0);
    void animate(KWin::EffectWindow *w, Attribute a, int ms, QPoint to, uint meta = 0, QEasingCurve curve = QEasingCurve(), int delay = 0);
    void animate(KWin::EffectWindow *w, Attribute a, int ms, QPointF to, uint meta = 0, QEasingCurve curve = QEasingCurve(), int delay = 0);
    void animate(KWin::EffectWindow *w, Attribute a, int ms, QSize to, uint meta = 0, QEasingCurve curve = QEasingCurve(), int delay = 0);
    void animate(KWin::EffectWindow *w, Attribute a, int ms, QSizeF to, uint meta = 0, QEasingCurve curve = QEasingCurve(), int delay = 0);
    void animate(KWin::EffectWindow *w, Attribute a, int ms, float to, float from, uint meta = 0, QEasingCurve curve = QEasingCurve(), int delay = 0);
    void animate(KWin::EffectWindow *w, Attribute a, int ms, float to, float to2, float from, float from2, uint meta = 0, QEasingCurve curve = QEasingCurve(), int delay = 0);
    void animate(KWin::EffectWindow *w, Attribute a, int ms, QPoint to, QPoint from, uint meta = 0, QEasingCurve curve = QEasingCurve(), int delay = 0);
    void animate(KWin::EffectWindow *w, Attribute a, int ms, QPointF to, QPointF from, uint meta = 0, QEasingCurve curve = QEasingCurve(), int delay = 0);
    void animate(KWin::EffectWindow *w, Attribute a, int ms, QSize to, QSize from, uint meta = 0, QEasingCurve curve = QEasingCurve(), int delay = 0);
    void animate(KWin::EffectWindow *w, Attribute a, int ms, QSizeF to, QSizeF from, uint meta = 0, QEasingCurve curve = QEasingCurve(), int delay = 0);

private Q_SLOTS:
    void signalHandlerException(const QScriptValue &value);
private:
    ScriptedEffect();
    bool init(const QString &pathToScript);
    QScriptEngine *m_engine;
    QString m_scriptFile;
};

}

#endif // KWIN_SCRIPTEDEFFECT_H
