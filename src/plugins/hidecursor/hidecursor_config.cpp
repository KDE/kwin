/*
    SPDX-FileCopyrightText: 2024 Jin Liu <m.liu.jin@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "hidecursor_config.h"

#include <config-kwin.h>

// KConfigSkeleton
#include "hidecursorconfig.h"

#include <KPluginFactory>
#include <kwineffects_interface.h>

K_PLUGIN_CLASS(KWin::HideCursorEffectConfig)

namespace KWin
{

InactivityDurationComboBox::InactivityDurationComboBox(QWidget *parent)
    : QComboBox(parent)
{
    connect(this, &QComboBox::currentIndexChanged, this, &InactivityDurationComboBox::durationChanged);
}

uint InactivityDurationComboBox::duration() const
{
    return currentData().toUInt();
}

void InactivityDurationComboBox::setDuration(uint duration)
{
    const int index = findData(duration);
    if (index == -1) {
        qWarning() << "Unknown duration:" << duration;
    } else {
        setCurrentIndex(index);
    }
}

HideCursorEffectConfig::HideCursorEffectConfig(QObject *parent, const KPluginMetaData &data)
    : KCModule(parent, data)
{
    HideCursorConfig::instance(KWIN_CONFIG);
    m_ui.setupUi(widget());

    auto inactivityDurationComboBox = new InactivityDurationComboBox(widget());
    inactivityDurationComboBox->setObjectName(QStringLiteral("kcfg_InactivityDuration"));
    inactivityDurationComboBox->setProperty("kcfg_property", QStringLiteral("duration"));
    m_ui.formLayout->setWidget(0, QFormLayout::FieldRole, inactivityDurationComboBox);

    const QList<uint> choices{0, 1, 5, 10, 15, 30, 60};
    for (const uint &choice : choices) {
        if (choice == 0) {
            inactivityDurationComboBox->addItem(i18nc("@item:inmenu never hide cursor on inactivity", "Never"), choice);
        } else {
            inactivityDurationComboBox->addItem(i18np("%1 second", "%1 seconds", choice), choice);
        }
    }
    if (const uint configuredInactivityDuration = HideCursorConfig::inactivityDuration(); !choices.contains(configuredInactivityDuration)) {
        inactivityDurationComboBox->addItem(i18np("%1 second", "%1 seconds", configuredInactivityDuration), configuredInactivityDuration);
    }

    addConfig(HideCursorConfig::self(), widget());
}

void HideCursorEffectConfig::save()
{
    KCModule::save();
    OrgKdeKwinEffectsInterface interface(QStringLiteral("org.kde.KWin"),
                                         QStringLiteral("/Effects"),
                                         QDBusConnection::sessionBus());
    interface.reconfigureEffect(QStringLiteral("hidecursor"));
}

} // namespace

#include "hidecursor_config.moc"

#include "moc_hidecursor_config.cpp"
