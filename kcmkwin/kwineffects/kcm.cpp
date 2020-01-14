/*
 * Copyright (C) 2019 Vlad Zahorodnii <vlad.zahorodnii@kde.org>
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
#include "effectsfilterproxymodel.h"
#include "effectsmodel.h"

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
    , m_model(new EffectsModel(this))
{
    qmlRegisterType<EffectsFilterProxyModel>("org.kde.private.kcms.kwin.effects", 1, 0, "EffectsFilterProxyModel");

    auto about = new KAboutData(
        QStringLiteral("kcm_kwin_effects"),
        i18n("Desktop Effects"),
        QStringLiteral("2.0"),
        QString(),
        KAboutLicense::GPL
    );
    about->addAuthor(i18n("Vlad Zahorodnii"), QString(), QStringLiteral("vlad.zahorodnii@kde.org"));
    setAboutData(about);

    setButtons(Apply | Default);

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
            m_model->load(EffectsModel::LoadOptions::KeepDirty);
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
    setRepresentsDefaults(m_model->isDefaults());
}

} // namespace KWin

#include "kcm.moc"
