
//////////////////////////////////////////////////////////////////////////////
// oxygendetectwidget.cpp
// Note: this class is a stripped down version of
// /kdebase/workspace/kwin/kcmkwin/kwinrules/detectwidget.cpp
// Copyright (c) 2004 Lubos Lunak <l.lunak@kde.org>
// -------------------
//
// Copyright (c) 2009 Hugo Pereira Da Costa <hugo.pereira@free.fr>
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

#include "oxygendetectwidget.h"
#include "oxygendetectwidget.moc"

#include <QButtonGroup>
#include <QLayout>
#include <QGroupBox>
#include <QMouseEvent>
#include <QPushButton>

#include <QX11Info>
#include <xcb/xcb.h>

namespace Oxygen
{

    //_________________________________________________________
    DetectDialog::DetectDialog( QWidget* parent ):
        QDialog( parent ),
        _grabber( 0 ),
        _wmStateAtom( 0 )
    {

        // setup
        setupUi( this );

        connect( buttonBox->button( QDialogButtonBox::Cancel ), SIGNAL(clicked()), this, SLOT(close()) );
        windowClassCheckBox->setChecked( true );

        // create atom
        xcb_connection_t* connection( QX11Info::connection() );
        const QString atomName( QStringLiteral( "WM_STATE" ) );
        xcb_intern_atom_cookie_t cookie( xcb_intern_atom( connection, false, atomName.size(), qPrintable( atomName ) ) );
        QScopedPointer<xcb_intern_atom_reply_t, QScopedPointerPodDeleter> reply( xcb_intern_atom_reply( connection, cookie, nullptr) );
        _wmStateAtom = reply ? reply->atom : 0;

    }

    //_________________________________________________________
    void DetectDialog::detect(  WId window )
    {
        if( window == 0 ) selectWindow();
        else readWindow( window );
    }

    //_________________________________________________________
    void DetectDialog::readWindow( WId window )
    {

        if( window == 0 )
        {
            emit detectionDone( false );
            return;
        }

        _info = KWindowSystem::windowInfo( window, -1U, -1U );
        if( !_info.valid())
        {
            emit detectionDone( false );
            return;
        }

        const QString wmClassClass( QString::fromUtf8( _info.windowClassClass() ) );
        const QString wmClassName( QString::fromUtf8( _info.windowClassName() ) );

        windowClass->setText( QStringLiteral( "%1 (%2 %3)" ).arg( wmClassClass ).arg( wmClassName ).arg( wmClassClass ) );
        Ui::OxygenDetectWidget::windowTitle->setText( _info.name() );
        emit detectionDone( exec() == QDialog::Accepted );

        return;

    }

    //_________________________________________________________
    void DetectDialog::selectWindow()
    {

        // use a dialog, so that all user input is blocked
        // use WX11BypassWM and moving away so that it's not actually visible
        // grab only mouse, so that keyboard can be used e.g. for switching windows
        _grabber = new QDialog( 0, Qt::X11BypassWindowManagerHint );
        _grabber->move( -1000, -1000 );
        _grabber->setModal( true );
        _grabber->show();

        // need to explicitly override cursor for Qt5
        qApp->setOverrideCursor( Qt::CrossCursor );
        _grabber->grabMouse( Qt::CrossCursor );
        _grabber->installEventFilter( this );

    }

    //_________________________________________________________
    bool DetectDialog::eventFilter( QObject* o, QEvent* e )
    {
        // check object and event type
        if( o != _grabber ) return false;
        if( e->type() != QEvent::MouseButtonRelease ) return false;

        // need to explicitely release cursor for Qt5
        qApp->restoreOverrideCursor();

        // delete old _grabber
        delete _grabber;
        _grabber = 0;

        // check button
        if( static_cast< QMouseEvent* >( e )->button() != Qt::LeftButton ) return true;

        // read window information
        readWindow( findWindow() );

        return true;
    }

    //_________________________________________________________
    WId DetectDialog::findWindow()
    {

        // check atom
        if( !_wmStateAtom ) return 0;

        xcb_connection_t* connection( QX11Info::connection() );
        xcb_window_t parent( QX11Info::appRootWindow() );

        // why is there a loop of only 10 here
        for( int i = 0; i < 10; ++i )
        {

            // query pointer
            xcb_query_pointer_cookie_t pointerCookie( xcb_query_pointer( connection, parent ) );
            QScopedPointer<xcb_query_pointer_reply_t, QScopedPointerPodDeleter> pointerReply( xcb_query_pointer_reply( connection, pointerCookie, nullptr ) );
            if( !( pointerReply && pointerReply->child ) ) return 0;

            const xcb_window_t child( pointerReply->child );
            xcb_get_property_cookie_t cookie( xcb_get_property( connection, 0, child, _wmStateAtom, XCB_GET_PROPERTY_TYPE_ANY, 0, 0 ) );
            QScopedPointer<xcb_get_property_reply_t, QScopedPointerPodDeleter> reply( xcb_get_property_reply( connection, cookie, nullptr ) );
            if( reply  && reply->type ) return child;
            else parent = child;

        }

        return 0;

    }

}
