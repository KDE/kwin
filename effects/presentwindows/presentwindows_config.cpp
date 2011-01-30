/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2007 Rivo Laks <rivolaks@hot.ee>
Copyright (C) 2008 Lucas Murray <lmurray@undefinedfire.com>

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

#include "presentwindows_config.h"
#include <kwineffects.h>

#include <kconfiggroup.h>
#include <KActionCollection>
#include <kaction.h>

#include <QVBoxLayout>

namespace KWin
{

KWIN_EFFECT_CONFIG_FACTORY

PresentWindowsEffectConfigForm::PresentWindowsEffectConfigForm(QWidget* parent) : QWidget(parent)
{
    setupUi(this);
}

PresentWindowsEffectConfig::PresentWindowsEffectConfig(QWidget* parent, const QVariantList& args)
    :   KCModule(EffectFactory::componentData(), parent, args)
{
    m_ui = new PresentWindowsEffectConfigForm(this);

    QVBoxLayout* layout = new QVBoxLayout(this);

    layout->addWidget(m_ui);

    // Shortcut config. The shortcut belongs to the component "kwin"!
    m_actionCollection = new KActionCollection(this, KComponentData("kwin"));

    m_actionCollection->setConfigGroup("PresentWindows");
    m_actionCollection->setConfigGlobal(true);

    KAction* a = (KAction*) m_actionCollection->addAction("ExposeAll");
    a->setText(i18n("Toggle Present Windows (All desktops)"));
    a->setProperty("isConfigurationAction", true);
    a->setGlobalShortcut(KShortcut(Qt::CTRL + Qt::Key_F10));

    KAction* b = (KAction*) m_actionCollection->addAction("Expose");
    b->setText(i18n("Toggle Present Windows (Current desktop)"));
    b->setProperty("isConfigurationAction", true);
    b->setGlobalShortcut(KShortcut(Qt::CTRL + Qt::Key_F9));

    KAction* c = (KAction*)m_actionCollection->addAction("ExposeClass");
    c->setText(i18n("Toggle Present Windows (Window class)"));
    c->setProperty("isConfigurationAction", true);
    c->setGlobalShortcut(KShortcut(Qt::CTRL + Qt::Key_F7));

    m_ui->shortcutEditor->addCollection(m_actionCollection);

    connect(m_ui->layoutCombo, SIGNAL(currentIndexChanged(int)), this, SLOT(changed()));
    connect(m_ui->displayTitleBox, SIGNAL(stateChanged(int)), this, SLOT(changed()));
    connect(m_ui->displayIconBox, SIGNAL(stateChanged(int)), this, SLOT(changed()));
    connect(m_ui->allowClosing, SIGNAL(stateChanged(int)), this, SLOT(changed()));
    connect(m_ui->ignoreMinimizedBox, SIGNAL(stateChanged(int)), this, SLOT(changed()));
    connect(m_ui->showPanelBox, SIGNAL(stateChanged(int)), this, SLOT(changed()));
    connect(m_ui->accuracySlider, SIGNAL(valueChanged(int)), this, SLOT(changed()));
    connect(m_ui->fillGapsBox, SIGNAL(stateChanged(int)), this, SLOT(changed()));
    connect(m_ui->shortcutEditor, SIGNAL(keyChange()), this, SLOT(changed()));
    connect(m_ui->leftButtonWindowCombo, SIGNAL(currentIndexChanged(int)), this, SLOT(changed()));
    connect(m_ui->middleButtonWindowCombo, SIGNAL(currentIndexChanged(int)), this, SLOT(changed()));
    connect(m_ui->rightButtonWindowCombo, SIGNAL(currentIndexChanged(int)), this, SLOT(changed()));
    connect(m_ui->leftButtonDesktopCombo, SIGNAL(currentIndexChanged(int)), this, SLOT(changed()));
    connect(m_ui->middleButtonDesktopCombo, SIGNAL(currentIndexChanged(int)), this, SLOT(changed()));
    connect(m_ui->rightButtonDesktopCombo, SIGNAL(currentIndexChanged(int)), this, SLOT(changed()));

    load();
}

PresentWindowsEffectConfig::~PresentWindowsEffectConfig()
{
    // If save() is called undoChanges() has no effect
    m_ui->shortcutEditor->undoChanges();
}

void PresentWindowsEffectConfig::load()
{
    KCModule::load();

    KConfigGroup conf = EffectsHandler::effectConfig("PresentWindows");

    int layoutMode = conf.readEntry("LayoutMode", int(PresentWindowsEffect::LayoutNatural));
    m_ui->layoutCombo->setCurrentIndex(layoutMode);

    bool displayTitle = conf.readEntry("DrawWindowCaptions", true);
    m_ui->displayTitleBox->setChecked(displayTitle);

    bool displayIcon = conf.readEntry("DrawWindowIcons", true);
    m_ui->displayIconBox->setChecked(displayIcon);

    bool allowClosing = conf.readEntry("AllowClosingWindows", true);
    m_ui->allowClosing->setChecked(allowClosing);

    bool ignoreMinimized = conf.readEntry("IgnoreMinimized", false);
    m_ui->ignoreMinimizedBox->setChecked(ignoreMinimized);

    bool showPanel = conf.readEntry("ShowPanel", false);
    m_ui->showPanelBox->setChecked(showPanel);

    int accuracy = conf.readEntry("Accuracy", 1);
    m_ui->accuracySlider->setSliderPosition(accuracy);

    bool fillGaps = conf.readEntry("FillGaps", true);
    m_ui->fillGapsBox->setChecked(fillGaps);

    int leftButtonWindow = conf.readEntry("LeftButtonWindow", int(PresentWindowsEffect::WindowActivateAction));
    m_ui->leftButtonWindowCombo->setCurrentIndex(leftButtonWindow);
    int middleButtonWindow = conf.readEntry("MiddleButtonWindow", int(PresentWindowsEffect::WindowNoAction));
    m_ui->middleButtonWindowCombo->setCurrentIndex(middleButtonWindow);
    int rightButtonWindow = conf.readEntry("RightButtonWindow", int(PresentWindowsEffect::WindowExitAction));
    m_ui->rightButtonWindowCombo->setCurrentIndex(rightButtonWindow);

    int leftButtonDesktop = conf.readEntry("LeftButtonDesktop", int(PresentWindowsEffect::DesktopExitAction));
    m_ui->leftButtonDesktopCombo->setCurrentIndex(leftButtonDesktop);
    int middleButtonDesktop = conf.readEntry("MiddleButtonDesktop", int(PresentWindowsEffect::DesktopNoAction));
    m_ui->middleButtonDesktopCombo->setCurrentIndex(middleButtonDesktop);
    int rightButtonDesktop = conf.readEntry("RightButtonDesktop", int(PresentWindowsEffect::DesktopNoAction));
    m_ui->rightButtonDesktopCombo->setCurrentIndex(rightButtonDesktop);

    emit changed(false);
}

void PresentWindowsEffectConfig::save()
{
    KCModule::save();

    KConfigGroup conf = EffectsHandler::effectConfig("PresentWindows");

    int layoutMode = m_ui->layoutCombo->currentIndex();
    conf.writeEntry("LayoutMode", layoutMode);

    conf.writeEntry("DrawWindowCaptions", m_ui->displayTitleBox->isChecked());
    conf.writeEntry("DrawWindowIcons", m_ui->displayIconBox->isChecked());
    conf.writeEntry("AllowClosingWindows", m_ui->allowClosing->isChecked());
    conf.writeEntry("IgnoreMinimized", m_ui->ignoreMinimizedBox->isChecked());
    conf.writeEntry("ShowPanel", m_ui->showPanelBox->isChecked());

    int accuracy = m_ui->accuracySlider->value();
    conf.writeEntry("Accuracy", accuracy);

    conf.writeEntry("FillGaps", m_ui->fillGapsBox->isChecked());

    int leftButtonWindow = m_ui->leftButtonWindowCombo->currentIndex();
    conf.writeEntry("LeftButtonWindow", leftButtonWindow);
    int middleButtonWindow = m_ui->middleButtonWindowCombo->currentIndex();
    conf.writeEntry("MiddleButtonWindow", middleButtonWindow);
    int rightButtonWindow = m_ui->rightButtonWindowCombo->currentIndex();
    conf.writeEntry("RightButtonWindow", rightButtonWindow);

    int leftButtonDesktop = m_ui->leftButtonDesktopCombo->currentIndex();
    conf.writeEntry("LeftButtonDesktop", leftButtonDesktop);
    int middleButtonDesktop = m_ui->middleButtonDesktopCombo->currentIndex();
    conf.writeEntry("MiddleButtonDesktop", middleButtonDesktop);
    int rightButtonDesktop = m_ui->rightButtonDesktopCombo->currentIndex();
    conf.writeEntry("RightButtonDesktop", rightButtonDesktop);

    m_ui->shortcutEditor->save();

    conf.sync();

    emit changed(false);
    EffectsHandler::sendReloadMessage("presentwindows");
}

void PresentWindowsEffectConfig::defaults()
{
    m_ui->layoutCombo->setCurrentIndex(int(PresentWindowsEffect::LayoutNatural));
    m_ui->displayTitleBox->setChecked(true);
    m_ui->displayIconBox->setChecked(true);
    m_ui->allowClosing->setChecked(true);
    m_ui->ignoreMinimizedBox->setChecked(false);
    m_ui->showPanelBox->setChecked(false);
    m_ui->accuracySlider->setSliderPosition(1);
    m_ui->fillGapsBox->setChecked(true);
    m_ui->shortcutEditor->allDefault();
    m_ui->leftButtonWindowCombo->setCurrentIndex(int(PresentWindowsEffect::WindowActivateAction));
    m_ui->middleButtonWindowCombo->setCurrentIndex(int(PresentWindowsEffect::WindowNoAction));
    m_ui->rightButtonWindowCombo->setCurrentIndex(int(PresentWindowsEffect::WindowExitAction));
    m_ui->leftButtonDesktopCombo->setCurrentIndex(int(PresentWindowsEffect::DesktopExitAction));
    m_ui->middleButtonDesktopCombo->setCurrentIndex(int(PresentWindowsEffect::DesktopNoAction));
    m_ui->rightButtonDesktopCombo->setCurrentIndex(int(PresentWindowsEffect::DesktopNoAction));
    emit changed(true);
}

} // namespace

#include "presentwindows_config.moc"
