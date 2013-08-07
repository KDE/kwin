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

#include <kwineffects.h>

#include <kconfiggroup.h>
#include <kcolorscheme.h>
#include <KActionCollection>
#include <kaction.h>
#include <KDE/KAboutData>

#include <QVBoxLayout>
#include <QColor>

namespace KWin
{

KWIN_EFFECT_CONFIG_FACTORY

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

#warning Global Shortcuts need porting
#if KWIN_QT5_PORTING
    // Shortcut config. The shortcut belongs to the component "kwin"!
    m_actionCollection = new KActionCollection(this, KComponentData("kwin"));

    m_actionCollection->setConfigGroup(QStringLiteral("Cube"));
    m_actionCollection->setConfigGlobal(true);

    KAction* cubeAction = (KAction*) m_actionCollection->addAction(QStringLiteral("Cube"));
    cubeAction->setText(i18n("Desktop Cube"));
    cubeAction->setProperty("isConfigurationAction", true);
    cubeAction->setGlobalShortcut(KShortcut(Qt::CTRL + Qt::Key_F11));
    KAction* cylinderAction = (KAction*) m_actionCollection->addAction(QStringLiteral("Cylinder"));
    cylinderAction->setText(i18n("Desktop Cylinder"));
    cylinderAction->setProperty("isConfigurationAction", true);
    cylinderAction->setGlobalShortcut(KShortcut(), KAction::ActiveShortcut);
    KAction* sphereAction = (KAction*) m_actionCollection->addAction(QStringLiteral("Sphere"));
    sphereAction->setText(i18n("Desktop Sphere"));
    sphereAction->setProperty("isConfigurationAction", true);
    sphereAction->setGlobalShortcut(KShortcut(), KAction::ActiveShortcut);

    m_ui->editor->addCollection(m_actionCollection);
#endif
    connect(m_ui->kcfg_Caps, SIGNAL(stateChanged(int)), this, SLOT(capsSelectionChanged()));
    m_ui->kcfg_Wallpaper->setFilter(QStringLiteral("*.png *.jpeg *.jpg "));
    addConfig(CubeConfig::self(), m_ui);
    load();
}

void CubeEffectConfig::save()
{
    KCModule::save();
    m_ui->editor->save();
    EffectsHandler::sendReloadMessage(QStringLiteral("cube"));
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

#include "moc_cube_config.cpp"
