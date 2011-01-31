/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

 Copyright (C) 2008 Martin Gräßlin <ubuntu@martin-graesslin.com>

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
#include <kwineffects.h>

#include <kconfiggroup.h>
#include <kcolorscheme.h>
#include <KActionCollection>
#include <kaction.h>

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
    KCModule(EffectFactory::componentData(), parent, args)
{
    m_ui = new CubeEffectConfigForm(this);

    QVBoxLayout* layout = new QVBoxLayout(this);

    layout->addWidget(m_ui);

    m_ui->tabWidget->setTabText(0, i18nc("@title:tab Basic Settings", "Basic"));
    m_ui->tabWidget->setTabText(1, i18nc("@title:tab Advanced Settings", "Advanced"));

    // Shortcut config. The shortcut belongs to the component "kwin"!
    m_actionCollection = new KActionCollection(this, KComponentData("kwin"));

    m_actionCollection->setConfigGroup("Cube");
    m_actionCollection->setConfigGlobal(true);

    KAction* cubeAction = (KAction*) m_actionCollection->addAction("Cube");
    cubeAction->setText(i18n("Desktop Cube"));
    cubeAction->setProperty("isConfigurationAction", true);
    cubeAction->setGlobalShortcut(KShortcut(Qt::CTRL + Qt::Key_F11));
    KAction* cylinderAction = (KAction*) m_actionCollection->addAction("Cylinder");
    cylinderAction->setText(i18n("Desktop Cylinder"));
    cylinderAction->setProperty("isConfigurationAction", true);
    cylinderAction->setGlobalShortcut(KShortcut(), KAction::ActiveShortcut);
    KAction* sphereAction = (KAction*) m_actionCollection->addAction("Sphere");
    sphereAction->setText(i18n("Desktop Sphere"));
    sphereAction->setProperty("isConfigurationAction", true);
    sphereAction->setGlobalShortcut(KShortcut(), KAction::ActiveShortcut);

    m_ui->editor->addCollection(m_actionCollection);

    m_ui->wallpaperRequester->setFilter("*.png *.jpeg *.jpg ");

    connect(m_ui->rotationDurationSpin, SIGNAL(valueChanged(int)), this, SLOT(changed()));
    connect(m_ui->cubeOpacitySlider, SIGNAL(valueChanged(int)), this, SLOT(changed()));
    connect(m_ui->cubeOpacitySpin, SIGNAL(valueChanged(int)), this, SLOT(changed()));
    connect(m_ui->desktopOpacityOnlyBox, SIGNAL(stateChanged(int)), this, SLOT(changed()));
    connect(m_ui->displayDesktopNameBox, SIGNAL(stateChanged(int)), this, SLOT(changed()));
    connect(m_ui->reflectionBox, SIGNAL(stateChanged(int)), this, SLOT(changed()));
    connect(m_ui->backgroundColorButton, SIGNAL(changed(QColor)), this, SLOT(changed()));
    connect(m_ui->cubeCapsBox, SIGNAL(stateChanged(int)), this, SLOT(changed()));
    connect(m_ui->cubeCapsBox, SIGNAL(stateChanged(int)), this, SLOT(capsSelectionChanged()));
    connect(m_ui->capsImageBox, SIGNAL(stateChanged(int)), this, SLOT(changed()));
    connect(m_ui->capColorButton, SIGNAL(changed(QColor)), this, SLOT(changed()));
    connect(m_ui->wallpaperRequester, SIGNAL(textChanged(QString)), this, SLOT(changed()));
    connect(m_ui->closeOnMouseReleaseBox, SIGNAL(stateChanged(int)), this, SLOT(changed()));
    connect(m_ui->zPositionSlider, SIGNAL(valueChanged(int)), this, SLOT(changed()));
    connect(m_ui->walkThroughDesktopBox, SIGNAL(stateChanged(int)), this, SLOT(changed()));
    connect(m_ui->invertKeysBox, SIGNAL(stateChanged(int)), this, SLOT(changed()));
    connect(m_ui->invertMouseBox, SIGNAL(stateChanged(int)), this, SLOT(changed()));
    connect(m_ui->capDeformationSlider, SIGNAL(valueChanged(int)), this, SLOT(changed()));
    connect(m_ui->zOrderingBox, SIGNAL(stateChanged(int)), this, SLOT(changed()));

    load();
}

void CubeEffectConfig::load()
{
    KCModule::load();

    KConfigGroup conf = EffectsHandler::effectConfig("Cube");

    int duration       = conf.readEntry("RotationDuration", 0);
    float opacity      = conf.readEntry("Opacity", 80);
    bool desktopOpacityOnly = conf.readEntry("OpacityDesktopOnly", false);
    bool desktopName   = conf.readEntry("DisplayDesktopName", true);
    bool reflection    = conf.readEntry("Reflection", true);
    QColor background  = conf.readEntry("BackgroundColor", QColor(Qt::black));
    QColor capColor = conf.readEntry("CapColor", KColorScheme(QPalette::Active, KColorScheme::Window).background().color());
    bool texturedCaps = conf.readEntry("TexturedCaps", true);
    bool caps = conf.readEntry("Caps", true);
    bool closeOnMouseRelease = conf.readEntry("CloseOnMouseRelease", false);
    bool walkThroughDesktop = conf.readEntry("TabBox", false);
    m_ui->zPositionSlider->setValue(conf.readEntry("ZPosition", 100));
    m_ui->wallpaperRequester->setUrl(KUrl(conf.readEntry("Wallpaper", "")));
    bool invertKeys = conf.readEntry("InvertKeys", false);
    bool invertMouse = conf.readEntry("InvertMouse", false);
    m_ui->capDeformationSlider->setValue(conf.readEntry("CapDeformation", 0));
    bool zOrdering = conf.readEntry("ZOrdering", false);

    m_ui->rotationDurationSpin->setValue(duration);
    m_ui->rotationDurationSpin->setSuffix(ki18np(" millisecond", " milliseconds"));
    m_ui->cubeOpacitySlider->setValue(opacity);
    m_ui->cubeOpacitySpin->setValue(opacity);
    m_ui->desktopOpacityOnlyBox->setChecked(desktopOpacityOnly);
    if (desktopName) {
        m_ui->displayDesktopNameBox->setCheckState(Qt::Checked);
    } else {
        m_ui->displayDesktopNameBox->setCheckState(Qt::Unchecked);
    }
    if (reflection) {
        m_ui->reflectionBox->setCheckState(Qt::Checked);
    } else {
        m_ui->reflectionBox->setCheckState(Qt::Unchecked);
    }
    if (caps) {
        m_ui->cubeCapsBox->setCheckState(Qt::Checked);
    } else {
        m_ui->cubeCapsBox->setCheckState(Qt::Unchecked);
    }
    if (texturedCaps) {
        m_ui->capsImageBox->setCheckState(Qt::Checked);
    } else {
        m_ui->capsImageBox->setCheckState(Qt::Unchecked);
    }
    if (closeOnMouseRelease) {
        m_ui->closeOnMouseReleaseBox->setCheckState(Qt::Checked);
    } else {
        m_ui->closeOnMouseReleaseBox->setCheckState(Qt::Unchecked);
    }
    if (walkThroughDesktop) {
        m_ui->walkThroughDesktopBox->setCheckState(Qt::Checked);
    } else {
        m_ui->walkThroughDesktopBox->setCheckState(Qt::Unchecked);
    }
    m_ui->backgroundColorButton->setColor(background);
    m_ui->capColorButton->setColor(capColor);
    m_ui->invertKeysBox->setChecked(invertKeys);
    m_ui->invertMouseBox->setChecked(invertMouse);
    m_ui->zOrderingBox->setChecked(zOrdering);
    capsSelectionChanged();

    emit changed(false);
}

void CubeEffectConfig::save()
{
    KConfigGroup conf = EffectsHandler::effectConfig("Cube");

    conf.writeEntry("RotationDuration", m_ui->rotationDurationSpin->value());
    conf.writeEntry("DisplayDesktopName", m_ui->displayDesktopNameBox->checkState() == Qt::Checked ? true : false);
    conf.writeEntry("Reflection", m_ui->reflectionBox->checkState() == Qt::Checked ? true : false);
    conf.writeEntry("Opacity", m_ui->cubeOpacitySpin->value());
    conf.writeEntry("OpacityDesktopOnly", m_ui->desktopOpacityOnlyBox->isChecked());
    conf.writeEntry("BackgroundColor", m_ui->backgroundColorButton->color());
    conf.writeEntry("Caps", m_ui->cubeCapsBox->checkState() == Qt::Checked ? true : false);
    conf.writeEntry("CapColor", m_ui->capColorButton->color());
    conf.writeEntry("TexturedCaps", m_ui->capsImageBox->checkState() == Qt::Checked ? true : false);
    conf.writeEntry("CloseOnMouseRelease", m_ui->closeOnMouseReleaseBox->checkState() == Qt::Checked ? true : false);
    conf.writeEntry("Wallpaper", m_ui->wallpaperRequester->url().path());
    conf.writeEntry("ZPosition", m_ui->zPositionSlider->value());
    conf.writeEntry("TabBox", m_ui->walkThroughDesktopBox->checkState() == Qt::Checked ? true : false);
    conf.writeEntry("InvertKeys", m_ui->invertKeysBox->isChecked());
    conf.writeEntry("InvertMouse", m_ui->invertMouseBox->isChecked());
    conf.writeEntry("CapDeformation", m_ui->capDeformationSlider->value());
    conf.writeEntry("ZOrdering", m_ui->zOrderingBox->isChecked());

    m_ui->editor->save();

    conf.sync();

    emit changed(false);
    EffectsHandler::sendReloadMessage("cube");
}

void CubeEffectConfig::defaults()
{
    m_ui->rotationDurationSpin->setValue(0);
    m_ui->displayDesktopNameBox->setCheckState(Qt::Checked);
    m_ui->reflectionBox->setCheckState(Qt::Checked);
    m_ui->cubeOpacitySpin->setValue(80);
    m_ui->cubeOpacitySlider->setValue(80);
    m_ui->desktopOpacityOnlyBox->setChecked(false);
    m_ui->backgroundColorButton->setColor(QColor(Qt::black));
    m_ui->cubeCapsBox->setCheckState(Qt::Checked);
    m_ui->capColorButton->setColor(KColorScheme(QPalette::Active, KColorScheme::Window).background().color());
    m_ui->capsImageBox->setCheckState(Qt::Checked);
    m_ui->closeOnMouseReleaseBox->setCheckState(Qt::Unchecked);
    m_ui->wallpaperRequester->setUrl(KUrl(""));
    m_ui->zPositionSlider->setValue(100);
    m_ui->walkThroughDesktopBox->setCheckState(Qt::Unchecked);
    m_ui->invertKeysBox->setChecked(false);
    m_ui->invertMouseBox->setChecked(false);
    m_ui->capDeformationSlider->setValue(0);
    m_ui->zOrderingBox->setChecked(false);
    m_ui->editor->allDefault();
    emit changed(true);
}

void CubeEffectConfig::capsSelectionChanged()
{
    if (m_ui->cubeCapsBox->checkState() == Qt::Checked) {
        // activate cap color
        m_ui->capColorButton->setEnabled(true);
        m_ui->capColorLabel->setEnabled(true);
        m_ui->capsImageBox->setEnabled(true);
    } else {
        // deactivate cap color
        m_ui->capColorButton->setEnabled(false);
        m_ui->capColorLabel->setEnabled(false);
        m_ui->capsImageBox->setEnabled(false);
    }
}

} // namespace

#include "cube_config.moc"
