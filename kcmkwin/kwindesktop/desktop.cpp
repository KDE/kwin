// -*- c-basic-offset: 2 -*-
/**
 *  Copyright (c) 2000 Matthias Elter <elter@kde.org>
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

// Own
#include "desktop.h"

// Qt
#include <QtDBus/QDBusInterface>
#include <QtGui/QBoxLayout>
#include <QtGui/QCheckBox>
#include <QtGui/QLabel>
#include <QtGui/QFormLayout>
#include <QtGui/QSlider>
#include <QGroupBox>

#ifdef Q_WS_X11
#include <QX11Info>
#endif

// KDE
#include <kglobal.h>
#include <klocale.h>
#include <kdialog.h>
#include <klineedit.h>
#include <knuminput.h>
#include <kconfig.h>
#include <kconfiggroup.h>
#include <kgenericfactory.h>

#include <netwm.h>

K_PLUGIN_FACTORY(KWinKcmFactory,
        registerPlugin<KDesktopConfig>();
        )
K_EXPORT_PLUGIN(KWinKcmFactory("kcm_kwindesktop"))

#if 0
#include "kdesktop_interface.h"
#endif

// I'm using lineedits by intention as it makes sence to be able
// to see all desktop names at the same time. It also makes sense to
// be able to TAB through those line edits fast. So don't send me mails
// asking why I did not implement a more intelligent/smaller GUI.

KDesktopConfig::KDesktopConfig(QWidget *parent, const QVariantList &)
  : KCModule(KWinKcmFactory::componentData(), parent)
{

  setQuickHelp( i18n("<h1>Multiple Desktops</h1>In this module, you can configure how many virtual desktops you want and how these should be labeled."));

  Q_ASSERT(maxDesktops % 2 == 0);

  QVBoxLayout *layout = new QVBoxLayout(this);
  layout->setMargin(0);
  layout->setSpacing(KDialog::spacingHint());

  QHBoxLayout *lay = new QHBoxLayout();
  lay->setMargin(KDialog::marginHint());
  lay->setSpacing(KDialog::spacingHint());

  QLabel *label = new QLabel(i18n("N&umber of desktops: "), this);
  _numInput = new KIntNumInput(4, this);
  _numInput->setRange(1, maxDesktops, 1);
  connect(_numInput, SIGNAL(valueChanged(int)), SLOT(slotValueChanged(int)));
  connect(_numInput, SIGNAL(valueChanged(int)), SLOT( changed() ));
  label->setBuddy( _numInput );
  QString wtstr = i18n( "Here you can set how many virtual desktops you want on your KDE desktop. Move the slider to change the value." );
  label->setWhatsThis( wtstr );
  _numInput->setWhatsThis( wtstr );
  //QSpacerItem *horizontalSpacer = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

  lay->addWidget(label);
  lay->addWidget(_numInput);
  //lay->addItem(horizontalSpacer);
  lay->setStretchFactor( _numInput, 2 );

  layout->addLayout(lay);

  // name group
  QGroupBox *name_group = new QGroupBox(i18n("Desktop &Names"), this);
  QGridLayout *namesLayout = new QGridLayout;
  name_group->setLayout(namesLayout);
  for(int i = 0; i < maxDesktops; i++)
    {
      _nameLabel[i] = new QLabel(i18n("Desktop %1:", i+1), name_group);
      _nameInput[i] = new KLineEdit(name_group);
      _nameLabel[i]->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
      _nameLabel[i]->setWhatsThis( i18n( "Here you can enter the name for desktop %1", i+1 ) );
      _nameInput[i]->setWhatsThis( i18n( "Here you can enter the name for desktop %1", i+1 ) );

      connect(_nameInput[i], SIGNAL(textChanged(const QString&)),
           SLOT( changed() ));
      namesLayout->addWidget(_nameLabel[i], i, 0, 1, 1);
      namesLayout->addWidget(_nameInput[i], i, 1, 1, 1);
    }

  for(int i = 1; i < maxDesktops; i++)
      setTabOrder( _nameInput[i-1], _nameInput[i] );

  layout->addWidget(name_group);

#if 0
  _wheelOption = new QCheckBox(i18n("Mouse wheel over desktop background switches desktop"), this);
  connect(_wheelOption,SIGNAL(toggled(bool)), SLOT( changed() ));

  layout->addWidget(_wheelOption);
#endif
  layout->addStretch(1);

  // Begin check for immutable
#ifdef Q_WS_X11
  int kwin_screen_number = DefaultScreen(QX11Info::display());
#else
  int kwin_screen_number = 0;
#endif

  KSharedConfig::Ptr config = KSharedConfig::openConfig( "kwinrc" );

  QByteArray groupname;
  if (kwin_screen_number == 0)
     groupname = "Desktops";
  else
     groupname = "Desktops-screen-" + QByteArray::number ( kwin_screen_number );

  if (config->isGroupImmutable(groupname))
  {
     name_group->setEnabled(false);
     //number of desktops widgets
     label->setEnabled(false);
     _numInput->setEnabled(false);
  }
  else
  {
     KConfigGroup cfgGroup(config.data(), groupname.constData());
     if (cfgGroup.isEntryImmutable("Number"))
     {
        //number of desktops widgets
        label->setEnabled(false);
        _numInput->setEnabled(false);
     }
  }
  // End check for immutable

  load();
}

void KDesktopConfig::load()
{
#ifdef Q_WS_X11
  // get number of desktops
  NETRootInfo info( QX11Info::display(), NET::NumberOfDesktops | NET::DesktopNames );
  int n = info.numberOfDesktops();

  _numInput->setValue(n);

  for(int i = 1; i <= maxDesktops; i++)
  {
    QString name = QString::fromUtf8(info.desktopName(i));
    _nameInput[i-1]->setText(name);
  }

  for(int i = 1; i <= maxDesktops; i++)
  {
    _nameLabel[i-1]->setVisible(i <= n);
    _nameInput[i-1]->setVisible(i <= n);
  }
  emit changed(false);

#if 0
  KSharedConfig::Ptr desktopConfig = KSharedConfig::openConfig("kdesktoprc", KConfig::NoGlobals);
  desktopConfig->setGroup("Mouse Buttons");
  _wheelOption->setChecked(desktopConfig->readEntry("WheelSwitchesWorkspace", false));

  _wheelOptionImmutable = desktopConfig->isEntryImmutable("WheelSwitchesWorkspace");

  if (_wheelOptionImmutable || n<2)
     _wheelOption->setEnabled(false);
#endif
#else
  _numInput->setValue(1);
#endif
}

void KDesktopConfig::save()
{
#ifdef Q_WS_X11
  NETRootInfo info( QX11Info::display(), NET::NumberOfDesktops | NET::DesktopNames );
  // set desktop names
  for(int i = 1; i <= maxDesktops; i++)
  {
    info.setDesktopName(i, (_nameInput[i-1]->text()).toUtf8());
    info.activate();
  }
  // set number of desktops
  info.setNumberOfDesktops(_numInput->value());
  info.activate();

  XSync(QX11Info::display(), false);

#if 0
  KSharedConfig::Ptr desktopConfig = KSharedConfig::openConfig("kdesktoprc");
  desktopConfig->setGroup("Mouse Buttons");
  desktopConfig->writeEntry("WheelSwitchesWorkspace", _wheelOption->isChecked());

  // Tell kdesktop about the new config file
  int konq_screen_number = 0;
  if (QX11Info::display())
     konq_screen_number = DefaultScreen(QX11Info::display());

  QByteArray appname;
  if (konq_screen_number == 0)
      appname = "org.kde.kdesktop";
  else
      appname = "org.kde.kdesktop-screen-" + QByteArray::number( konq_screen_number );

  org::kde::kdesktop::Desktop desktop(appname, "/Desktop", QDBusConnection::sessionBus());
  desktop.configure();
#endif

  emit changed(false);
#endif
}

void KDesktopConfig::defaults()
{
  int n = 4;
  _numInput->setValue(n);

  for(int i = 0; i < maxDesktops; i++)
    _nameInput[i]->setText(i18n("Desktop %1", i+1));

  for(int i = 0; i < maxDesktops; i++)
  {
    _nameLabel[i]->setVisible(i < n);
    _nameInput[i]->setVisible(i < n);
  }

#if 0
  _wheelOption->setChecked(false);
  if (!_wheelOptionImmutable)
    _wheelOption->setEnabled(true);
#endif

  emit changed(false);
}

void KDesktopConfig::slotValueChanged(int n)
{
  for(int i = 0; i < maxDesktops; i++)
  {
    _nameLabel[i]->setVisible(i < n);
    _nameInput[i]->setVisible(i < n);
    if(i<n && _nameInput[i]->text().isEmpty())
      _nameInput[i]->setText(i18n("Desktop %1", i+1));
  }
#if 0
  if (!_wheelOptionImmutable)
    _wheelOption->setEnabled(n>1);
#endif
  emit changed(true);
}

#include "desktop.moc"

