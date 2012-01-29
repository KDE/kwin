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

#include "scriptedeffect.h"
#include "meta.h"
// KDE
#include <KDE/KDebug>
// Qt
#include <QtCore/QFile>
#include <QtScript/QScriptEngine>
#include <QtScript/QScriptValueIterator>

typedef KWin::EffectWindow* KEffectWindowRef;
namespace KWin
{

QScriptValue kwinEffectScriptPrint(QScriptContext *context, QScriptEngine *engine)
{
    ScriptedEffect *script = qobject_cast<ScriptedEffect*>(context->callee().data().toQObject());
    QString result;
    for (int i = 0; i < context->argumentCount(); ++i) {
        if (i > 0) {
            result.append(" ");
        }
        result.append(context->argument(i).toString());
    }
    kDebug(1212) << script->scriptFile() << ":" << result;

    return engine->undefinedValue();
}

QScriptValue effectWindowToScriptValue(QScriptEngine *eng, const KEffectWindowRef &window)
{
    return eng->newQObject(window, QScriptEngine::QtOwnership,
                           QScriptEngine::ExcludeChildObjects | QScriptEngine::ExcludeDeleteLater | QScriptEngine::PreferExistingWrapperObject);
}

void effectWindowFromScriptValue(const QScriptValue &value, EffectWindow* &window)
{
    window = qobject_cast<EffectWindow*>(value.toQObject());
}

ScriptedEffect *ScriptedEffect::create(const QString &pathToScript)
{
    ScriptedEffect *effect = new ScriptedEffect();
    if (!effect->init(pathToScript)) {
        delete effect;
        return NULL;
    }
    return effect;
}

ScriptedEffect::ScriptedEffect()
    : AnimationEffect()
    , m_engine(new QScriptEngine(this))
{
    connect(m_engine, SIGNAL(signalHandlerException(QScriptValue)), SLOT(signalHandlerException(QScriptValue)));
}

bool ScriptedEffect::init(const QString &pathToScript)
{
    QFile scriptFile(pathToScript);
    if (!scriptFile.open(QIODevice::ReadOnly)) {
        return false;
    }
    m_scriptFile = pathToScript;
    QScriptValue effectsObject = m_engine->newQObject(effects, QScriptEngine::QtOwnership, QScriptEngine::ExcludeDeleteLater);
    m_engine->globalObject().setProperty("effects", effectsObject, QScriptValue::Undeletable);
    m_engine->globalObject().setProperty("Effect", m_engine->newQMetaObject(&AnimationEffect::staticMetaObject));
    m_engine->globalObject().setProperty("effect", m_engine->newQObject(this, QScriptEngine::QtOwnership, QScriptEngine::ExcludeDeleteLater), QScriptValue::Undeletable);
    MetaScripting::registration(m_engine);
    qScriptRegisterMetaType<KEffectWindowRef>(m_engine, effectWindowToScriptValue, effectWindowFromScriptValue);
    // add our print
    QScriptValue printFunc = m_engine->newFunction(kwinEffectScriptPrint);
    printFunc.setData(m_engine->newQObject(this));
    m_engine->globalObject().setProperty("print", printFunc);

    QScriptValue ret = m_engine->evaluate(scriptFile.readAll());

    if (ret.isError()) {
        signalHandlerException(ret);
        return false;
    }
    scriptFile.close();
    return true;
}

void ScriptedEffect::signalHandlerException(const QScriptValue &value)
{
    if (value.isError()) {
        kDebug(1212) << "KWin Effect script encountered an error at [Line " << m_engine->uncaughtExceptionLineNumber() << "]";
        kDebug(1212) << "Message: " << value.toString();

        QScriptValueIterator iter(value);
        while (iter.hasNext()) {
            iter.next();
            kDebug(1212) << " " << iter.name() << ": " << iter.value().toString();
        }
    }
}

void ScriptedEffect::animate(EffectWindow *w, Attribute a, int ms, float to, uint meta, QEasingCurve curve, int delay)
{
    AnimationEffect::animate(w, a, meta, ms, FPx2(to), curve, delay);
}

void ScriptedEffect::animate(EffectWindow *w, Attribute a, int ms, QPoint to, uint meta, QEasingCurve curve, int delay)
{
    AnimationEffect::animate(w, a, meta, ms, FPx2(to), curve, delay);
}

void ScriptedEffect::animate(EffectWindow *w, Attribute a, int ms, QPointF to, uint meta, QEasingCurve curve, int delay)
{
    AnimationEffect::animate(w, a, meta, ms, FPx2(to), curve, delay);
}

void ScriptedEffect::animate(EffectWindow *w, Attribute a, int ms, QSize to, uint meta, QEasingCurve curve, int delay)
{
    AnimationEffect::animate(w, a, meta, ms, FPx2(to), curve, delay);
}

void ScriptedEffect::animate(EffectWindow *w, Attribute a, int ms, QSizeF to, uint meta, QEasingCurve curve, int delay)
{
    AnimationEffect::animate(w, a, meta, ms, FPx2(to), curve, delay);
}

void ScriptedEffect::animate(EffectWindow *w, Attribute a, int ms, float to, float from, uint meta, QEasingCurve curve, int delay)
{
    AnimationEffect::animate(w, a, meta, ms, FPx2(to), curve, delay, FPx2(from));
}

void ScriptedEffect::animate(EffectWindow *w, Attribute a, int ms, float to, float to2, float from, float from2, uint meta, QEasingCurve curve, int delay)
{
    AnimationEffect::animate(w, a, meta, ms, FPx2(to, to2), curve, delay, FPx2(from, from2));
}

void ScriptedEffect::animate(EffectWindow *w, Attribute a, int ms, QPoint to, QPoint from, uint meta, QEasingCurve curve, int delay)
{
    AnimationEffect::animate(w, a, meta, ms, FPx2(to), curve, delay, FPx2(from));
}

void ScriptedEffect::animate(EffectWindow *w, Attribute a, int ms, QPointF to, QPointF from, uint meta, QEasingCurve curve, int delay)
{
    AnimationEffect::animate(w, a, meta, ms, FPx2(to), curve, delay, FPx2(from));
}

void ScriptedEffect::animate(EffectWindow *w, Attribute a, int ms, QSize to, QSize from, uint meta, QEasingCurve curve, int delay)
{
    AnimationEffect::animate(w, a, meta, ms, FPx2(to), curve, delay, FPx2(from));
}

void ScriptedEffect::animate(EffectWindow *w, Attribute a, int ms, QSizeF to, QSizeF from, uint meta, QEasingCurve curve, int delay)
{
    AnimationEffect::animate(w, a, meta, ms, FPx2(to), curve, delay, FPx2(from));
}

} // namespace
