/*
 * Copyright (C) 2019 Vlad Zagorodniy <vladzzag@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "kcm.h"
#include "effectmodel.h"
#include "effectsfilterproxymodel.h"

#include <KAboutData>
#include <KCModule>
#include <KLocalizedString>
#include <KNS3/DownloadDialog>
#include <KPluginFactory>
#include <KPluginTrader>

#include <QDialogButtonBox>
#include <QPushButton>
#include <QQuickWindow>
#include <QVBoxLayout>
#include <QWindow>

K_PLUGIN_FACTORY_WITH_JSON(DesktopEffectsKCMFactory,
                           "kcm_kwin_effects.json",
                           registerPlugin<KWin::DesktopEffectsKCM>();)

namespace KWin
{

DesktopEffectsKCM::DesktopEffectsKCM(QObject *parent, const QVariantList &args)
    : KQuickAddons::ConfigModule(parent, args)
    , m_model(new EffectModel(this))
{
    qmlRegisterType<EffectsFilterProxyModel>("org.kde.private.kcms.kwin.effects", 1, 0, "EffectsFilterProxyModel");

    auto about = new KAboutData(
        QStringLiteral("kcm_kwin_effects"),
        i18n("Configure Desktop Effects"),
        QStringLiteral("2.0"),
        QString(),
        KAboutLicense::GPL
    );
    about->addAuthor(i18n("Vlad Zagorodniy"), QString(), QStringLiteral("vladzzag@gmail.com"));
    setAboutData(about);

    setButtons(Apply | Default);

    connect(m_model, &EffectModel::dataChanged, this, &DesktopEffectsKCM::updateNeedsSave);
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

void DesktopEffectsKCM::openGHNS(QQuickItem *context)
{
    QPointer<KNS3::DownloadDialog> dialog = new KNS3::DownloadDialog(QStringLiteral("kwineffect.knsrc"));
    dialog->setWindowTitle(i18n("Download New Desktop Effects"));
    dialog->winId();

    if (context && context->window()) {
        dialog->windowHandle()->setTransientParent(context->window());
    }

    if (dialog->exec() == QDialog::Accepted) {
        if (!dialog->changedEntries().isEmpty()) {
            m_model->load();
        }
    }

    delete dialog;
}

static KCModule *findBinaryConfig(const QString &pluginId, QObject *parent)
{
    return KPluginTrader::createInstanceFromQuery<KCModule>(
        QStringLiteral("kwin/effects/configs/"),
        QString(),
        QStringLiteral("'%1' in [X-KDE-ParentComponents]").arg(pluginId),
        parent
    );
}

static KCModule *findScriptedConfig(const QString &pluginId, QObject *parent)
{
    const auto offers = KPluginTrader::self()->query(
        QStringLiteral("kwin/effects/configs/"),
        QString(),
        QStringLiteral("[X-KDE-Library] == 'kcm_kwin4_genericscripted'")
    );

    if (offers.isEmpty()) {
        return nullptr;
    }

    const KPluginInfo &generic = offers.first();
    KPluginLoader loader(generic.libraryPath());
    KPluginFactory *factory = loader.factory();
    if (!factory) {
        return nullptr;
    }

    return factory->create<KCModule>(pluginId, parent);
}

void DesktopEffectsKCM::configure(const QString &pluginId, QQuickItem *context)
{
    const QModelIndex idx = m_model->findByPluginId(pluginId);
    if (!idx.isValid()) {
        return;
    }

    QPointer<QDialog> dialog = new QDialog();

    KCModule *module = idx.data(EffectModel::ScriptedRole).toBool()
        ? findScriptedConfig(pluginId, dialog)
        : findBinaryConfig(pluginId, dialog);
    if (!module) {
        delete dialog;
        return;
    }

    dialog->setWindowTitle(idx.data(EffectModel::NameRole).toString());
    dialog->winId();

    auto buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok |
        QDialogButtonBox::Cancel |
        QDialogButtonBox::RestoreDefaults,
        dialog
    );
    connect(buttons, &QDialogButtonBox::accepted, dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, dialog, &QDialog::reject);
    connect(buttons->button(QDialogButtonBox::RestoreDefaults), &QPushButton::clicked,
        module, &KCModule::defaults);

    auto layout = new QVBoxLayout(dialog);
    layout->addWidget(module);
    layout->addWidget(buttons);

    if (context && context->window()) {
        dialog->windowHandle()->setTransientParent(context->window());
    }

    dialog->exec();

    delete dialog;
}

void DesktopEffectsKCM::updateNeedsSave()
{
    setNeedsSave(m_model->needsSave());
}

} // namespace KWin

#include "kcm.moc"
