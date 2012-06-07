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
#include "scriptingutils.h"
#include "workspace_wrapper.h"
// KDE
#include <KDE/KConfigGroup>
#include <KDE/KDebug>
#include <KDE/KStandardDirs>
// Qt
#include <QtCore/QFile>
#include <QtScript/QScriptEngine>
#include <QtScript/QScriptValueIterator>

typedef KWin::EffectWindow* KEffectWindowRef;

Q_DECLARE_METATYPE(KWin::AnimationData*)
Q_SCRIPT_DECLARE_QMETAOBJECT(KWin::AnimationData, QObject*)

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

QScriptValue kwinEffectScriptAnimationTime(QScriptContext *context, QScriptEngine *engine)
{
    if (context->argumentCount() != 1) {
        return engine->undefinedValue();
    }
    if (!context->argument(0).isNumber()) {
        return engine->undefinedValue();
    }
    return Effect::animationTime(context->argument(0).toInteger());
}

QScriptValue kwinEffectDisplayWidth(QScriptContext *context, QScriptEngine *engine)
{
    Q_UNUSED(context)
    Q_UNUSED(engine)
    return Effect::displayWidth();
}

QScriptValue kwinEffectDisplayHeight(QScriptContext *context, QScriptEngine *engine)
{
    Q_UNUSED(context)
    Q_UNUSED(engine)
    return Effect::displayHeight();
}

QScriptValue kwinScriptGlobalShortcut(QScriptContext *context, QScriptEngine *engine)
{
    return globalShortcut<KWin::ScriptedEffect*>(context, engine);
}

QScriptValue kwinScriptScreenEdge(QScriptContext *context, QScriptEngine *engine)
{
    return registerScreenEdge<KWin::ScriptedEffect*>(context, engine);
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

QScriptValue fpx2ToScriptValue(QScriptEngine *eng, const KWin::FPx2 &fpx2)
{
    QScriptValue val = eng->newObject();
    val.setProperty("value1", fpx2[0]);
    val.setProperty("value2", fpx2[1]);
    return val;
}

void fpx2FromScriptValue(const QScriptValue &value, KWin::FPx2 &fpx2)
{
    if (value.isNull()) {
        fpx2 = FPx2();
        return;
    }
    if (value.isNumber()) {
        fpx2 = FPx2(value.toNumber());
        return;
    }
    if (value.isObject()) {
        QScriptValue value1 = value.property("value1");
        QScriptValue value2 = value.property("value2");
        if (!value1.isValid() || !value2.isValid() || !value1.isNumber() || !value2.isNumber()) {
            kDebug(1212) << "Cannot cast scripted FPx2 to C++";
            fpx2 = FPx2();
            return;
        }
        fpx2 = FPx2(value1.toNumber(), value2.toNumber());
    }
}

ScriptedEffect *ScriptedEffect::create(const QString& effectName, const QString& pathToScript)
{
    ScriptedEffect *effect = new ScriptedEffect();
    if (!effect->init(effectName, pathToScript)) {
        delete effect;
        return NULL;
    }
    return effect;
}

ScriptedEffect::ScriptedEffect()
    : AnimationEffect()
    , m_engine(new QScriptEngine(this))
    , m_scriptFile(QString())
{
    connect(m_engine, SIGNAL(signalHandlerException(QScriptValue)), SLOT(signalHandlerException(QScriptValue)));
#ifdef KWIN_BUILD_SCREENEDGES
    connect(Workspace::self()->screenEdge(), SIGNAL(activated(ElectricBorder)), SLOT(slotBorderActivated(ElectricBorder)));
#endif
}

ScriptedEffect::~ScriptedEffect()
{
#ifdef KWIN_BUILD_SCREENEDGES
    for (QHash<int, QList<QScriptValue> >::const_iterator it = m_screenEdgeCallbacks.constBegin();
            it != m_screenEdgeCallbacks.constEnd();
            ++it) {
        KWin::Workspace::self()->screenEdge()->unreserve(static_cast<KWin::ElectricBorder>(it.key()));
    }
#endif
}

bool ScriptedEffect::init(const QString &effectName, const QString &pathToScript)
{
    QFile scriptFile(pathToScript);
    if (!scriptFile.open(QIODevice::ReadOnly)) {
        kDebug(1212) << "Could not open script file: " << pathToScript;
        return false;
    }
    m_effectName = effectName;
    m_scriptFile = pathToScript;

    QScriptValue effectsObject = m_engine->newQObject(effects, QScriptEngine::QtOwnership, QScriptEngine::ExcludeDeleteLater);
    m_engine->globalObject().setProperty("effects", effectsObject, QScriptValue::Undeletable);
    m_engine->globalObject().setProperty("Effect", m_engine->newQMetaObject(&ScriptedEffect::staticMetaObject));
    m_engine->globalObject().setProperty("KWin", m_engine->newQMetaObject(&WorkspaceWrapper::staticMetaObject));
    m_engine->globalObject().setProperty("effect", m_engine->newQObject(this, QScriptEngine::QtOwnership, QScriptEngine::ExcludeDeleteLater), QScriptValue::Undeletable);
    m_engine->globalObject().setProperty("AnimationData", m_engine->scriptValueFromQMetaObject<AnimationData>());
    MetaScripting::registration(m_engine);
    qScriptRegisterMetaType<KEffectWindowRef>(m_engine, effectWindowToScriptValue, effectWindowFromScriptValue);
    qScriptRegisterMetaType<KWin::FPx2>(m_engine, fpx2ToScriptValue, fpx2FromScriptValue);
    qScriptRegisterSequenceMetaType<QList< KWin::EffectWindow* > >(m_engine);
    // add our print
    QScriptValue printFunc = m_engine->newFunction(kwinEffectScriptPrint);
    printFunc.setData(m_engine->newQObject(this));
    m_engine->globalObject().setProperty("print", printFunc);
    // add our animationTime
    QScriptValue animationTimeFunc = m_engine->newFunction(kwinEffectScriptAnimationTime);
    animationTimeFunc.setData(m_engine->newQObject(this));
    m_engine->globalObject().setProperty("animationTime", animationTimeFunc);
    // add displayWidth and displayHeight
    QScriptValue displayWidthFunc = m_engine->newFunction(kwinEffectDisplayWidth);
    m_engine->globalObject().setProperty("displayWidth", displayWidthFunc);
    QScriptValue displayHeightFunc = m_engine->newFunction(kwinEffectDisplayHeight);
    m_engine->globalObject().setProperty("displayHeight", displayHeightFunc);
    // add global Shortcut
    registerGlobalShortcutFunction(this, m_engine, kwinScriptGlobalShortcut);
    registerScreenEdgeFunction(this, m_engine, kwinScriptScreenEdge);

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

void ScriptedEffect::animate(KWin::EffectWindow* w, KWin::AnimationEffect::Attribute a, int ms, KWin::FPx2 to, KWin::FPx2 from, KWin::AnimationData* data, QEasingCurve curve, int delay)
{
    uint meta = 0;
    if (data) {
        if (data->axis() != 0) {
            AnimationEffect::setMetaData(AnimationEffect::Axis, data->axis() -1, meta);
        }
        if (data->sourceAnchor() != 0) {
            AnimationEffect::setMetaData(AnimationEffect::SourceAnchor, data->sourceAnchor(), meta);
        }
        if (data->targetAnchor() != 0) {
            AnimationEffect::setMetaData(AnimationEffect::TargetAnchor, data->targetAnchor(), meta);
        }
        if (data->relativeSourceX() != 0) {
            AnimationEffect::setMetaData(AnimationEffect::RelativeSourceX, data->relativeSourceX(), meta);
        }
        if (data->relativeSourceY() != 0) {
            AnimationEffect::setMetaData(AnimationEffect::RelativeSourceY, data->relativeSourceY(), meta);
        }
        if (data->relativeTargetX() != 0) {
            AnimationEffect::setMetaData(AnimationEffect::RelativeTargetX, data->relativeTargetX(), meta);
        }
        if (data->relativeTargetY() != 0) {
            AnimationEffect::setMetaData(AnimationEffect::RelativeTargetY, data->relativeTargetY(), meta);
        }
    }
    AnimationEffect::animate(w, a, meta, ms, to, curve, delay, from);
}

bool ScriptedEffect::isGrabbed(EffectWindow* w, ScriptedEffect::DataRole grabRole)
{
    void *e = w->data(static_cast<KWin::DataRole>(grabRole)).value<void*>();
    if (e) {
        return e != this;
    } else {
        return false;
    }
}

void ScriptedEffect::reconfigure(ReconfigureFlags flags)
{
    AnimationEffect::reconfigure(flags);
    emit configChanged();
}

void ScriptedEffect::registerShortcut(QAction *a, QScriptValue callback)
{
    m_shortcutCallbacks.insert(a, callback);
    connect(a, SIGNAL(triggered(bool)), SLOT(globalShortcutTriggered()));
}

void ScriptedEffect::globalShortcutTriggered()
{
    callGlobalShortcutCallback<KWin::ScriptedEffect*>(this, sender());
}

void ScriptedEffect::slotBorderActivated(ElectricBorder edge)
{
    screenEdgeActivated(this, edge);
}

QVariant ScriptedEffect::readConfig(const QString &key, const QVariant defaultValue)
{
    KConfigGroup cg = effects->effectConfig(m_effectName);
    return cg.readEntry(key, defaultValue);
}

AnimationData::AnimationData (QObject* parent)
    : QObject (parent)
    , m_sourceAnchor((AnimationEffect::Anchor)0)
    , m_targetAnchor((AnimationEffect::Anchor)0)
    , m_relativeSourceX(0)
    , m_relativeSourceY(0)
    , m_relativeTargetX(0)
    , m_relativeTargetY(0)
    , m_axis((AnimationData::Axis)0)
{
}

AnimationData::Axis AnimationData::axis() const
{
    return m_axis;
}

int AnimationData::relativeSourceX() const
{
    return m_relativeSourceX;
}

int AnimationData::relativeSourceY() const
{
    return m_relativeSourceY;
}

int AnimationData::relativeTargetX() const
{
    return m_relativeTargetX;
}

int AnimationData::relativeTargetY() const
{
    return m_relativeTargetY;
}

void AnimationData::setRelativeSourceX(int relativeSourceX)
{
    m_relativeSourceX = relativeSourceX;
}

void AnimationData::setRelativeSourceY(int relativeSourceY)
{
    m_relativeSourceY = relativeSourceY;
}

void AnimationData::setRelativeTargetX(int relativeTargetX)
{
    m_relativeTargetX = relativeTargetX;
}

void AnimationData::setRelativeTargetY(int relativeTargetY)
{
    m_relativeTargetY = relativeTargetY;
}

void AnimationData::setAxis(AnimationData::Axis axis)
{
    m_axis = axis;
}

void AnimationData::setSourceAnchor(AnimationEffect::Anchor sourceAnchor)
{
    m_sourceAnchor = sourceAnchor;
}

void AnimationData::setTargetAnchor(AnimationEffect::Anchor targetAnchor)
{
    m_targetAnchor = targetAnchor;
}

AnimationEffect::Anchor AnimationData::sourceAnchor() const
{
    return m_sourceAnchor;
}

AnimationEffect::Anchor AnimationData::targetAnchor() const
{
    return m_targetAnchor;
}

} // namespace
