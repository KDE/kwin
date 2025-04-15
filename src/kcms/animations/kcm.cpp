/*
    SPDX-FileCopyrightText: 2025 Oliver Beard <olib141@outlook.com>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include <QDBusConnection>
#include <QDBusMessage>
#include <QProcess>

#include <KPluginFactory>
#include <KService>

#include "effectsmodel.h"

#include "animationsdata.h"
#include "animationskdeglobalssettings.h"
#include "effectssubsetmodel.h"

#include "kcm.h"

K_PLUGIN_FACTORY_WITH_JSON(AnimationsKCMFactory, "kcm_animations.json", registerPlugin<KWin::AnimationsKCM>(); registerPlugin<KWin::AnimationsData>();)

namespace KWin
{

AnimationsKCM::AnimationsKCM(QObject *parent, const KPluginMetaData &metaData)
    : KQuickConfigModule(parent, metaData)
{
    m_animationsGlobalsSettings = new AnimationsGlobalsSettings(this);

    m_model = new EffectsModel(this);
    m_windowOpenCloseAnimations = new EffectsSubsetModel(m_model, QStringLiteral("toplevel-open-close-animation"), this);
    m_windowMaximizeAnimations = new EffectsSubsetModel(m_model, QStringLiteral("maximize"), this);
    m_windowMinimizeAnimations = new EffectsSubsetModel(m_model, QStringLiteral("minimize"), this);
    m_windowFullScreenAnimations = new EffectsSubsetModel(m_model, QStringLiteral("fullscreen"), this);
    m_peekDesktopAnimations = new EffectsSubsetModel(m_model, QStringLiteral("show-desktop"), this);
    m_virtualDesktopAnimations = new EffectsSubsetModel(m_model, QStringLiteral("desktop-animations"), this);
    m_otherEffects = new EffectsSubsetModel(m_model, {"fadingpopups", "slidingpopups", "login", "logout"}, this);

    connect(m_animationsGlobalsSettings, &AnimationsGlobalsSettings::animationDurationFactorChanged, this, &AnimationsKCM::updateNeedsSave);
    connect(m_model, &EffectsModel::dataChanged, this, &AnimationsKCM::updateNeedsSave);
    connect(m_model, &EffectsModel::loaded, this, &AnimationsKCM::updateNeedsSave);

    qmlRegisterAnonymousType<AnimationsGlobalsSettings>("org.kde.plasma.kcm.animations", 0);
    qmlRegisterUncreatableType<EffectsModel>("org.kde.plasma.kcm.animations", 0, 0, "EffectsModel", "Role enums only");
    qmlRegisterAnonymousType<EffectsSubsetModel>("org.kde.plasma.kcm.animations", 0);

    setButtons(Apply | Default | Help);
}

AnimationsKCM::~AnimationsKCM()
{
}

AnimationsGlobalsSettings *AnimationsKCM::globalsSettings() const
{
    return m_animationsGlobalsSettings;
}

EffectsSubsetModel *AnimationsKCM::windowOpenCloseAnimations() const
{
    return m_windowOpenCloseAnimations;
}

EffectsSubsetModel *AnimationsKCM::windowMaximizeAnimations() const
{
    return m_windowMaximizeAnimations;
}

EffectsSubsetModel *AnimationsKCM::windowMinimizeAnimations() const
{
    return m_windowMinimizeAnimations;
}

EffectsSubsetModel *AnimationsKCM::windowFullscreenAnimations() const
{
    return m_windowFullScreenAnimations;
}

EffectsSubsetModel *AnimationsKCM::peekDesktopAnimations() const
{
    return m_peekDesktopAnimations;
}

EffectsSubsetModel *AnimationsKCM::virtualDesktopAnimations() const
{
    return m_virtualDesktopAnimations;
}

EffectsSubsetModel *AnimationsKCM::otherEffects() const
{
    return m_otherEffects;
}

void AnimationsKCM::configure(const QString &pluginId, QQuickItem *context) const
{
    const QModelIndex index = m_model->findByPluginId(pluginId);
    m_model->requestConfigure(index, context);
}

QVariantMap AnimationsKCM::effectsKCMData() const
{
    if (KService::Ptr kcm = KService::serviceByStorageId(QStringLiteral("kcm_kwin_effects"))) {
        QVariantMap data;
        data["icon"] = kcm->icon();
        data["name"] = kcm->name();
        return data;
    }

    return {};
}

void AnimationsKCM::launchEffectsKCM() const
{
    QProcess::startDetached(QStringLiteral("systemsettings"), QStringList{QStringLiteral("kcm_kwin_effects")});
}

void AnimationsKCM::load()
{
    m_animationsGlobalsSettings->load();
    m_model->load();
    setNeedsSave(false);
}

void AnimationsKCM::save()
{
    m_animationsGlobalsSettings->save();
    m_model->save();
    setNeedsSave(false);

    {
        QDBusMessage message =
            QDBusMessage::createSignal(QStringLiteral("/KGlobalSettings"), QStringLiteral("org.kde.KGlobalSettings"), QStringLiteral("notifyChange"));
        QList<QVariant> args;
        args.append(3 /*KGlobalSettings::SettingsChanged*/);
        args.append(0 /*GlobalSettings::SettingsCategory::SETTINGS_MOUSE*/);
        message.setArguments(args);
        QDBusConnection::sessionBus().send(message);
    }
}

void AnimationsKCM::defaults()
{
    m_animationsGlobalsSettings->setDefaults();
    m_windowOpenCloseAnimations->defaults();
    m_windowMaximizeAnimations->defaults();
    m_windowMinimizeAnimations->defaults();
    m_windowFullScreenAnimations->defaults();
    m_peekDesktopAnimations->defaults();
    m_virtualDesktopAnimations->defaults();
    m_otherEffects->defaults();
    updateNeedsSave();
}

void AnimationsKCM::updateNeedsSave()
{
    setNeedsSave(m_animationsGlobalsSettings->isSaveNeeded() || m_model->needsSave());
    setRepresentsDefaults(m_animationsGlobalsSettings->isDefaults()
                          && m_windowOpenCloseAnimations->isDefaults()
                          && m_windowMaximizeAnimations->isDefaults()
                          && m_windowMinimizeAnimations->isDefaults()
                          && m_windowFullScreenAnimations->isDefaults()
                          && m_virtualDesktopAnimations->isDefaults()
                          && m_peekDesktopAnimations->isDefaults()
                          && m_otherEffects->isDefaults());
}

} // namespace KWin

#include "kcm.moc"
#include "moc_kcm.cpp"
