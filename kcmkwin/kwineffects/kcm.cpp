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
#include <KLocalizedString>
#include <KNS3/DownloadDialog>

#include <QQuickWindow>
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
            m_model->load(EffectModel::LoadOptions::KeepDirty);
        }
    }

    delete dialog;
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
}

} // namespace KWin

#include "kcm.moc"
