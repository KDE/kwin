/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2012 Filip Wieladek <wattos@gmail.com>

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

#include "mouseclick_config.h"

#include <kwineffects.h>

#include <KDE/KActionCollection>
#include <KDE/KAction>
#include <KDE/KConfigGroup>
#include <KDE/KShortcutsEditor>

#include <QWidget>

namespace KWin
{

KWIN_EFFECT_CONFIG_FACTORY

MouseClickEffectConfigForm::MouseClickEffectConfigForm(QWidget* parent) : QWidget(parent)
{
    setupUi(this);
}

MouseClickEffectConfig::MouseClickEffectConfig(QWidget* parent, const QVariantList& args) :
    KCModule(EffectFactory::componentData(), parent, args)
{
    m_ui = new MouseClickEffectConfigForm(this);

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->addWidget(m_ui);

    connect(m_ui->editor, SIGNAL(keyChange()), this, SLOT(changed()));
    connect(m_ui->button1_color_input, SIGNAL(currentIndexChanged(int)), this, SLOT(changed()));
    connect(m_ui->button2_color_input, SIGNAL(currentIndexChanged(int)), this, SLOT(changed()));
    connect(m_ui->button3_color_input, SIGNAL(currentIndexChanged(int)), this, SLOT(changed()));
    connect(m_ui->ring_line_width_input, SIGNAL(valueChanged(double)), this, SLOT(changed()));
    connect(m_ui->ring_duration_input, SIGNAL(valueChanged(int)), this, SLOT(changed()));
    connect(m_ui->ring_radius_input, SIGNAL(valueChanged(int)), this, SLOT(changed()));
    connect(m_ui->ring_count_input, SIGNAL(valueChanged(int)), this, SLOT(changed()));
    connect(m_ui->showtext_input, SIGNAL(toggled(bool)), this, SLOT(changed()));
    connect(m_ui->font_input, SIGNAL(fontSelected(QFont)), this, SLOT(changed()));

    // Shortcut config. The shortcut belongs to the component "kwin"!
    m_actionCollection = new KActionCollection(this, KComponentData("kwin"));

    KAction* a = static_cast<KAction*>(m_actionCollection->addAction("ToggleMouseClick"));
    a->setText(i18n("Toggle Effect"));
    a->setProperty("isConfigurationAction", true);
    a->setGlobalShortcut(KShortcut(Qt::META + Qt::Key_Asterisk));

    m_ui->editor->addCollection(m_actionCollection);
    load();
}

MouseClickEffectConfig::~MouseClickEffectConfig()
{
    // Undo (only) unsaved changes to global key shortcuts
    m_ui->editor->undoChanges();
}

void MouseClickEffectConfig::load()
{
    KCModule::load();

    KConfigGroup conf = EffectsHandler::effectConfig("MouseClick");

    QColor color1 = conf.readEntry("Color1", QColor(Qt::red));
    QColor color2 = conf.readEntry("Color2", QColor(Qt::green));
    QColor color3 = conf.readEntry("Color3", QColor(Qt::blue));

    float lineWidth = conf.readEntry("LineWidth", 1.f);
    float ringLife = conf.readEntry("RingLife", 300);
    float ringSize = conf.readEntry("RingSize", 20);

    bool showText = conf.readEntry("ShowText", true);
    int ringCount = conf.readEntry("RingCount", 2);
    QFont font = conf.readEntry("Font", QFont());

    m_ui->button1_color_input->setColor(color1);
    m_ui->button2_color_input->setColor(color2);
    m_ui->button3_color_input->setColor(color3);
    m_ui->ring_line_width_input->setValue(lineWidth);
    m_ui->ring_duration_input->setValue(ringLife);
    m_ui->ring_radius_input->setValue(ringSize);
    m_ui->ring_count_input->setValue(ringCount);

    m_ui->showtext_input->setChecked(showText);
    m_ui->font_input->setFont(font);

    m_ui->ring_line_width_input->setSuffix(i18n(" pixels"));
    m_ui->ring_duration_input->setSuffix(ki18np(" millisecond", " milliseconds"));
    m_ui->ring_radius_input->setSuffix(ki18np(" pixel", " pixels"));

    emit changed(false);
}

void MouseClickEffectConfig::save()
{
    //KCModule::save();

    KConfigGroup conf = EffectsHandler::effectConfig("MouseClick");

    conf.writeEntry("Color1", m_ui->button1_color_input->color());
    conf.writeEntry("Color2", m_ui->button2_color_input->color());
    conf.writeEntry("Color3", m_ui->button3_color_input->color());

    conf.writeEntry("LineWidth", m_ui->ring_line_width_input->value());
    conf.writeEntry("RingLife", m_ui->ring_duration_input->value());
    conf.writeEntry("RingSize", m_ui->ring_radius_input->value());
    conf.writeEntry("RingCount", m_ui->ring_count_input->value());

    conf.writeEntry("ShowText", m_ui->showtext_input->isChecked());
    conf.writeEntry("Font", m_ui->font_input->font());

    m_actionCollection->writeSettings();
    m_ui->editor->save();   // undo() will restore to this state from now on

    conf.sync();

    emit changed(false);
    EffectsHandler::sendReloadMessage("mouseclick");
}

void MouseClickEffectConfig::defaults()
{
    m_ui->button1_color_input->setColor(Qt::red);
    m_ui->button2_color_input->setColor(Qt::green);
    m_ui->button3_color_input->setColor(Qt::blue);

    m_ui->ring_line_width_input->setValue(1.f);
    m_ui->ring_duration_input->setValue(300);
    m_ui->ring_radius_input->setValue(20);
    m_ui->ring_count_input->setValue(2);

    m_ui->showtext_input->setChecked(true);
    m_ui->font_input->setFont(QFont());

    emit changed(true);
}


} // namespace

#include "mouseclick_config.moc"
