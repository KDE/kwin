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

#include <QDialogButtonBox>
#include <QDBusConnection>

#include <KPushButton>
#include <KFileDialog>
#include <KLocalizedString>

namespace Oxygen
{

    //_________________________________________________________
    ShadowDemoDialog::ShadowDemoDialog( QWidget* parent ):
        QDialog( parent ),
        _helper(),
        _cache( _helper )
    {

        setWindowTitle( i18n( "Oxygen Shadow Demo" ) );
        setupUi( this );

        inactiveRoundWidget->setHelper( _helper );
        inactiveSquareWidget->setHelper( _helper );
        activeRoundWidget->setHelper( _helper );
        activeSquareWidget->setHelper( _helper );

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

            connect( _backgroundCheckBox, SIGNAL(toggled(bool)), inactiveRoundWidget,  SLOT(toggleBackground(bool)) );
            connect( _backgroundCheckBox, SIGNAL(toggled(bool)), inactiveSquareWidget, SLOT(toggleBackground(bool)) );
            connect( _backgroundCheckBox, SIGNAL(toggled(bool)), activeRoundWidget, SLOT(toggleBackground(bool)) );
            connect( _backgroundCheckBox, SIGNAL(toggled(bool)), activeSquareWidget, SLOT(toggleBackground(bool)) );

        }

        // use DBus connection to update on oxygen configuration change
        QDBusConnection dbus = QDBusConnection::sessionBus();
        dbus.connect( QString(),
            QStringLiteral( "/OxygenWindeco" ),
            QStringLiteral( "org.kde.Oxygen.Style" ),
            QStringLiteral( "reparseConfiguration" ), this, SLOT(reparseConfiguration()) );

    }

    //_________________________________________________________
    void ShadowDemoDialog::reparseConfiguration( void )
    {

        // read shadow configurations
        _cache.invalidateCaches();
        _cache.setEnabled( true );

        // pass tileSets to UI
        ShadowCache::Key key;
        key.active = false;
        key.hasBorder = true;
        inactiveRoundWidget->setTileSet( *_cache.tileSet( key ) );
        inactiveRoundWidget->setShadowSize( _cache.shadowSize() );

        key.active = false;
        key.hasBorder = false;
        inactiveSquareWidget->setTileSet( *_cache.tileSet( key ) );
        inactiveSquareWidget->setShadowSize( _cache.shadowSize() );
        inactiveSquareWidget->setSquare( true );

        key.active = true;
        key.hasBorder = true;
        activeRoundWidget->setTileSet( *_cache.tileSet( key ) );
        activeRoundWidget->setShadowSize( _cache.shadowSize() );

        key.active = true;
        key.hasBorder = false;
        activeSquareWidget->setTileSet( *_cache.tileSet( key ) );
        activeSquareWidget->setShadowSize( _cache.shadowSize() );
        activeSquareWidget->setSquare( true );

    }

}
