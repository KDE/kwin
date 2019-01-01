/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

 Copyright (C) 2008 Martin Gräßlin <mgraesslin@kde.org>

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
#include "cube_config.h"
// KConfigSkeleton
#include "cubeconfig.h"
#include <config-kwin.h>
#include <kwineffects_interface.h>

#include <QAction>

#include <kconfiggroup.h>
#include <kcolorscheme.h>
#include <KActionCollection>
#include <KAboutData>
#include <KGlobalAccel>
#include <KLocalizedString>
#include <KPluginFactory>

#include <QVBoxLayout>
#include <QColor>

K_PLUGIN_FACTORY_WITH_JSON(CubeEffectConfigFactory,
                           "cube_config.json",
                           registerPlugin<KWin::CubeEffectConfig>();)

namespace KWin
{

CubeEffectConfigForm::CubeEffectConfigForm(QWidget* parent) : QWidget(parent)
{
    setupUi(this);
}

CubeEffectConfig::CubeEffectConfig(QWidget* parent, const QVariantList& args) :
    KCModule(KAboutData::pluginData(QStringLiteral("cube")), parent, args)
{
    m_ui = new CubeEffectConfigForm(this);

    QVBoxLayout* layout = new QVBoxLayout(this);

    layout->addWidget(m_ui);

    m_ui->tabWidget->setTabText(0, i18nc("@title:tab Basic Settings", "Basic"));
    m_ui->tabWidget->setTabText(1, i18nc("@title:tab Advanced Settings", "Advanced"));

    // Shortcut config. The shortcut belongs to the component "kwin"!
    m_actionCollection = new KActionCollection(this, QStringLiteral("kwin"));
    m_actionCollection->setComponentDisplayName(i18n("KWin"));

    m_actionCollection->setConfigGroup(QStringLiteral("Cube"));
    m_actionCollection->setConfigGlobal(true);

    QAction* cubeAction = m_actionCollection->addAction(QStringLiteral("Cube"));
    cubeAction->setText(i18n("Desktop Cube"));
    cubeAction->setProperty("isConfigurationAction", true);
    KGlobalAccel::self()->setDefaultShortcut(cubeAction, QList<QKeySequence>() << Qt::CTRL + Qt::Key_F11);
    KGlobalAccel::self()->setShortcut(cubeAction, QList<QKeySequence>() << Qt::CTRL + Qt::Key_F11);
    QAction* cylinderAction = m_actionCollection->addAction(QStringLiteral("Cylinder"));
    cylinderAction->setText(i18n("Desktop Cylinder"));
    cylinderAction->setProperty("isConfigurationAction", true);
    KGlobalAccel::self()->setShortcut(cylinderAction, QList<QKeySequence>());
    QAction* sphereAction = m_actionCollection->addAction(QStringLiteral("Sphere"));
    sphereAction->setText(i18n("Desktop Sphere"));
    sphereAction->setProperty("isConfigurationAction", true);
    KGlobalAccel::self()->setShortcut(sphereAction, QList<QKeySequence>());

    m_ui->editor->addCollection(m_actionCollection);

    capsSelectionChanged();
    connect(m_ui->kcfg_Caps, &QCheckBox::stateChanged, this, &CubeEffectConfig::capsSelectionChanged);
    m_ui->kcfg_Wallpaper->setFilter(QStringLiteral("*.png *.jpeg *.jpg "));
    CubeConfig::instance(KWIN_CONFIG);
    addConfig(CubeConfig::self(), m_ui);
    load();
}

void CubeEffectConfig::save()
{
    KCModule::save();
    m_ui->editor->save();
    OrgKdeKwinEffectsInterface interface(QStringLiteral("org.kde.KWin"),
                                         QStringLiteral("/Effects"),
                                         QDBusConnection::sessionBus());
    interface.reconfigureEffect(QStringLiteral("cube"));
}

void CubeEffectConfig::capsSelectionChanged()
{
    if (m_ui->kcfg_Caps->checkState() == Qt::Checked) {
        // activate cap color
        m_ui->kcfg_CapColor->setEnabled(true);
        m_ui->capColorLabel->setEnabled(true);
        m_ui->kcfg_TexturedCaps->setEnabled(true);
    } else {
        // deactivate cap color
        m_ui->kcfg_CapColor->setEnabled(false);
        m_ui->capColorLabel->setEnabled(false);
        m_ui->kcfg_TexturedCaps->setEnabled(false);
    }
}

} // namespace

#include "cube_config.moc"
