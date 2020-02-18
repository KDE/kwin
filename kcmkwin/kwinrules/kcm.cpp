/*
 * Copyright (c) 2004 Lubos Lunak <l.lunak@kde.org>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "kcm.h"

#include <QLayout>
//Added by qt3to4:
#include <QVBoxLayout>
#include <KLocalizedString>
#include <kaboutdata.h>
#include <QtDBus>
#include <QX11Info>

#include "ruleslist.h"
#include <KPluginFactory>
#include <KPluginLoader>

K_PLUGIN_FACTORY(KCMRulesFactory,
                 registerPlugin<KWin::KCMRules>();
                )

namespace KWin
{

KCMRules::KCMRules(QWidget *parent, const QVariantList &)
    : KCModule(parent)
{
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    widget = new KCMRulesList(this);
    layout->addWidget(widget);
    connect(widget, SIGNAL(changed(bool)), SLOT(moduleChanged(bool)));
    KAboutData *about = new KAboutData(QStringLiteral("kcmkwinrules"),
                                       i18n("Window-Specific Settings Configuration Module"),
                                       QString(), QString(), KAboutLicense::GPL, i18n("(c) 2004 KWin and KControl Authors"));
    about->addAuthor(i18n("Lubos Lunak"), QString(), "l.lunak@kde.org");
    setAboutData(about);
}

void KCMRules::load()
{
    widget->load();
    emit KCModule::changed(false);
}

void KCMRules::save()
{
    widget->save();
    emit KCModule::changed(false);
    // Send signal to all kwin instances
    QDBusMessage message =
        QDBusMessage::createSignal("/KWin", "org.kde.KWin", "reloadConfig");
    QDBusConnection::sessionBus().send(message);

}

void KCMRules::defaults()
{
    widget->defaults();
}

QString KCMRules::quickHelp() const
{
    return i18n("<p><h1>Window-specific Settings</h1> Here you can customize window settings specifically only"
                " for some windows.</p>"
                " <p>Please note that this configuration will not take effect if you do not use"
                " KWin as your window manager. If you do use a different window manager, please refer to its documentation"
                " for how to customize window behavior.</p>");
}

void KCMRules::moduleChanged(bool state)
{
    emit KCModule::changed(state);
}

}

// i18n freeze :-/
#if 0
I18N_NOOP("Remember settings separately for every window")
I18N_NOOP("Show internal settings for remembering")
I18N_NOOP("Internal setting for remembering")
#endif


#include "kcm.moc"
