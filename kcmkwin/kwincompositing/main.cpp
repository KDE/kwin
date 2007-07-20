/*****************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2007 Rivo Laks <rivolaks@hot.ee>

You can Freely distribute this program under the GNU General Public
License. See the file "COPYING" for the exact licensing terms.
******************************************************************/

#include "main.h"

#include "advanced.h"

#include <kgenericfactory.h>
#include <kaboutdata.h>
#include <kconfig.h>
#include <kconfiggroup.h>
#include <kdebug.h>
#include <ksettings/dispatcher.h>

#include <QtDBus/QtDBus>




typedef KGenericFactory<KWin::KWinCompositingConfig> KWinCompositingConfigFactory;
K_EXPORT_COMPONENT_FACTORY( kcm_kwincompositing, KWinCompositingConfigFactory("kcmkwincompositing"))


namespace KWin
{


KWinCompositingConfig::KWinCompositingConfig(QWidget *parent, const QStringList &)
    : KCModule( KWinCompositingConfigFactory::componentData(), parent),
        mKWinConfig(KSharedConfig::openConfig("kwinrc"))
{
    ui.setupUi(this);

    connect(ui.advancedOptions, SIGNAL(clicked()), this, SLOT(showAdvancedOptions()));
    connect(ui.useCompositing, SIGNAL(toggled(bool)), ui.compositingOptionsContainer, SLOT(setEnabled(bool)));

    connect(ui.useCompositing, SIGNAL(toggled(bool)), this, SLOT(changed()));

    // Load config
    load();

    KAboutData *about = new KAboutData(I18N_NOOP("kcmkwincompositing"), 0,
            ki18n("KWin Desktop Effects Configuration Module"),
            0, KLocalizedString(), KAboutData::License_GPL, ki18n("(c) 2007 Rivo Laks"));
    about->addAuthor(ki18n("Rivo Laks"), KLocalizedString(), "rivolaks@hot.ee");
    setAboutData(about);
}

KWinCompositingConfig::~KWinCompositingConfig()
{
}

void KWinCompositingConfig::reparseConfiguration(const QByteArray&conf)
{
  KSettings::Dispatcher::reparseConfiguration(conf);
}

void KWinCompositingConfig::showAdvancedOptions()
{
    KWinAdvancedCompositingOptions* dialog = new KWinAdvancedCompositingOptions(this, mKWinConfig);

    dialog->show();
    connect(dialog, SIGNAL(configSaved()), this, SLOT(configChanged()));
}

void KWinCompositingConfig::load()
{
    kDebug() << k_funcinfo << endl;
    mKWinConfig->reparseConfiguration();

    KConfigGroup config(mKWinConfig, "Compositing");
    ui.useCompositing->setChecked(config.readEntry("Enabled", false));

    emit changed( false );
}


void KWinCompositingConfig::save()
{
    kDebug() << k_funcinfo << endl;

    KConfigGroup config(mKWinConfig, "Compositing");
    config.writeEntry("Enabled", ui.useCompositing->isChecked());

    // TODO: save effects

    emit changed( false );

    configChanged();
}

void KWinCompositingConfig::configChanged()
{
    // Send signal to kwin
    mKWinConfig->sync();
    // Send signal to all kwin instances
    QDBusMessage message = QDBusMessage::createSignal("/KWin", "org.kde.KWin", "reloadConfig");
    QDBusConnection::sessionBus().send(message);
}


void KWinCompositingConfig::defaults()
{
    kDebug() << k_funcinfo << endl;
    ui.useCompositing->setChecked(false);
}

QString KWinCompositingConfig::quickHelp() const
{
    kDebug() << k_funcinfo << endl;
    return i18n("<h1>Compositing</h1>");
}

} // namespace

#include "main.moc"
