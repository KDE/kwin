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
#ifdef KWIN_BUILD_SCREENEDGES
#include "../screenedge.h"
#endif
// KDE
#include <KDE/KConfigGroup>
#include <KDE/KDebug>
#include <KDE/KStandardDirs>
#include <Plasma/ConfigLoader>
// Qt
#include <QFile>
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

struct AnimationSettings {
    enum { Type = 1<<0, Curve = 1<<1, Delay = 1<<2, Duration = 1<<3 };
    AnimationEffect::Attribute type;
    QEasingCurve::Type curve;
    FPx2 from;
    FPx2 to;
    int delay;
    uint duration;
    uint set;
};

AnimationSettings animationSettingsFromObject(QScriptValue &object)
{
    AnimationSettings settings;
    settings.set = 0;

    settings.to = qscriptvalue_cast<FPx2>(object.property("to"));
    settings.from = qscriptvalue_cast<FPx2>(object.property("from"));

    QScriptValue duration = object.property("duration");
    if (duration.isValid() && duration.isNumber()) {
        settings.duration = duration.toUInt32();
        settings.set |= AnimationSettings::Duration;
    } else {
        settings.duration = 0;
    }

    QScriptValue delay = object.property("delay");
    if (delay.isValid() && delay.isNumber()) {
        settings.delay = delay.toInt32();
        settings.set |= AnimationSettings::Delay;
    } else {
        settings.delay = 0;
    }

    QScriptValue curve = object.property("curve");
    if (curve.isValid() && curve.isNumber()) {
        settings.curve = static_cast<QEasingCurve::Type>(curve.toInt32());
        settings.set |= AnimationSettings::Curve;
    } else {
        settings.curve = QEasingCurve::Linear;
    }

    QScriptValue type = object.property("type");
    if (type.isValid() && type.isNumber()) {
        settings.type = static_cast<AnimationEffect::Attribute>(type.toInt32());
        settings.set |= AnimationSettings::Type;
    } else {
        settings.type = static_cast<AnimationEffect::Attribute>(-1);
    }

    return settings;
}

QList<AnimationSettings> animationSettings(QScriptContext *context, ScriptedEffect *effect, EffectWindow **window)
{
    QList<AnimationSettings> settings;
    if (!effect) {
        context->throwError(QScriptContext::ReferenceError, "Internal Scripted KWin Effect error");
        return settings;
    }
    if (context->argumentCount() != 1) {
        context->throwError(QScriptContext::SyntaxError, "Exactly one argument expected");
        return settings;
    }
    if (!context->argument(0).isObject()) {
        context->throwError(QScriptContext::TypeError, "Argument needs to be an object");
        return settings;
    }
    QScriptValue object = context->argument(0);
    QScriptValue windowProperty = object.property("window");
    if (!windowProperty.isValid() || !windowProperty.isObject()) {
        context->throwError(QScriptContext::TypeError, "Window property missing in animation options");
        return settings;
    }
    *window = qobject_cast<EffectWindow*>(windowProperty.toQObject());

    settings << animationSettingsFromObject(object); // global

    QScriptValue animations = object.property("animations"); // array
    if (animations.isValid()) {
        if (!animations.isArray()) {
            context->throwError(QScriptContext::TypeError, "Animations provided but not an array");
            settings.clear();
            return settings;
        }
        const int length = static_cast<int>(animations.property("length").toInteger());
        for (int i=0; i<length; ++i) {
            QScriptValue value = animations.property(QString::number(i));
            if (!value.isValid()) {
                continue;
            }
            if (value.isObject()) {
                AnimationSettings s = animationSettingsFromObject(value);
                const uint set = s.set | settings.at(0).set;
                // Catch show stoppers (incompletable animation)
                if (!(set & AnimationSettings::Type)) {
                    context->throwError(QScriptContext::TypeError, "Type property missing in animation options");
                    continue;
                }
                if (!(set & AnimationSettings::Duration)) {
                    context->throwError(QScriptContext::TypeError, "Duration property missing in animation options");
                    continue;
                }
                // Complete local animations from global settings
                if (!(s.set & AnimationSettings::Duration)) {
                    s.duration = settings.at(0).duration;
                }
                if (!(s.set & AnimationSettings::Curve)) {
                    s.curve = settings.at(0).curve;
                }
                if (!(s.set & AnimationSettings::Delay)) {
                    s.delay = settings.at(0).delay;
                }
                settings << s;
            }
        }
    }

    if (settings.count() == 1) {
        const uint set = settings.at(0).set;
        if (!(set & AnimationSettings::Type)) {
            context->throwError(QScriptContext::TypeError, "Type property missing in animation options");
            settings.clear();
        }
        if (!(set & AnimationSettings::Duration)) {
            context->throwError(QScriptContext::TypeError, "Duration property missing in animation options");
            settings.clear();
        }
    } else if (!(settings.at(0).set & AnimationSettings::Type)) { // invalid global
        settings.removeAt(0); // -> get rid of it, only used to complete the others
    }

    return settings;
}

QScriptValue kwinEffectAnimate(QScriptContext *context, QScriptEngine *engine)
{
    ScriptedEffect *effect = qobject_cast<ScriptedEffect*>(context->callee().data().toQObject());
    EffectWindow *window;
    QList<AnimationSettings> settings = animationSettings(context, effect, &window);
    if (settings.empty()) {
        context->throwError(QScriptContext::TypeError, "No animations provided");
        return engine->undefinedValue();
    }
    if (!window) {
        context->throwError(QScriptContext::TypeError, "Window property does not contain an EffectWindow");
        return engine->undefinedValue();
    }

    QList<QVariant> animIds;
    foreach (const AnimationSettings &setting, settings) {
        animIds << QVariant(effect->animate(window,
                                    setting.type,
                                    setting.duration,
                                    setting.to,
                                    setting.from,
                                    NULL,
                                    setting.curve,
                                    setting.delay));
    }
    return engine->newVariant(animIds);
}

QScriptValue kwinEffectSet(QScriptContext *context, QScriptEngine *engine)
{
    ScriptedEffect *effect = qobject_cast<ScriptedEffect*>(context->callee().data().toQObject());

    EffectWindow *window;
    QList<AnimationSettings> settings = animationSettings(context, effect, &window);
    if (settings.empty()) {
        context->throwError(QScriptContext::TypeError, "No animations provided");
        return engine->undefinedValue();
    }
    if (!window) {
        context->throwError(QScriptContext::TypeError, "Window property does not contain an EffectWindow");
        return engine->undefinedValue();
    }

    QList<QVariant> animIds;
    foreach (const AnimationSettings &setting, settings) {
        animIds << QVariant(effect->set(window,
                               setting.type,
                               setting.duration,
                               setting.to,
                               setting.from,
                               NULL,
                               setting.curve,
                               setting.delay));
    }

    return engine->newVariant(animIds);
}

QScriptValue kwinEffectCancel(QScriptContext *context, QScriptEngine *engine)
{
    ScriptedEffect *effect = qobject_cast<ScriptedEffect*>(context->callee().data().toQObject());
    if (context->argumentCount() != 1) {
        context->throwError(QScriptContext::SyntaxError, "Exactly one argument expected");
        return engine->undefinedValue();
    }
    QVariant v = context->argument(0).toVariant();
    QList<quint64> animIds;
    bool ok = false;
    if (v.isValid()) {
        quint64 animId = v.toULongLong(&ok);
        if (ok)
            animIds << animId;
    }
    if (!ok) { // may still be a variantlist of variants being quint64
        QList<QVariant> list = v.toList();
        if (!list.isEmpty()) {
            foreach (const QVariant &vv, list) {
                quint64 animId = vv.toULongLong(&ok);
                if (ok)
                    animIds << animId;
            }
            ok = !animIds.isEmpty();
        }
    }
    if (!ok) {
        context->throwError(QScriptContext::TypeError, "Argument needs to be one or several quint64");
        return engine->undefinedValue();
    }

    foreach (const quint64 &animId, animIds) {
        ok |= engine->newVariant(effect->cancel(animId)).toBool();
    }

    return engine->newVariant(ok);
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
    , m_config(NULL)
{
    connect(m_engine, SIGNAL(signalHandlerException(QScriptValue)), SLOT(signalHandlerException(QScriptValue)));
}

ScriptedEffect::~ScriptedEffect()
{
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

    // does the effect contain an KConfigXT file?
    const QString kconfigXTFile = KStandardDirs::locate("data", QLatin1String(KWIN_NAME) + "/effects/" + m_effectName + "/contents/config/main.xml");
    if (!kconfigXTFile.isNull()) {
        KConfigGroup cg = effects->effectConfig(m_effectName);
        QFile xmlFile(kconfigXTFile);
        m_config = new Plasma::ConfigLoader(&cg, &xmlFile, this);
        m_config->readConfig();
    }

    QScriptValue effectsObject = m_engine->newQObject(effects, QScriptEngine::QtOwnership, QScriptEngine::ExcludeDeleteLater);
    m_engine->globalObject().setProperty("effects", effectsObject, QScriptValue::Undeletable);
    m_engine->globalObject().setProperty("Effect", m_engine->newQMetaObject(&ScriptedEffect::staticMetaObject));
    m_engine->globalObject().setProperty("KWin", m_engine->newQMetaObject(&WorkspaceWrapper::staticMetaObject));
    m_engine->globalObject().setProperty("QEasingCurve", m_engine->newQMetaObject(&QEasingCurve::staticMetaObject));
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
    // add the animate method
    QScriptValue animateFunc = m_engine->newFunction(kwinEffectAnimate);
    animateFunc.setData(m_engine->newQObject(this));
    m_engine->globalObject().setProperty("animate", animateFunc);

    // and the set variant
    QScriptValue setFunc = m_engine->newFunction(kwinEffectSet);
    setFunc.setData(m_engine->newQObject(this));
    m_engine->globalObject().setProperty("set", setFunc);

    // cancel...
    QScriptValue cancelFunc = m_engine->newFunction(kwinEffectCancel);
    cancelFunc.setData(m_engine->newQObject(this));
    m_engine->globalObject().setProperty("cancel", cancelFunc);

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

uint metaFromData(KWin::AnimationData* data)
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
    return meta;
}

quint64 ScriptedEffect::animate(KWin::EffectWindow* w, KWin::AnimationEffect::Attribute a, int ms, KWin::FPx2 to, KWin::FPx2 from, KWin::AnimationData* data, QEasingCurve::Type curve, int delay)
{
    return AnimationEffect::animate(w, a, metaFromData(data), ms, to, QEasingCurve(curve), delay, from);
}

quint64 ScriptedEffect::set(KWin::EffectWindow* w, KWin::AnimationEffect::Attribute a, int ms, KWin::FPx2 to, KWin::FPx2 from, KWin::AnimationData* data, QEasingCurve::Type curve, int delay)
{
    return AnimationEffect::set(w, a, metaFromData(data), ms, to, QEasingCurve(curve), delay, from);
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
    if (m_config) {
        m_config->readConfig();
    }
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

bool ScriptedEffect::borderActivated(ElectricBorder edge)
{
    screenEdgeActivated(this, edge);
    return true;
}

QVariant ScriptedEffect::readConfig(const QString &key, const QVariant defaultValue)
{
    if (!m_config) {
        return defaultValue;
    }
    return m_config->property(key);
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
