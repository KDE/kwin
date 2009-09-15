//////////////////////////////////////////////////////////////////////////////
// x11util.cpp
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

#include "x11util.h"

using namespace std;

namespace Oxygen
{

  //________________________________________________________________________
  X11Util& X11Util::get( void )
  {
    static X11Util singleton;
    return singleton;
  }

  //________________________________________________________________________
  X11Util::X11Util( void )
  {
    initializeAtomNames();
  }

  //________________________________________________________________________
  bool X11Util::isSupported( const Atoms& atom )
  {

    #ifdef Q_WS_X11

    SupportedAtomMap::const_iterator iter( supportedAtoms().find( atom ) );
    if( iter != supportedAtoms().end() ) return iter->second;

    Display* display( QX11Info::display() );
    Atom net_supported( findAtom( _NET_SUPPORTED ) );
    Atom searched( findAtom( atom ) );

    Atom actual;
    int format;
    unsigned char *data;
    unsigned long offset = 0;

    while( 1 )
    {
      unsigned long n, left;
      XGetWindowProperty( display, QX11Info::appRootWindow(),
        net_supported, offset, 1L,
        false, XA_ATOM, &actual,
        &format, &n, &left,
        (unsigned char **) &data);

      if( data == None ) break;

      // try cast data to atom
      Atom found( *(Atom*)data );

      if( found == searched )
      {
        supportedAtoms_[atom] = true;
        return true;
      }

      if( !left ) break;
      else offset ++;

    }

    supportedAtoms_[atom] = false;

    #endif

    return false;

  }

  //________________________________________________________________________
  bool X11Util::moveResizeWidget(
    WId id,
    int screen,
    QPoint position,
    X11Util::Direction direction,
    Qt::MouseButton button )
  {

    #ifdef Q_WS_X11

    // check
    if( !isSupported( _NET_WM_MOVERESIZE ) ) return false;

    Display* display( QX11Info::display() );
    Atom net_wm_moveresize( findAtom( _NET_WM_MOVERESIZE ) );

    XEvent event;
    event.xclient.type = ClientMessage;
    event.xclient.message_type = net_wm_moveresize;
    event.xclient.display = display;
    event.xclient.window = id;
    event.xclient.format = 32;
    event.xclient.data.l[0] = position.x();
    event.xclient.data.l[1] = position.y();
    event.xclient.data.l[2] = direction;
    event.xclient.data.l[3] = button;
    event.xclient.data.l[4] = 0;
    XUngrabPointer( display, QX11Info::appTime() );
    XSendEvent(display,
      QX11Info::appRootWindow( screen ),
      false,
      SubstructureRedirectMask | SubstructureNotifyMask, &event);
    return true;

    #else
    return false;
    #endif
  }

  //________________________________________________________________________
  void X11Util::initializeAtomNames( void )
  {

    atomNames_[_NET_SUPPORTED] = "_NET_SUPPORTED";
    atomNames_[_NET_WM_STATE] = "_NET_WM_STATE";
    atomNames_[_NET_WM_MOVERESIZE] = "_NET_WM_MOVERESIZE";

    return;
  }

  #ifdef Q_WS_X11

  //________________________________________________________________________
  Atom X11Util::findAtom( const Atoms& atom )
  {

    // find atom in map
    AtomMap::iterator iter( atoms().find( atom ) );
    if( iter != atoms().end() ) return iter->second;

    // create atom if not found
    Display* display( QX11Info::display() );
    Atom out( XInternAtom(display, qPrintable( atomNames_[atom] ), false ) );
    atoms_[atom] = out;
    return out;

  }

}

#endif
