/*
    SPDX-FileCopyrightText: 2019 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "kcm.h"
#include "desktopeffectsdata.h"
#include "effectsfilterproxymodel.h"
#include "effectsmodel.h"

#include <KAboutData>
#include <KLocalizedString>
#include <KPluginFactory>

#include <QQuickWindow>
#include <QWindow>

K_PLUGIN_FACTORY_WITH_JSON(DesktopEffectsKCMFactory,
                           "kcm_kwin_effects.json",
                           registerPlugin<KWin::DesktopEffectsKCM>();
                           registerPlugin<KWin::DesktopEffectsData>();)

namespace KWin
{

DesktopEffectsKCM::DesktopEffectsKCM(QObject *parent, const QVariantList &args)
    : KQuickAddons::ConfigModule(parent, args)
    , m_model(new EffectsModel(this))
{
    qmlRegisterType<EffectsFilterProxyModel>("org.kde.private.kcms.kwin.effects", 1, 0, "EffectsFilterProxyModel");

    auto about = new KAboutData(
        QStringLiteral("kcm_kwin_effects"),
        i18n("Desktop Effects"),
        QStringLiteral("2.0"),
        QString(),
        KAboutLicense::GPL);
    about->addAuthor(i18n("Vlad Zahorodnii"), QString(), QStringLiteral("vlad.zahorodnii@kde.org"));
    setAboutData(about);

    setButtons(Apply | Default | Help);

    connect(m_model, &EffectsModel::dataChanged, this, &DesktopEffectsKCM::updateNeedsSave);
    connect(m_model, &EffectsModel::loaded, this, &DesktopEffectsKCM::updateNeedsSave);
}

DesktopEffectsKCM::~DesktopEffectsKCM()
{
}

QAbstractItemModel *DesktopEffectsKCM::effectsModel() const
{
    return m_model;
}

void DesktopEffectsKCM::load()
{
    m_model->load();
    setNeedsSave(false);
}

void DesktopEffectsKCM::save()
{
    m_model->save();
    setNeedsSave(false);
}

void DesktopEffectsKCM::defaults()
{
    m_model->defaults();
    updateNeedsSave();
}

void DesktopEffectsKCM::onGHNSEntriesChanged()
{
    m_model->load(EffectsModel::LoadOptions::KeepDirty);
}

void DesktopEffectsKCM::configure(const QString &pluginId, QQuickItem *context)
{
    const QModelIndex index = m_model->findByPluginId(pluginId);

    QWindow *transientParent = nullptr;
    if (context && context->window()) {
        transientParent = context->window();
    }

    m_model->requestConfigure(index, transientParent);
}

void DesktopEffectsKCM::updateNeedsSave()
{
    setNeedsSave(m_model->needsSave());
    setRepresentsDefaults(m_model->isDefaults());
}

} // namespace KWin

#include "kcm.moc"
