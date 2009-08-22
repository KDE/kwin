#ifndef X11Util_h
#define X11Util_h

// $Id: x11util.h,v 1.4 2009/07/03 00:36:20 hpereira Exp $

/******************************************************************************
*
* Copyright (C) 2002 Hugo PEREIRA <mailto: hugo.pereira@free.fr>
*
* This is free software; you can redistribute it and/or modify it under the
* terms of the GNU General Public License as published by the Free Software
* Foundation; either version 2 of the License, or (at your option) any later
* version.
*
* This software is distributed in the hope that it will be useful, but WITHOUT
* ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
* FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
* for more details.
*
* You should have received a copy of the GNU General Public License along with
* software; if not, write to the Free Software Foundation, Inc., 59 Temple
* Place, Suite 330, Boston, MA  02111-1307 USA
*
*
*******************************************************************************/

/*!
  \file    X11Util.h
  \brief   some X11 specific utilities
  \author  Hugo Pereira
  \version $Revision: 1.4 $
  \date    $Date: 2009/07/03 00:36:20 $
*/

#include <QWidget>
#include <map>

#ifdef Q_WS_X11
#include <QX11Info>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#endif

namespace Nitrogen 
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
      _NET_WM_MOVERESIZE_MOVE_KEYBOARD=10,    
    };
      
    //! supported atoms
    bool isSupported( const Atoms& atom );
     
    //! move/resize widget using X11 window manager
    /*! returns true on success */
    bool moveResizeWidget( WId, int screen, QPoint, Direction, Qt::MouseButton button = Qt::LeftButton );
    
    #ifdef Q_WS_X11
    
    //! find atom
    Atom findAtom( const Atoms& atom );
    
    #endif
    
    private:
    
    //! constructor
    X11Util( void );
    
    //! atom names
    typedef std::map<Atoms, QString> AtomNameMap; 
    
    //! atom names
    void _initializeAtomNames( void );
    
    //! atom names
    const AtomNameMap& _atomNames( void ) const
    { return atom_names_; }
    
    AtomNameMap atom_names_;
    
    //! supported atoms
    typedef std::map<Atoms, bool > SupportedAtomMap;
    
    //! supported atoms
    const SupportedAtomMap& _supportedAtoms( void ) const
    { return supported_atoms_; }
    
    SupportedAtomMap supported_atoms_;
    
    #ifdef Q_WS_X11
    
    //! atom map
    typedef std::map<Atoms, Atom> AtomMap;
    
    //! atoms
    // const AtomMap& _atoms( void ) const
    AtomMap& _atoms( void )
    { return atoms_; }
    
    //! atoms
    AtomMap atoms_;
    
    #endif
    
  };
  
};

#endif
