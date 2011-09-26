//////////////////////////////////////////////////////////////////////////////
// oxygenshadowdemodialog.h
// oxygen shadow demo dialog
// -------------------
//
// Copyright (c) 2010 Hugo Pereira Da Costa <hugo@oxygen-icons.org>
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to
// deal in the Software without restriction, including without limitation the
// rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
// sell copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
// IN THE SOFTWARE.
//////////////////////////////////////////////////////////////////////////////

#include "oxygenshadowdemodialog.h"
#include "oxygenshadowdemodialog.moc"
#include "oxygenshadowconfiguration.h"

#include <QtGui/QDialogButtonBox>
#include <QtDBus/QDBusConnection>

#include <KPushButton>
#include <KFileDialog>

namespace Oxygen
{

    //_________________________________________________________
    ShadowDemoDialog::ShadowDemoDialog( QWidget* parent ):
        KDialog( parent ),
        _helper( "oxygen" ),
        _cache( _helper )
    {

        setWindowTitle( i18n( "Oxygen Shadow Demo" ) );

        setButtons( KDialog::Cancel|KDialog::Apply );
        button( KDialog::Apply )->setText( i18n("Save") );
        button( KDialog::Apply )->setIcon( KIcon("document-save-as") );
        button( KDialog::Apply )->setToolTip( i18n( "Save shadows as pixmaps in provided directory" ) );

        QWidget *mainWidget( new QWidget( this ) );
        ui.setupUi( mainWidget );
        setMainWidget( mainWidget );

        ui.inactiveRoundWidget->setHelper( _helper );
        ui.inactiveSquareWidget->setHelper( _helper );
        ui.activeRoundWidget->setHelper( _helper );
        ui.activeSquareWidget->setHelper( _helper );

        // reparse configuration
        reparseConfiguration();

        // customize button box
        QList<QDialogButtonBox*> children( findChildren<QDialogButtonBox*>() );
        if( !children.isEmpty() )
        {
            QDialogButtonBox* buttonBox( children.front() );

            _backgroundCheckBox = new QCheckBox( i18n( "Draw window background" ) );
            _backgroundCheckBox->setChecked( true );
            buttonBox->addButton( _backgroundCheckBox, QDialogButtonBox::ResetRole );

            connect( _backgroundCheckBox, SIGNAL(toggled(bool)), ui.inactiveRoundWidget,  SLOT(toggleBackground(bool)) );
            connect( _backgroundCheckBox, SIGNAL(toggled(bool)), ui.inactiveSquareWidget, SLOT(toggleBackground(bool)) );
            connect( _backgroundCheckBox, SIGNAL(toggled(bool)), ui.activeRoundWidget, SLOT(toggleBackground(bool)) );
            connect( _backgroundCheckBox, SIGNAL(toggled(bool)), ui.activeSquareWidget, SLOT(toggleBackground(bool)) );

        }

        // connections
        connect( button( KDialog::Apply ), SIGNAL(clicked()), SLOT(save()) );

        // use DBus connection to update on oxygen configuration change
        QDBusConnection dbus = QDBusConnection::sessionBus();
        dbus.connect( QString(), "/OxygenWindeco", "org.kde.Oxygen.Style", "reparseConfiguration", this, SLOT(reparseConfiguration()) );

    }

    //_________________________________________________________
    void ShadowDemoDialog::reparseConfiguration( void )
    {

        // read shadow configurations
        KConfig config( "oxygenrc" );
        _cache.invalidateCaches();
        _cache.setEnabled( true );
        _cache.setShadowConfiguration( ShadowConfiguration( QPalette::Inactive, KConfigGroup( &config, "InactiveShadow") ) );
        _cache.setShadowConfiguration( ShadowConfiguration( QPalette::Active, KConfigGroup( &config, "ActiveShadow") ) );

        // pass tileSets to UI
        ShadowCache::Key key;
        key.active = false;
        key.hasBorder = true;
        ui.inactiveRoundWidget->setTileSet( *_cache.tileSet( key ) );
        ui.inactiveRoundWidget->setShadowSize( _cache.shadowSize() );

        key.active = false;
        key.hasBorder = false;
        ui.inactiveSquareWidget->setTileSet( *_cache.tileSet( key ) );
        ui.inactiveSquareWidget->setShadowSize( _cache.shadowSize() );
        ui.inactiveSquareWidget->setSquare( true );

        key.active = true;
        key.hasBorder = true;
        ui.activeRoundWidget->setTileSet( *_cache.tileSet( key ) );
        ui.activeRoundWidget->setShadowSize( _cache.shadowSize() );

        key.active = true;
        key.hasBorder = false;
        ui.activeSquareWidget->setTileSet( *_cache.tileSet( key ) );
        ui.activeSquareWidget->setShadowSize( _cache.shadowSize() );
        ui.activeSquareWidget->setSquare( true );

    }

    //_________________________________________________________
    void ShadowDemoDialog::save( void )
    {

        QString dir( KFileDialog::getExistingDirectory() );
        ui.inactiveRoundWidget->tileSet().save( dir + "/shadow" );

    }
}
