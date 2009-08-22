#ifndef nitrogen_config_h
#define nitrogen_config_h
// $Id: config.h,v 1.7 2009/06/24 01:43:14 hpereira Exp $

/*
 * Nitrogen KWin client configuration module
 *
 * Copyright (C) 2008 Lubos Lunak <l.lunak@kde.org>
 *
 * Based on the Quartz configuration module,
 *     Copyright (c) 2001 Karol Szwed <gallium@kde.org>
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the license, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include <KConfig>

#include "nitrogenconfigurationui.h"
#include "../nitrogenexceptionlist.h"

namespace Nitrogen {

  class NitrogenConfiguration;
  
  // nitrogen configuration object
  class Config: public QObject
  {
    
    Q_OBJECT
    
    public:
      
    //! constructor
    Config( KConfig* conf, QWidget* parent );
  
    //! destructor
    ~Config();
    
    signals:
    
    //! emmited whenever configuration is changed
    void changed();
    
    public slots:
    
    //! load configuration
    void load( const KConfigGroup& conf );	
    
    //! save configuration
    void save( KConfigGroup& conf );
    
    //! restore defaults
    void defaults();
    
    private slots:
    
    //! raise exception list 
    void showExceptions( void );
    
    //! about nitrogen
    void aboutNitrogen( void );
    
    private:
    
    //! load configuration
    void loadConfiguration( const NitrogenConfiguration& );
    
    //! user interface
    NitrogenConfigurationUI *user_interface_;
    
    //! kconfiguration object
    KConfig *configuration_;
    
    //! internal list of exceptions
    NitrogenExceptionList exceptions_;
    
  };
  
} //namespace Nitrogen

#endif
