/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2012 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "scriptedeffect.h"
#include "meta.h"
#include "scriptingutils.h"
#include "workspace_wrapper.h"
#include "../screens.h"
#include "../screenedge.h"
#include "scripting_logging.h"
// KDE
#include <KConfigGroup>
#include <kconfigloader.h>
#include <KPluginMetaData>
// Qt
#include <QFile>
#include <QtScript/QScriptEngine>
#include <QtScript/QScriptValueIterator>
#include <QStandardPaths>

typedef KWin::EffectWindow* KEffectWindowRef;

Q_DECLARE_METATYPE(KSharedConfigPtr)

namespace KWin
{

QScriptValue kwinEffectScriptPrint(QScriptContext *context, QScriptEngine *engine)
{
    ScriptedEffect *script = qobject_cast<ScriptedEffect*>(context->callee().data().toQObject());
    QString result;
    for (int i = 0; i < context->argumentCount(); ++i) {
        if (i > 0) {
            result.append(QLatin1Char(' '));
        }
        result.append(context->argument(i).toString());
    }
    qCDebug(KWIN_SCRIPTING) << script->scriptFile() << ":" << result;

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
    return screens()->displaySize().width();
}

QScriptValue kwinEffectDisplayHeight(QScriptContext *context, QScriptEngine *engine)
{
    Q_UNUSED(context)
    Q_UNUSED(engine)
    return screens()->displaySize().height();
}

QScriptValue kwinScriptGlobalShortcut(QScriptContext *context, QScriptEngine *engine)
{
    return globalShortcut<KWin::ScriptedEffect*>(context, engine);
}

QScriptValue kwinScriptScreenEdge(QScriptContext *context, QScriptEngine *engine)
{
    return registerScreenEdge<KWin::ScriptedEffect*>(context, engine);
}

QScriptValue kwinRegisterTouchScreenEdge(QScriptContext *context, QScriptEngine *engine)
{
    return registerTouchScreenEdge<KWin::ScriptedEffect*>(context, engine);
}

QScriptValue kwinUnregisterTouchScreenEdge(QScriptContext *context, QScriptEngine *engine)
{
    return unregisterTouchScreenEdge<KWin::ScriptedEffect*>(context, engine);
}

struct AnimationSettings {
    enum {
        Type       = 1<<0,
        Curve      = 1<<1,
        Delay      = 1<<2,
        Duration   = 1<<3,
        FullScreen = 1<<4,
        KeepAlive  = 1<<5
    };
    AnimationEffect::Attribute type;
    QEasingCurve::Type curve;
    FPx2 from;
    FPx2 to;
    int delay;
    uint duration;
    uint set;
    uint metaData;
    bool fullScreenEffect;
    bool keepAlive;
};

AnimationSettings animationSettingsFromObject(QScriptValue &object)
{
    AnimationSettings settings;
    settings.set = 0;
    settings.metaData = 0;

    settings.to = qscriptvalue_cast<FPx2>(object.property(QStringLiteral("to")));
    settings.from = qscriptvalue_cast<FPx2>(object.property(QStringLiteral("from")));

    QScriptValue duration = object.property(QStringLiteral("duration"));
    if (duration.isValid() && duration.isNumber()) {
        settings.duration = duration.toUInt32();
        settings.set |= AnimationSettings::Duration;
    } else {
        settings.duration = 0;
    }

    QScriptValue delay = object.property(QStringLiteral("delay"));
    if (delay.isValid() && delay.isNumber()) {
        settings.delay = delay.toInt32();
        settings.set |= AnimationSettings::Delay;
    } else {
        settings.delay = 0;
    }

    QScriptValue curve = object.property(QStringLiteral("curve"));
    if (curve.isValid() && curve.isNumber()) {
        settings.curve = static_cast<QEasingCurve::Type>(curve.toInt32());
        settings.set |= AnimationSettings::Curve;
    } else {
        settings.curve = QEasingCurve::Linear;
    }

    QScriptValue type = object.property(QStringLiteral("type"));
    if (type.isValid() && type.isNumber()) {
        settings.type = static_cast<AnimationEffect::Attribute>(type.toInt32());
        settings.set |= AnimationSettings::Type;
    } else {
        settings.type = static_cast<AnimationEffect::Attribute>(-1);
    }

    QScriptValue isFullScreen = object.property(QStringLiteral("fullScreen"));
    if (isFullScreen.isValid() && isFullScreen.isBool()) {
        settings.fullScreenEffect = isFullScreen.toBool();
        settings.set |= AnimationSettings::FullScreen;
    } else {
        settings.fullScreenEffect = false;
    }

    QScriptValue keepAlive = object.property(QStringLiteral("keepAlive"));
    if (keepAlive.isValid() && keepAlive.isBool()) {
        settings.keepAlive = keepAlive.toBool();
        settings.set |= AnimationSettings::KeepAlive;
    } else {
        settings.keepAlive = true;
    }

    return settings;
}

QList<AnimationSettings> animationSettings(QScriptContext *context, ScriptedEffect *effect, EffectWindow **window)
{
    QList<AnimationSettings> settings;
    if (!effect) {
        context->throwError(QScriptContext::ReferenceError, QStringLiteral("Internal Scripted KWin Effect error"));
        return settings;
    }
    if (context->argumentCount() != 1) {
        context->throwError(QScriptContext::SyntaxError, QStringLiteral("Exactly one argument expected"));
        return settings;
    }
    if (!context->argument(0).isObject()) {
        context->throwError(QScriptContext::TypeError, QStringLiteral("Argument needs to be an object"));
        return settings;
    }
    QScriptValue object = context->argument(0);
    QScriptValue windowProperty = object.property(QStringLiteral("window"));
    if (!windowProperty.isValid() || !windowProperty.isObject()) {
        context->throwError(QScriptContext::TypeError, QStringLiteral("Window property missing in animation options"));
        return settings;
    }
    *window = qobject_cast<EffectWindow*>(windowProperty.toQObject());

    settings << animationSettingsFromObject(object); // global

    QScriptValue animations = object.property(QStringLiteral("animations")); // array
    if (animations.isValid()) {
        if (!animations.isArray()) {
            context->throwError(QScriptContext::TypeError, QStringLiteral("Animations provided but not an array"));
            settings.clear();
            return settings;
        }
        const int length = static_cast<int>(animations.property(QStringLiteral("length")).toInteger());
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
                    context->throwError(QScriptContext::TypeError, QStringLiteral("Type property missing in animation options"));
                    continue;
                }
                if (!(set & AnimationSettings::Duration)) {
                    context->throwError(QScriptContext::TypeError, QStringLiteral("Duration property missing in animation options"));
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
                if (!(s.set & AnimationSettings::FullScreen)) {
                    s.fullScreenEffect = settings.at(0).fullScreenEffect;
                }
                if (!(s.set & AnimationSettings::KeepAlive)) {
                    s.keepAlive = settings.at(0).keepAlive;
                }

                s.metaData = 0;
                typedef QMap<AnimationEffect::MetaType, QString> MetaTypeMap;
                static MetaTypeMap metaTypes({
                    {AnimationEffect::SourceAnchor, QStringLiteral("sourceAnchor")},
                    {AnimationEffect::TargetAnchor, QStringLiteral("targetAnchor")},
                    {AnimationEffect::RelativeSourceX, QStringLiteral("relativeSourceX")},
                    {AnimationEffect::RelativeSourceY, QStringLiteral("relativeSourceY")},
                    {AnimationEffect::RelativeTargetX, QStringLiteral("relativeTargetX")},
                    {AnimationEffect::RelativeTargetY, QStringLiteral("relativeTargetY")},
                    {AnimationEffect::Axis, QStringLiteral("axis")}
                });

                for (MetaTypeMap::const_iterator it = metaTypes.constBegin(),
                                                end = metaTypes.constEnd(); it != end; ++it) {
                    QScriptValue metaVal = value.property(*it);
                    if (metaVal.isValid() && metaVal.isNumber()) {
                        AnimationEffect::setMetaData(it.key(), metaVal.toInt32(), s.metaData);
                    }
                }

                settings << s;
            }
        }
    }

    if (settings.count() == 1) {
        const uint set = settings.at(0).set;
        if (!(set & AnimationSettings::Type)) {
            context->throwError(QScriptContext::TypeError, QStringLiteral("Type property missing in animation options"));
            settings.clear();
        }
        if (!(set & AnimationSettings::Duration)) {
            context->throwError(QScriptContext::TypeError, QStringLiteral("Duration property missing in animation options"));
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
        context->throwError(QScriptContext::TypeError, QStringLiteral("No animations provided"));
        return engine->undefinedValue();
    }
    if (!window) {
        context->throwError(QScriptContext::TypeError, QStringLiteral("Window property does not contain an EffectWindow"));
        return engine->undefinedValue();
    }

    QScriptValue array = engine->newArray(settings.length());
    int i = 0;
    foreach (const AnimationSettings &setting, settings) {
        array.setProperty(i, (uint)effect->animate(window,
                                    setting.type,
                                    setting.duration,
                                    setting.to,
                                    setting.from,
                                    setting.metaData,
                                    setting.curve,
                                    setting.delay,
                                    setting.fullScreenEffect,
                                    setting.keepAlive));
        ++i;
    }
    return array;
}

QScriptValue kwinEffectSet(QScriptContext *context, QScriptEngine *engine)
{
    ScriptedEffect *effect = qobject_cast<ScriptedEffect*>(context->callee().data().toQObject());

    EffectWindow *window;
    QList<AnimationSettings> settings = animationSettings(context, effect, &window);
    if (settings.empty()) {
        context->throwError(QScriptContext::TypeError, QStringLiteral("No animations provided"));
        return engine->undefinedValue();
    }
    if (!window) {
        context->throwError(QScriptContext::TypeError, QStringLiteral("Window property does not contain an EffectWindow"));
        return engine->undefinedValue();
    }

    QList<QVariant> animIds;
    foreach (const AnimationSettings &setting, settings) {
        animIds << QVariant(effect->set(window,
                               setting.type,
                               setting.duration,
                               setting.to,
                               setting.from,
                               setting.metaData,
                               setting.curve,
                               setting.delay,
                               setting.fullScreenEffect,
                               setting.keepAlive));
    }

    return engine->newVariant(animIds);
}

QList<quint64> animations(const QVariant &v, bool *ok)
{
    QList<quint64> animIds;
    *ok = false;
    if (v.isValid()) {
        quint64 animId = v.toULongLong(ok);
        if (*ok)
            animIds << animId;
    }
    if (!*ok) { // may still be a variantlist of variants being quint64
        QList<QVariant> list = v.toList();
        if (!list.isEmpty()) {
            foreach (const QVariant &vv, list) {
                quint64 animId = vv.toULongLong(ok);
                if (*ok)
                    animIds << animId;
            }
            *ok = !animIds.isEmpty();
        }
    }
    return animIds;
}

QScriptValue fpx2ToScriptValue(QScriptEngine *eng, const KWin::FPx2 &fpx2)
{
    QScriptValue val = eng->newObject();
    val.setProperty(QStringLiteral("value1"), fpx2[0]);
    val.setProperty(QStringLiteral("value2"), fpx2[1]);
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
        QScriptValue value1 = value.property(QStringLiteral("value1"));
        QScriptValue value2 = value.property(QStringLiteral("value2"));
        if (!value1.isValid() || !value2.isValid() || !value1.isNumber() || !value2.isNumber()) {
            qCDebug(KWIN_SCRIPTING) << "Cannot cast scripted FPx2 to C++";
            fpx2 = FPx2();
            return;
        }
        fpx2 = FPx2(value1.toNumber(), value2.toNumber());
    }
}

QScriptValue kwinEffectRetarget(QScriptContext *context, QScriptEngine *engine)
{
    ScriptedEffect *effect = qobject_cast<ScriptedEffect*>(context->callee().data().toQObject());
    if (context->argumentCount() < 2 || context->argumentCount() > 3) {
        context->throwError(QScriptContext::SyntaxError, QStringLiteral("2 or 3 arguments expected"));
        return engine->undefinedValue();
    }
    QVariant v = context->argument(0).toVariant();
    bool ok = false;
    QList<quint64> animIds = animations(v, &ok);
    if (!ok) {
        context->throwError(QScriptContext::TypeError, QStringLiteral("Argument needs to be one or several quint64"));
        return engine->undefinedValue();
    }
    FPx2 target;
    fpx2FromScriptValue(context->argument(1), target);

    ok = false;
    const int remainingTime = context->argumentCount() == 3 ? context->argument(2).toVariant().toInt() : -1;
    foreach (const quint64 &animId, animIds) {
        ok = effect->retarget(animId, target, remainingTime);
        if (!ok) {
            break;
        }
    }

    return QScriptValue(ok);
}

QScriptValue kwinEffectRedirect(QScriptContext *context, QScriptEngine *engine)
{
    if (context->argumentCount() != 2 && context->argumentCount() != 3) {
        const QString errorMessage = QStringLiteral("redirect() takes either 2 or 3 arguments (%1 given)")
            .arg(context->argumentCount());
        context->throwError(QScriptContext::SyntaxError, errorMessage);
        return engine->undefinedValue();
    }

    bool ok = false;
    QList<quint64> animationIds = animations(context->argument(0).toVariant(), &ok);
    if (!ok) {
        context->throwError(QScriptContext::TypeError, QStringLiteral("Argument needs to be one or several quint64"));
        return engine->undefinedValue();
    }

    const QScriptValue wrappedDirection = context->argument(1);
    if (!wrappedDirection.isNumber()) {
        context->throwError(QScriptContext::TypeError, QStringLiteral("Direction has invalid type"));
        return engine->undefinedValue();
    }

    const auto direction = static_cast<AnimationEffect::Direction>(wrappedDirection.toInt32());
    switch (direction) {
    case AnimationEffect::Forward:
    case AnimationEffect::Backward:
        break;

    default:
        context->throwError(QScriptContext::SyntaxError, QStringLiteral("Unknown direction"));
        return engine->undefinedValue();
    }

    AnimationEffect::TerminationFlags terminationFlags = AnimationEffect::TerminateAtSource;
    if (context->argumentCount() >= 3) {
        const QScriptValue wrappedTerminationFlags = context->argument(2);
        if (!wrappedTerminationFlags.isNumber()) {
            context->throwError(QScriptContext::TypeError, QStringLiteral("Termination flags argument has invalid type"));
            return engine->undefinedValue();
        }

        terminationFlags = static_cast<AnimationEffect::TerminationFlags>(wrappedTerminationFlags.toInt32());
    }

    ScriptedEffect *effect = qobject_cast<ScriptedEffect *>(context->callee().data().toQObject());
    for (const quint64 &animationId : qAsConst(animationIds)) {
        if (!effect->redirect(animationId, direction, terminationFlags)) {
            return QScriptValue(false);
        }
    }

    return QScriptValue(true);
}

QScriptValue kwinEffectComplete(QScriptContext *context, QScriptEngine *engine)
{
    if (context->argumentCount() != 1) {
        const QString errorMessage = QStringLiteral("complete() takes exactly 1 arguments (%1 given)")
            .arg(context->argumentCount());
        context->throwError(QScriptContext::SyntaxError, errorMessage);
        return engine->undefinedValue();
    }

    bool ok = false;
    QList<quint64> animationIds = animations(context->argument(0).toVariant(), &ok);
    if (!ok) {
        context->throwError(QScriptContext::TypeError, QStringLiteral("Argument needs to be one or several quint64"));
        return engine->undefinedValue();
    }

    ScriptedEffect *effect = qobject_cast<ScriptedEffect *>(context->callee().data().toQObject());
    for (const quint64 &animationId : qAsConst(animationIds)) {
        if (!effect->complete(animationId)) {
            return QScriptValue(false);
        }
    }

    return QScriptValue(true);
}

QScriptValue kwinEffectCancel(QScriptContext *context, QScriptEngine *engine)
{
    ScriptedEffect *effect = qobject_cast<ScriptedEffect*>(context->callee().data().toQObject());
    if (context->argumentCount() != 1) {
        context->throwError(QScriptContext::SyntaxError, QStringLiteral("Exactly one argument expected"));
        return engine->undefinedValue();
    }
    QVariant v = context->argument(0).toVariant();
    bool ok = false;
    QList<quint64> animIds = animations(v, &ok);
    if (!ok) {
        context->throwError(QScriptContext::TypeError, QStringLiteral("Argument needs to be one or several quint64"));
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

ScriptedEffect *ScriptedEffect::create(const KPluginMetaData &effect)
{
    const QString name = effect.pluginId();
    const QString scriptName = effect.value(QStringLiteral("X-Plasma-MainScript"));
    if (scriptName.isEmpty()) {
        qCDebug(KWIN_SCRIPTING) << "X-Plasma-MainScript not set";
        return nullptr;
    }
    const QString scriptFile = QStandardPaths::locate(QStandardPaths::GenericDataLocation,
                                                      QLatin1String(KWIN_NAME "/effects/") + name + QLatin1String("/contents/") + scriptName);
    if (scriptFile.isNull()) {
        qCDebug(KWIN_SCRIPTING) << "Could not locate the effect script";
        return nullptr;
    }
    return ScriptedEffect::create(name, scriptFile, effect.value(QStringLiteral("X-KDE-Ordering")).toInt());
}

ScriptedEffect *ScriptedEffect::create(const QString& effectName, const QString& pathToScript, int chainPosition)
{
    ScriptedEffect *effect = new ScriptedEffect();
    if (!effect->init(effectName, pathToScript)) {
        delete effect;
        return nullptr;
    }
    effect->m_chainPosition = chainPosition;
    return effect;
}

bool ScriptedEffect::supported()
{
    return effects->animationsSupported();
}

ScriptedEffect::ScriptedEffect()
    : AnimationEffect()
    , m_engine(new QScriptEngine(this))
    , m_scriptFile(QString())
    , m_config(nullptr)
    , m_chainPosition(0)
{
    Q_ASSERT(effects);
    connect(m_engine, SIGNAL(signalHandlerException(QScriptValue)), SLOT(signalHandlerException(QScriptValue)));
    connect(effects, &EffectsHandler::activeFullScreenEffectChanged, this, [this]() {
        Effect* fullScreenEffect = effects->activeFullScreenEffect();
        if (fullScreenEffect == m_activeFullScreenEffect) {
            return;
        }
        if (m_activeFullScreenEffect == this || fullScreenEffect == this) {
            emit isActiveFullScreenEffectChanged();
        }
        m_activeFullScreenEffect = fullScreenEffect;
    });
}

ScriptedEffect::~ScriptedEffect()
{
}

bool ScriptedEffect::init(const QString &effectName, const QString &pathToScript)
{
    QFile scriptFile(pathToScript);
    if (!scriptFile.open(QIODevice::ReadOnly)) {
        qCDebug(KWIN_SCRIPTING) << "Could not open script file: " << pathToScript;
        return false;
    }
    m_effectName = effectName;
    m_scriptFile = pathToScript;

    // does the effect contain an KConfigXT file?
    const QString kconfigXTFile = QStandardPaths::locate(QStandardPaths::GenericDataLocation, QLatin1String(KWIN_NAME "/effects/") + m_effectName + QLatin1String("/contents/config/main.xml"));
    if (!kconfigXTFile.isNull()) {
        KConfigGroup cg = QCoreApplication::instance()->property("config").value<KSharedConfigPtr>()->group(QStringLiteral("Effect-%1").arg(m_effectName));
        QFile xmlFile(kconfigXTFile);
        m_config = new KConfigLoader(cg, &xmlFile, this);
        m_config->load();
    }

    QScriptValue effectsObject = m_engine->newQObject(effects, QScriptEngine::QtOwnership, QScriptEngine::ExcludeDeleteLater);
    m_engine->globalObject().setProperty(QStringLiteral("effects"), effectsObject, QScriptValue::Undeletable);
    m_engine->globalObject().setProperty(QStringLiteral("Effect"), m_engine->newQMetaObject(&ScriptedEffect::staticMetaObject));
#ifndef KWIN_UNIT_TEST
    m_engine->globalObject().setProperty(QStringLiteral("KWin"), m_engine->newQMetaObject(&QtScriptWorkspaceWrapper::staticMetaObject));
#endif
    m_engine->globalObject().setProperty(QStringLiteral("Globals"), m_engine->newQMetaObject(&KWin::staticMetaObject));

    m_engine->globalObject().setProperty(QStringLiteral("QEasingCurve"), m_engine->newQMetaObject(&QEasingCurve::staticMetaObject));
    m_engine->globalObject().setProperty(QStringLiteral("effect"), m_engine->newQObject(this, QScriptEngine::QtOwnership, QScriptEngine::ExcludeDeleteLater), QScriptValue::Undeletable);
    MetaScripting::registration(m_engine);
    qScriptRegisterMetaType<KEffectWindowRef>(m_engine, effectWindowToScriptValue, effectWindowFromScriptValue);
    qScriptRegisterMetaType<KWin::FPx2>(m_engine, fpx2ToScriptValue, fpx2FromScriptValue);
    qScriptRegisterSequenceMetaType<QList< KWin::EffectWindow* > >(m_engine);
    // add our print
    QScriptValue printFunc = m_engine->newFunction(kwinEffectScriptPrint);
    printFunc.setData(m_engine->newQObject(this));
    m_engine->globalObject().setProperty(QStringLiteral("print"), printFunc);
    // add our animationTime
    QScriptValue animationTimeFunc = m_engine->newFunction(kwinEffectScriptAnimationTime);
    animationTimeFunc.setData(m_engine->newQObject(this));
    m_engine->globalObject().setProperty(QStringLiteral("animationTime"), animationTimeFunc);
    // add displayWidth and displayHeight
    QScriptValue displayWidthFunc = m_engine->newFunction(kwinEffectDisplayWidth);
    m_engine->globalObject().setProperty(QStringLiteral("displayWidth"), displayWidthFunc);
    QScriptValue displayHeightFunc = m_engine->newFunction(kwinEffectDisplayHeight);
    m_engine->globalObject().setProperty(QStringLiteral("displayHeight"), displayHeightFunc);
    // add global Shortcut
    registerGlobalShortcutFunction(this, m_engine, kwinScriptGlobalShortcut);
    registerScreenEdgeFunction(this, m_engine, kwinScriptScreenEdge);
    registerTouchScreenEdgeFunction(this, m_engine, kwinRegisterTouchScreenEdge);
    unregisterTouchScreenEdgeFunction(this, m_engine, kwinUnregisterTouchScreenEdge);
    // add the animate method
    QScriptValue animateFunc = m_engine->newFunction(kwinEffectAnimate);
    animateFunc.setData(m_engine->newQObject(this));
    m_engine->globalObject().setProperty(QStringLiteral("animate"), animateFunc);

    // and the set variant
    QScriptValue setFunc = m_engine->newFunction(kwinEffectSet);
    setFunc.setData(m_engine->newQObject(this));
    m_engine->globalObject().setProperty(QStringLiteral("set"), setFunc);

    // retarget
    QScriptValue retargetFunc = m_engine->newFunction(kwinEffectRetarget);
    retargetFunc.setData(m_engine->newQObject(this));
    m_engine->globalObject().setProperty(QStringLiteral("retarget"), retargetFunc);

    // redirect
    QScriptValue redirectFunc = m_engine->newFunction(kwinEffectRedirect);
    redirectFunc.setData(m_engine->newQObject(this));
    m_engine->globalObject().setProperty(QStringLiteral("redirect"), redirectFunc);

    // complete
    QScriptValue completeFunc = m_engine->newFunction(kwinEffectComplete);
    completeFunc.setData(m_engine->newQObject(this));
    m_engine->globalObject().setProperty(QStringLiteral("complete"), completeFunc);

    // cancel...
    QScriptValue cancelFunc = m_engine->newFunction(kwinEffectCancel);
    cancelFunc.setData(m_engine->newQObject(this));
    m_engine->globalObject().setProperty(QStringLiteral("cancel"), cancelFunc);

    QScriptValue ret = m_engine->evaluate(QString::fromUtf8(scriptFile.readAll()));

    if (ret.isError()) {
        signalHandlerException(ret);
        return false;
    }
    scriptFile.close();
    return true;
}

void ScriptedEffect::animationEnded(KWin::EffectWindow *w, Attribute a, uint meta)
{
    AnimationEffect::animationEnded(w, a, meta);
    emit animationEnded(w, 0);
}

QString ScriptedEffect::pluginId() const
{
    return m_effectName;
}

bool ScriptedEffect::isActiveFullScreenEffect() const
{
    return effects->activeFullScreenEffect() == this;
}

void ScriptedEffect::signalHandlerException(const QScriptValue &value)
{
    if (value.isError()) {
        qCDebug(KWIN_SCRIPTING) << "KWin Effect script encountered an error at [Line " << m_engine->uncaughtExceptionLineNumber() << "]";
        qCDebug(KWIN_SCRIPTING) << "Message: " << value.toString();

        QScriptValueIterator iter(value);
        while (iter.hasNext()) {
            iter.next();
            qCDebug(KWIN_SCRIPTING) << " " << iter.name() << ": " << iter.value().toString();
        }
    }
}

quint64 ScriptedEffect::animate(KWin::EffectWindow* w, KWin::AnimationEffect::Attribute a, int ms, KWin::FPx2 to, KWin::FPx2 from, uint metaData, int curve, int delay, bool fullScreen, bool keepAlive)
{
    QEasingCurve qec;
    if (curve < QEasingCurve::Custom)
        qec.setType(static_cast<QEasingCurve::Type>(curve));
    else if (curve == GaussianCurve)
        qec.setCustomType(qecGaussian);
    return AnimationEffect::animate(w, a, metaData, ms, to, qec, delay, from, fullScreen, keepAlive);
}

quint64 ScriptedEffect::set(KWin::EffectWindow* w, KWin::AnimationEffect::Attribute a, int ms, KWin::FPx2 to, KWin::FPx2 from, uint metaData, int curve, int delay, bool fullScreen, bool keepAlive)
{
    QEasingCurve qec;
    if (curve < QEasingCurve::Custom)
        qec.setType(static_cast<QEasingCurve::Type>(curve));
    else if (curve == GaussianCurve)
        qec.setCustomType(qecGaussian);
    return AnimationEffect::set(w, a, metaData, ms, to, qec, delay, from, fullScreen, keepAlive);
}

bool ScriptedEffect::retarget(quint64 animationId, KWin::FPx2 newTarget, int newRemainingTime)
{
    return AnimationEffect::retarget(animationId, newTarget, newRemainingTime);
}

bool ScriptedEffect::redirect(quint64 animationId, Direction direction, TerminationFlags terminationFlags)
{
    return AnimationEffect::redirect(animationId, direction, terminationFlags);
}

bool ScriptedEffect::complete(quint64 animationId)
{
    return AnimationEffect::complete(animationId);
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

bool ScriptedEffect::grab(EffectWindow *w, DataRole grabRole, bool force)
{
    void *grabber = w->data(grabRole).value<void *>();

    if (grabber == this) {
        return true;
    }

    if (grabber != nullptr && grabber != this && !force) {
        return false;
    }

    w->setData(grabRole, QVariant::fromValue(static_cast<void *>(this)));

    return true;
}

bool ScriptedEffect::ungrab(EffectWindow *w, DataRole grabRole)
{
    void *grabber = w->data(grabRole).value<void *>();

    if (grabber == nullptr) {
        return true;
    }

    if (grabber != this) {
        return false;
    }

    w->setData(grabRole, QVariant());

    return true;
}

void ScriptedEffect::reconfigure(ReconfigureFlags flags)
{
    AnimationEffect::reconfigure(flags);
    if (m_config) {
        m_config->read();
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

bool ScriptedEffect::registerTouchScreenCallback(int edge, QScriptValue callback)
{
    if (m_touchScreenEdgeCallbacks.constFind(edge) != m_touchScreenEdgeCallbacks.constEnd()) {
        return false;
    }
    QAction *action = new QAction(this);
    connect(action, &QAction::triggered, this,
        [callback] {
            QScriptValue invoke(callback);
            invoke.call();
        }
    );
    ScreenEdges::self()->reserveTouch(KWin::ElectricBorder(edge), action);
    m_touchScreenEdgeCallbacks.insert(edge, action);
    return true;
}

bool ScriptedEffect::unregisterTouchScreenCallback(int edge)
{
    auto it = m_touchScreenEdgeCallbacks.find(edge);
    if (it == m_touchScreenEdgeCallbacks.end()) {
        return false;
    }
    delete it.value();
    m_touchScreenEdgeCallbacks.erase(it);
    return true;
}

QScriptEngine *ScriptedEffect::engine() const
{
    return m_engine;
}

} // namespace
