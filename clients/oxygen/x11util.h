#ifndef X11Util_h
#define X11Util_h

//////////////////////////////////////////////////////////////////////////////
// x11util.h
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

#include <QtGui/QWidget>
#include <map>

#include <QtGui/QX11Info>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>

namespace Oxygen
{
  class X11Util
  {

    public:

    //! singleton
    static X11Util& get( void );

    //! Atoms enumeration
    enum Atoms
    {
      _NET_SUPPORTED,
      _NET_WM_STATE,
      _NET_WM_MOVERESIZE
    };

    enum Direction
    {
      _NET_WM_MOVERESIZE_SIZE_TOPLEFT=0,
      _NET_WM_MOVERESIZE_SIZE_TOP=1,
      _NET_WM_MOVERESIZE_SIZE_TOPRIGHT=2,
      _NET_WM_MOVERESIZE_SIZE_RIGHT=3,
      _NET_WM_MOVERESIZE_SIZE_BOTTOMRIGHT=4,
      _NET_WM_MOVERESIZE_SIZE_BOTTOM=5,
      _NET_WM_MOVERESIZE_SIZE_BOTTOMLEFT=6,
      _NET_WM_MOVERESIZE_SIZE_LEFT=7,
      _NET_WM_MOVERESIZE_MOVE=8,
      _NET_WM_MOVERESIZE_SIZE_KEYBOARD=9,
      _NET_WM_MOVERESIZE_MOVE_KEYBOARD=10
    };

    //! supported atoms
    bool isSupported( const Atoms& atom );

    //! move/resize widget using X11 window manager
    /*! returns true on success */
    bool moveResizeWidget( WId, int screen, QPoint, Direction, Qt::MouseButton button = Qt::LeftButton );

    //! find atom
    Atom findAtom( const Atoms& atom );

    private:

    //! constructor
    X11Util( void );

    //! atom names
    typedef std::map<Atoms, QString> AtomNameMap;

    //! atom names
    void initializeAtomNames( void );

    //! atom names
    const AtomNameMap& atomNames( void ) const
    { return atomNames_; }

    AtomNameMap atomNames_;

    //! supported atoms
    typedef std::map<Atoms, bool > SupportedAtomMap;

    //! supported atoms
    const SupportedAtomMap& supportedAtoms( void ) const
    { return supportedAtoms_; }

    SupportedAtomMap supportedAtoms_;

    //! atom map
    typedef std::map<Atoms, Atom> AtomMap;

    //! atoms
    // const AtomMap& atoms( void ) const
    AtomMap& atoms( void )
    { return atoms_; }

    //! atoms
    AtomMap atoms_;

  };

}

#endif
