/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2012 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2018 David Edmundson <davidedmundson@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "scriptedeffect.h"
#include "scripting_logging.h"
#include "scriptingutils.h"
#include "workspace_wrapper.h"

#include "input.h"
#include "screenedge.h"
#include "workspace.h"
// KDE
#include <KConfigGroup>
#include <KGlobalAccel>
#include <KPluginMetaData>
#include <kconfigloader.h>
#include <kwinglutils.h>
// Qt
#include <QAction>
#include <QFile>
#include <QQmlEngine>
#include <QStandardPaths>
#include <QVector>

#include <optional>

Q_DECLARE_METATYPE(KSharedConfigPtr)

namespace KWin
{

struct AnimationSettings
{
    enum {
        Type = 1 << 0,
        Curve = 1 << 1,
        Delay = 1 << 2,
        Duration = 1 << 3,
        FullScreen = 1 << 4,
        KeepAlive = 1 << 5,
        FrozenTime = 1 << 6
    };
    AnimationEffect::Attribute type;
    QEasingCurve::Type curve;
    QJSValue from;
    QJSValue to;
    int delay;
    qint64 frozenTime;
    uint duration;
    uint set;
    uint metaData;
    bool fullScreenEffect;
    bool keepAlive;
    std::optional<uint> shader;
};

AnimationSettings animationSettingsFromObject(const QJSValue &object)
{
    AnimationSettings settings;
    settings.set = 0;
    settings.metaData = 0;

    settings.to = object.property(QStringLiteral("to"));
    settings.from = object.property(QStringLiteral("from"));

    const QJSValue duration = object.property(QStringLiteral("duration"));
    if (duration.isNumber()) {
        settings.duration = duration.toUInt();
        settings.set |= AnimationSettings::Duration;
    } else {
        settings.duration = 0;
    }

    const QJSValue delay = object.property(QStringLiteral("delay"));
    if (delay.isNumber()) {
        settings.delay = delay.toInt();
        settings.set |= AnimationSettings::Delay;
    } else {
        settings.delay = 0;
    }

    const QJSValue curve = object.property(QStringLiteral("curve"));
    if (curve.isNumber()) {
        settings.curve = static_cast<QEasingCurve::Type>(curve.toInt());
        settings.set |= AnimationSettings::Curve;
    } else {
        settings.curve = QEasingCurve::Linear;
    }

    const QJSValue type = object.property(QStringLiteral("type"));
    if (type.isNumber()) {
        settings.type = static_cast<AnimationEffect::Attribute>(type.toInt());
        settings.set |= AnimationSettings::Type;
    } else {
        settings.type = static_cast<AnimationEffect::Attribute>(-1);
    }

    const QJSValue isFullScreen = object.property(QStringLiteral("fullScreen"));
    if (isFullScreen.isBool()) {
        settings.fullScreenEffect = isFullScreen.toBool();
        settings.set |= AnimationSettings::FullScreen;
    } else {
        settings.fullScreenEffect = false;
    }

    const QJSValue keepAlive = object.property(QStringLiteral("keepAlive"));
    if (keepAlive.isBool()) {
        settings.keepAlive = keepAlive.toBool();
        settings.set |= AnimationSettings::KeepAlive;
    } else {
        settings.keepAlive = true;
    }

    const QJSValue frozenTime = object.property(QStringLiteral("frozenTime"));
    if (frozenTime.isNumber()) {
        settings.frozenTime = frozenTime.toInt();
        settings.set |= AnimationSettings::FrozenTime;
    } else {
        settings.frozenTime = -1;
    }

    if (const auto shader = object.property(QStringLiteral("fragmentShader")); shader.isNumber()) {
        settings.shader = shader.toUInt();
    }

    return settings;
}

static KWin::FPx2 fpx2FromScriptValue(const QJSValue &value)
{
    if (value.isNull()) {
        return FPx2();
    }
    if (value.isNumber()) {
        return FPx2(value.toNumber());
    }
    if (value.isObject()) {
        const QJSValue value1 = value.property(QStringLiteral("value1"));
        const QJSValue value2 = value.property(QStringLiteral("value2"));
        if (!value1.isNumber() || !value2.isNumber()) {
            qCDebug(KWIN_SCRIPTING) << "Cannot cast scripted FPx2 to C++";
            return FPx2();
        }
        return FPx2(value1.toNumber(), value2.toNumber());
    }
    return FPx2();
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
                                                      QLatin1String("kwin/effects/") + name + QLatin1String("/contents/") + scriptName);
    if (scriptFile.isNull()) {
        qCDebug(KWIN_SCRIPTING) << "Could not locate the effect script";
        return nullptr;
    }

    return ScriptedEffect::create(name, scriptFile, effect.value(QStringLiteral("X-KDE-Ordering"), 0), effect.value(QStringLiteral("X-KWin-Exclusive-Category")));
}

ScriptedEffect *ScriptedEffect::create(const QString &effectName, const QString &pathToScript, int chainPosition, const QString &exclusiveCategory)
{
    ScriptedEffect *effect = new ScriptedEffect();
    effect->m_exclusiveCategory = exclusiveCategory;
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
    , m_engine(new QJSEngine(this))
    , m_scriptFile(QString())
    , m_config(nullptr)
    , m_chainPosition(0)
{
    Q_ASSERT(effects);
    connect(effects, &EffectsHandler::activeFullScreenEffectChanged, this, [this]() {
        Effect *fullScreenEffect = effects->activeFullScreenEffect();
        if (fullScreenEffect == m_activeFullScreenEffect) {
            return;
        }
        if (m_activeFullScreenEffect == this || fullScreenEffect == this) {
            Q_EMIT isActiveFullScreenEffectChanged();
        }
        m_activeFullScreenEffect = fullScreenEffect;
    });
}

ScriptedEffect::~ScriptedEffect() = default;

bool ScriptedEffect::init(const QString &effectName, const QString &pathToScript)
{
    qRegisterMetaType<QJSValueList>();
    qRegisterMetaType<EffectWindowList>();

    QFile scriptFile(pathToScript);
    if (!scriptFile.open(QIODevice::ReadOnly)) {
        qCDebug(KWIN_SCRIPTING) << "Could not open script file: " << pathToScript;
        return false;
    }
    m_effectName = effectName;
    m_scriptFile = pathToScript;

    // does the effect contain an KConfigXT file?
    const QString kconfigXTFile = QStandardPaths::locate(QStandardPaths::GenericDataLocation, QLatin1String("kwin/effects/") + m_effectName + QLatin1String("/contents/config/main.xml"));
    if (!kconfigXTFile.isNull()) {
        KConfigGroup cg = QCoreApplication::instance()->property("config").value<KSharedConfigPtr>()->group(QStringLiteral("Effect-%1").arg(m_effectName));
        QFile xmlFile(kconfigXTFile);
        m_config = new KConfigLoader(cg, &xmlFile, this);
        m_config->load();
    }

    m_engine->installExtensions(QJSEngine::ConsoleExtension);

    QJSValue globalObject = m_engine->globalObject();

    QJSValue effectsObject = m_engine->newQObject(effects);
    QQmlEngine::setObjectOwnership(effects, QQmlEngine::CppOwnership);
    globalObject.setProperty(QStringLiteral("effects"), effectsObject);

    QJSValue selfObject = m_engine->newQObject(this);
    QQmlEngine::setObjectOwnership(this, QQmlEngine::CppOwnership);
    globalObject.setProperty(QStringLiteral("effect"), selfObject);

    // desktopChanged is overloaded, which is problematic. Old code exposed the signal also
    // with parameters. QJSEngine does not so we have to fake it.
    effectsObject.setProperty(QStringLiteral("desktopChanged(int,int)"),
                              effectsObject.property(QStringLiteral("desktopChangedLegacy")));
    effectsObject.setProperty(QStringLiteral("desktopChanged(int,int,KWin::EffectWindow*)"),
                              effectsObject.property(QStringLiteral("desktopChanged")));

    globalObject.setProperty(QStringLiteral("Effect"),
                             m_engine->newQMetaObject(&ScriptedEffect::staticMetaObject));
    globalObject.setProperty(QStringLiteral("KWin"),
                             m_engine->newQMetaObject(&QtScriptWorkspaceWrapper::staticMetaObject));
    globalObject.setProperty(QStringLiteral("Globals"),
                             m_engine->newQMetaObject(&KWin::staticMetaObject));
    globalObject.setProperty(QStringLiteral("QEasingCurve"),
                             m_engine->newQMetaObject(&QEasingCurve::staticMetaObject));

    static const QStringList globalProperties{
        QStringLiteral("animationTime"),
        QStringLiteral("displayWidth"),
        QStringLiteral("displayHeight"),

        QStringLiteral("registerShortcut"),
        QStringLiteral("registerScreenEdge"),
        QStringLiteral("registerRealtimeScreenEdge"),
        QStringLiteral("registerTouchScreenEdge"),
        QStringLiteral("unregisterScreenEdge"),
        QStringLiteral("unregisterTouchScreenEdge"),

        QStringLiteral("animate"),
        QStringLiteral("set"),
        QStringLiteral("retarget"),
        QStringLiteral("freezeInTime"),
        QStringLiteral("redirect"),
        QStringLiteral("complete"),
        QStringLiteral("cancel"),
        QStringLiteral("addShader"),
        QStringLiteral("setUniform"),
    };

    for (const QString &propertyName : globalProperties) {
        globalObject.setProperty(propertyName, selfObject.property(propertyName));
    }

    const QJSValue result = m_engine->evaluate(QString::fromUtf8(scriptFile.readAll()));

    if (result.isError()) {
        qCWarning(KWIN_SCRIPTING, "%s:%d: error: %s", qPrintable(scriptFile.fileName()),
                  result.property(QStringLiteral("lineNumber")).toInt(),
                  qPrintable(result.property(QStringLiteral("message")).toString()));
        return false;
    }

    return true;
}

void ScriptedEffect::animationEnded(KWin::EffectWindow *w, Attribute a, uint meta)
{
    AnimationEffect::animationEnded(w, a, meta);
    Q_EMIT animationEnded(w, 0);
}

QString ScriptedEffect::pluginId() const
{
    return m_effectName;
}

bool ScriptedEffect::isActiveFullScreenEffect() const
{
    return effects->activeFullScreenEffect() == this;
}

QList<int> ScriptedEffect::touchEdgesForAction(const QString &action) const
{
    QList<int> ret;
    if (m_exclusiveCategory == QStringLiteral("show-desktop") && action == QStringLiteral("show-desktop")) {
        for (const auto b : {ElectricTop, ElectricRight, ElectricBottom, ElectricLeft}) {
            if (workspace()->screenEdges()->actionForTouchBorder(b) == ElectricActionShowDesktop) {
                ret.append(b);
            }
        }
        return ret;
    } else {
        if (!m_config) {
            return ret;
        }
        return m_config->property(QStringLiteral("TouchBorderActivate") + action).value<QList<int>>();
    }
}

QJSValue ScriptedEffect::animate_helper(const QJSValue &object, AnimationType animationType)
{
    QJSValue windowProperty = object.property(QStringLiteral("window"));
    if (!windowProperty.isObject()) {
        m_engine->throwError(QStringLiteral("Window property missing in animation options"));
        return QJSValue();
    }

    EffectWindow *window = qobject_cast<EffectWindow *>(windowProperty.toQObject());
    if (!window) {
        m_engine->throwError(QStringLiteral("Window property references invalid window"));
        return QJSValue();
    }

    QVector<AnimationSettings> settings{animationSettingsFromObject(object)}; // global

    QJSValue animations = object.property(QStringLiteral("animations")); // array
    if (!animations.isUndefined()) {
        if (!animations.isArray()) {
            m_engine->throwError(QStringLiteral("Animations provided but not an array"));
            return QJSValue();
        }

        const int length = static_cast<int>(animations.property(QStringLiteral("length")).toInt());
        for (int i = 0; i < length; ++i) {
            QJSValue value = animations.property(QString::number(i));
            if (value.isObject()) {
                AnimationSettings s = animationSettingsFromObject(value);
                const uint set = s.set | settings.at(0).set;
                // Catch show stoppers (incompletable animation)
                if (!(set & AnimationSettings::Type)) {
                    m_engine->throwError(QStringLiteral("Type property missing in animation options"));
                    return QJSValue();
                }
                if (!(set & AnimationSettings::Duration)) {
                    m_engine->throwError(QStringLiteral("Duration property missing in animation options"));
                    return QJSValue();
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
                if (!s.shader.has_value()) {
                    s.shader = settings.at(0).shader;
                }

                s.metaData = 0;
                typedef QMap<AnimationEffect::MetaType, QString> MetaTypeMap;
                static MetaTypeMap metaTypes({{AnimationEffect::SourceAnchor, QStringLiteral("sourceAnchor")},
                                              {AnimationEffect::TargetAnchor, QStringLiteral("targetAnchor")},
                                              {AnimationEffect::RelativeSourceX, QStringLiteral("relativeSourceX")},
                                              {AnimationEffect::RelativeSourceY, QStringLiteral("relativeSourceY")},
                                              {AnimationEffect::RelativeTargetX, QStringLiteral("relativeTargetX")},
                                              {AnimationEffect::RelativeTargetY, QStringLiteral("relativeTargetY")},
                                              {AnimationEffect::Axis, QStringLiteral("axis")}});

                for (auto it = metaTypes.constBegin(),
                          end = metaTypes.constEnd();
                     it != end; ++it) {
                    QJSValue metaVal = value.property(*it);
                    if (metaVal.isNumber()) {
                        AnimationEffect::setMetaData(it.key(), metaVal.toInt(), s.metaData);
                    }
                }
                if (s.type == ShaderUniform && s.shader) {
                    auto uniformProperty = value.property(QStringLiteral("uniform")).toString();
                    auto shader = findShader(s.shader.value());
                    if (!shader) {
                        m_engine->throwError(QStringLiteral("Shader for given shaderId not found"));
                        return {};
                    }
                    if (!effects->makeOpenGLContextCurrent()) {
                        m_engine->throwError(QStringLiteral("Failed to make OpenGL context current"));
                        return {};
                    }
                    ShaderBinder binder{shader};
                    s.metaData = shader->uniformLocation(uniformProperty.toUtf8().constData());
                }

                settings << s;
            }
        }
    }

    if (settings.count() == 1) {
        const uint set = settings.at(0).set;
        if (!(set & AnimationSettings::Type)) {
            m_engine->throwError(QStringLiteral("Type property missing in animation options"));
            return QJSValue();
        }
        if (!(set & AnimationSettings::Duration)) {
            m_engine->throwError(QStringLiteral("Duration property missing in animation options"));
            return QJSValue();
        }
    } else if (!(settings.at(0).set & AnimationSettings::Type)) { // invalid global
        settings.removeAt(0); // -> get rid of it, only used to complete the others
    }

    if (settings.isEmpty()) {
        m_engine->throwError(QStringLiteral("No animations provided"));
        return QJSValue();
    }

    QJSValue array = m_engine->newArray(settings.length());
    for (int i = 0; i < settings.count(); i++) {
        const AnimationSettings &setting = settings[i];
        int animationId;
        if (animationType == AnimationType::Set) {
            animationId = set(window,
                              setting.type,
                              setting.duration,
                              setting.to,
                              setting.from,
                              setting.metaData,
                              setting.curve,
                              setting.delay,
                              setting.fullScreenEffect,
                              setting.keepAlive,
                              setting.shader ? setting.shader.value() : 0u);
            if (setting.frozenTime >= 0) {
                freezeInTime(animationId, setting.frozenTime);
            }
        } else {
            animationId = animate(window,
                                  setting.type,
                                  setting.duration,
                                  setting.to,
                                  setting.from,
                                  setting.metaData,
                                  setting.curve,
                                  setting.delay,
                                  setting.fullScreenEffect,
                                  setting.keepAlive,
                                  setting.shader ? setting.shader.value() : 0u);
            if (setting.frozenTime >= 0) {
                freezeInTime(animationId, setting.frozenTime);
            }
        }
        array.setProperty(i, animationId);
    }

    return array;
}

quint64 ScriptedEffect::animate(KWin::EffectWindow *window, KWin::AnimationEffect::Attribute attribute,
                                int ms, const QJSValue &to, const QJSValue &from, uint metaData, int curve,
                                int delay, bool fullScreen, bool keepAlive, uint shaderId)
{
    QEasingCurve qec;
    if (curve < QEasingCurve::Custom) {
        qec.setType(static_cast<QEasingCurve::Type>(curve));
    } else if (curve == GaussianCurve) {
        qec.setCustomType(qecGaussian);
    }
    return AnimationEffect::animate(window, attribute, metaData, ms, fpx2FromScriptValue(to), qec,
                                    delay, fpx2FromScriptValue(from), fullScreen, keepAlive, findShader(shaderId));
}

QJSValue ScriptedEffect::animate(const QJSValue &object)
{
    return animate_helper(object, AnimationType::Animate);
}

quint64 ScriptedEffect::set(KWin::EffectWindow *window, KWin::AnimationEffect::Attribute attribute,
                            int ms, const QJSValue &to, const QJSValue &from, uint metaData, int curve,
                            int delay, bool fullScreen, bool keepAlive, uint shaderId)
{
    QEasingCurve qec;
    if (curve < QEasingCurve::Custom) {
        qec.setType(static_cast<QEasingCurve::Type>(curve));
    } else if (curve == GaussianCurve) {
        qec.setCustomType(qecGaussian);
    }
    return AnimationEffect::set(window, attribute, metaData, ms, fpx2FromScriptValue(to), qec,
                                delay, fpx2FromScriptValue(from), fullScreen, keepAlive, findShader(shaderId));
}

QJSValue ScriptedEffect::set(const QJSValue &object)
{
    return animate_helper(object, AnimationType::Set);
}

bool ScriptedEffect::retarget(quint64 animationId, const QJSValue &newTarget, int newRemainingTime)
{
    return AnimationEffect::retarget(animationId, fpx2FromScriptValue(newTarget), newRemainingTime);
}

bool ScriptedEffect::retarget(const QList<quint64> &animationIds, const QJSValue &newTarget, int newRemainingTime)
{
    return std::all_of(animationIds.begin(), animationIds.end(), [&](quint64 animationId) {
        return retarget(animationId, newTarget, newRemainingTime);
    });
}

bool ScriptedEffect::freezeInTime(quint64 animationId, qint64 frozenTime)
{
    return AnimationEffect::freezeInTime(animationId, frozenTime);
}

bool ScriptedEffect::freezeInTime(const QList<quint64> &animationIds, qint64 frozenTime)
{
    return std::all_of(animationIds.begin(), animationIds.end(), [&](quint64 animationId) {
        return AnimationEffect::freezeInTime(animationId, frozenTime);
    });
}

bool ScriptedEffect::redirect(quint64 animationId, Direction direction, TerminationFlags terminationFlags)
{
    return AnimationEffect::redirect(animationId, direction, terminationFlags);
}

bool ScriptedEffect::redirect(const QList<quint64> &animationIds, Direction direction, TerminationFlags terminationFlags)
{
    return std::all_of(animationIds.begin(), animationIds.end(), [&](quint64 animationId) {
        return redirect(animationId, direction, terminationFlags);
    });
}

bool ScriptedEffect::complete(quint64 animationId)
{
    return AnimationEffect::complete(animationId);
}

bool ScriptedEffect::complete(const QList<quint64> &animationIds)
{
    return std::all_of(animationIds.begin(), animationIds.end(), [&](quint64 animationId) {
        return complete(animationId);
    });
}

bool ScriptedEffect::cancel(quint64 animationId)
{
    return AnimationEffect::cancel(animationId);
}

bool ScriptedEffect::cancel(const QList<quint64> &animationIds)
{
    bool ret = false;
    for (const quint64 &animationId : animationIds) {
        ret |= cancel(animationId);
    }
    return ret;
}

bool ScriptedEffect::isGrabbed(EffectWindow *w, ScriptedEffect::DataRole grabRole)
{
    void *e = w->data(static_cast<KWin::DataRole>(grabRole)).value<void *>();
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
    Q_EMIT configChanged();
}

void ScriptedEffect::registerShortcut(const QString &objectName, const QString &text,
                                      const QString &keySequence, const QJSValue &callback)
{
    if (!callback.isCallable()) {
        m_engine->throwError(QStringLiteral("Shortcut handler must be callable"));
        return;
    }
    QAction *action = new QAction(this);
    action->setObjectName(objectName);
    action->setText(text);
    const QKeySequence shortcut = QKeySequence(keySequence);
    KGlobalAccel::self()->setShortcut(action, QList<QKeySequence>() << shortcut);
    connect(action, &QAction::triggered, this, [this, action, callback]() {
        QJSValue actionObject = m_engine->newQObject(action);
        QQmlEngine::setObjectOwnership(action, QQmlEngine::CppOwnership);
        QJSValue(callback).call(QJSValueList{actionObject});
    });
}

bool ScriptedEffect::borderActivated(ElectricBorder edge)
{
    auto it = screenEdgeCallbacks().constFind(edge);
    if (it != screenEdgeCallbacks().constEnd()) {
        for (const QJSValue &callback : it.value()) {
            QJSValue(callback).call();
        }
    }
    return true;
}

QJSValue ScriptedEffect::readConfig(const QString &key, const QJSValue &defaultValue)
{
    if (!m_config) {
        return defaultValue;
    }
    return m_engine->toScriptValue(m_config->property(key));
}

int ScriptedEffect::displayWidth() const
{
    return workspace()->geometry().width();
}

int ScriptedEffect::displayHeight() const
{
    return workspace()->geometry().height();
}

int ScriptedEffect::animationTime(int defaultTime) const
{
    return Effect::animationTime(defaultTime);
}

bool ScriptedEffect::registerScreenEdge(int edge, const QJSValue &callback)
{
    if (!callback.isCallable()) {
        m_engine->throwError(QStringLiteral("Screen edge handler must be callable"));
        return false;
    }
    auto it = screenEdgeCallbacks().find(edge);
    if (it == screenEdgeCallbacks().end()) {
        // not yet registered
        workspace()->screenEdges()->reserve(static_cast<KWin::ElectricBorder>(edge), this, "borderActivated");
        screenEdgeCallbacks().insert(edge, QJSValueList{callback});
    } else {
        it->append(callback);
    }
    return true;
}

bool ScriptedEffect::registerRealtimeScreenEdge(int edge, const QJSValue &callback)
{
    if (!callback.isCallable()) {
        m_engine->throwError(QStringLiteral("Screen edge handler must be callable"));
        return false;
    }
    auto it = realtimeScreenEdgeCallbacks().find(edge);
    if (it == realtimeScreenEdgeCallbacks().end()) {
        // not yet registered
        realtimeScreenEdgeCallbacks().insert(edge, QJSValueList{callback});
        auto *triggerAction = new QAction(this);
        connect(triggerAction, &QAction::triggered, this, [this, edge]() {
            auto it = realtimeScreenEdgeCallbacks().constFind(edge);
            if (it != realtimeScreenEdgeCallbacks().constEnd()) {
                for (const QJSValue &callback : it.value()) {
                    QJSValue(callback).call({edge});
                }
            }
        });
        effects->registerRealtimeTouchBorder(static_cast<KWin::ElectricBorder>(edge), triggerAction, [this](ElectricBorder border, const QPointF &deltaProgress, EffectScreen *screen) {
            auto it = realtimeScreenEdgeCallbacks().constFind(border);
            if (it != realtimeScreenEdgeCallbacks().constEnd()) {
                for (const QJSValue &callback : it.value()) {
                    QJSValue delta = m_engine->newObject();
                    delta.setProperty("width", deltaProgress.x());
                    delta.setProperty("height", deltaProgress.y());

                    QJSValue(callback).call({border, QJSValue(delta), m_engine->newQObject(screen)});
                }
            }
        });
    } else {
        it->append(callback);
    }
    return true;
}

bool ScriptedEffect::unregisterScreenEdge(int edge)
{
    auto it = screenEdgeCallbacks().find(edge);
    if (it == screenEdgeCallbacks().end()) {
        // not previously registered
        return false;
    }
    workspace()->screenEdges()->unreserve(static_cast<KWin::ElectricBorder>(edge), this);
    screenEdgeCallbacks().erase(it);
    return true;
}

bool ScriptedEffect::registerTouchScreenEdge(int edge, const QJSValue &callback)
{
    if (m_touchScreenEdgeCallbacks.constFind(edge) != m_touchScreenEdgeCallbacks.constEnd()) {
        return false;
    }
    if (!callback.isCallable()) {
        m_engine->throwError(QStringLiteral("Touch screen edge handler must be callable"));
        return false;
    }
    QAction *action = new QAction(this);
    connect(action, &QAction::triggered, this, [callback]() {
        QJSValue(callback).call();
    });
    workspace()->screenEdges()->reserveTouch(KWin::ElectricBorder(edge), action);
    m_touchScreenEdgeCallbacks.insert(edge, action);
    return true;
}

bool ScriptedEffect::unregisterTouchScreenEdge(int edge)
{
    auto it = m_touchScreenEdgeCallbacks.find(edge);
    if (it == m_touchScreenEdgeCallbacks.end()) {
        return false;
    }
    delete it.value();
    m_touchScreenEdgeCallbacks.erase(it);
    return true;
}

QJSEngine *ScriptedEffect::engine() const
{
    return m_engine;
}

uint ScriptedEffect::addFragmentShader(ShaderTrait traits, const QString &fragmentShaderFile)
{
    if (!effects->makeOpenGLContextCurrent()) {
        m_engine->throwError(QStringLiteral("Failed to make OpenGL context current"));
        return 0;
    }
    const QString shaderDir{QLatin1String("kwin/effects/") + m_effectName + QLatin1String("/contents/shaders/")};
    const QString fragment = fragmentShaderFile.isEmpty() ? QString{} : QStandardPaths::locate(QStandardPaths::GenericDataLocation, shaderDir + fragmentShaderFile);

    auto shader = ShaderManager::instance()->generateShaderFromFile(static_cast<KWin::ShaderTraits>(int(traits)), {}, fragment);
    if (!shader->isValid()) {
        m_engine->throwError(QStringLiteral("Shader failed to load"));
        // 0 is never a valid shader identifier, it's ensured the first shader gets id 1
        return 0;
    }

    const uint shaderId{m_nextShaderId};
    m_nextShaderId++;
    m_shaders[shaderId] = std::move(shader);
    return shaderId;
}

GLShader *ScriptedEffect::findShader(uint shaderId) const
{
    if (auto it = m_shaders.find(shaderId); it != m_shaders.end()) {
        return it->second.get();
    }
    return nullptr;
}

void ScriptedEffect::setUniform(uint shaderId, const QString &name, const QJSValue &value)
{
    auto shader = findShader(shaderId);
    if (!shader) {
        m_engine->throwError(QStringLiteral("Shader for given shaderId not found"));
        return;
    }
    if (!effects->makeOpenGLContextCurrent()) {
        m_engine->throwError(QStringLiteral("Failed to make OpenGL context current"));
        return;
    }
    auto setColorUniform = [this, shader, name] (const QColor &color)
    {
        if (!color.isValid()) {
            return;
        }
        if (!shader->setUniform(name.toUtf8().constData(), color)) {
            m_engine->throwError(QStringLiteral("Failed to set uniform ") + name);
        }
    };
    ShaderBinder binder{shader};
    if (value.isString()) {
        setColorUniform(value.toString());
    } else if (value.isNumber()) {
        if (!shader->setUniform(name.toUtf8().constData(), float(value.toNumber()))) {
            m_engine->throwError(QStringLiteral("Failed to set uniform ") + name);
        }
    } else if (value.isArray()) {
        const auto length = value.property(QStringLiteral("length")).toInt();
        if (length == 2) {
            if (!shader->setUniform(name.toUtf8().constData(), QVector2D{float(value.property(0).toNumber()), float(value.property(1).toNumber())})) {
                m_engine->throwError(QStringLiteral("Failed to set uniform ") + name);
            }
        } else if (length == 3) {
            if (!shader->setUniform(name.toUtf8().constData(), QVector3D{float(value.property(0).toNumber()), float(value.property(1).toNumber()), float(value.property(2).toNumber())})) {
                m_engine->throwError(QStringLiteral("Failed to set uniform ") + name);
            }
        } else if (length == 4) {
            if (!shader->setUniform(name.toUtf8().constData(), QVector4D{float(value.property(0).toNumber()), float(value.property(1).toNumber()), float(value.property(2).toNumber()), float(value.property(3).toNumber())})) {
                m_engine->throwError(QStringLiteral("Failed to set uniform ") + name);
            }
        } else {
            m_engine->throwError(QStringLiteral("Invalid number of elements in array"));
        }
    } else if (value.isVariant()) {
        const auto variant = value.toVariant();
        setColorUniform(variant.value<QColor>());
    } else {
        m_engine->throwError(QStringLiteral("Invalid value provided for uniform"));
    }
}

} // namespace
