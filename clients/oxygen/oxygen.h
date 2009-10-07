#ifndef oxygen_h
#define oxygen_h

//////////////////////////////////////////////////////////////////////////////
// oxygen.h
// -------------------
//
// Copyright (c) 2009 Hugo Pereira <hugo.pereira@free.fr>
// Copyright (c) 2006, 2007 Riccardo Iaconelli <ruphy@fsfe.org>
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

#include <QtCore/QObject>
#include <kdecorationfactory.h>
#include <kdeversion.h>

#include "oxygenconfiguration.h"
#include "oxygenexceptionlist.h"

class OxygenHelper;

namespace Oxygen
{

  class OxygenClient;
  class OxygenShadowCache;

  //! helper singleton
  OxygenHelper* oxygenHelper();

  //! shadow cache singleton
  OxygenShadowCache* oxygenShadowCache();

  //! buttons
  enum ButtonType
  {
    ButtonHelp=0,
    ButtonMax,
    ButtonMin,
    ButtonClose,
    ButtonMenu,
    ButtonSticky,
    ButtonAbove,
    ButtonBelow,
    ButtonShade,
    ButtonTypeCount
  };

  #if !KDE_IS_VERSION(4,3,90)
  enum{ SettingCompositing = 1 << 6 };
  #endif

  //! maximum index/frame used for animations
  static const int maxAnimationIndex = 256;

  /*
  If non zero, this possibly allow one to have an additional space
  around window that is clickable although it is part of the shadow
  */
  static const int EXTENDED_HITAREA = 0;

  // this is the top title bar edge
  static const int TFRAMESIZE = 3;

  // this is the extra title bar top and bottom edges
  // needed to outline active window title bar
  static const int HFRAMESIZE = 4;

  Q_DECLARE_FLAGS(ButtonTypes, ButtonType)

  //! window decoration factory
  class OxygenFactory: public QObject, public KDecorationFactoryUnstable
  {

    Q_OBJECT

    public:

    //! constructor
    OxygenFactory();

    //! destructor
    virtual ~OxygenFactory();

    //! create decoration
    virtual KDecoration *createDecoration(KDecorationBridge *b);

    //! configuration reset
    virtual bool reset(unsigned long changed);

    //! configuration capabilities
    virtual bool supports( Ability ability ) const;

    //! true if initialized
    static bool initialized()
    { return initialized_; }

    //! get configuration for a give client
    static OxygenConfiguration configuration( const OxygenClient& );

    signals:

    //! configuration has changed
    void configurationChanged( void );

    private:

    //! read configuration from KConfig
    bool readConfig();

    //! default configuration
    static OxygenConfiguration defaultConfiguration( void )
    { return defaultConfiguration_; }

    //! initialization
    static void setInitialized( bool value )
    { initialized_ = value; }

    //! set default configuration
    static void setDefaultConfiguration( OxygenConfiguration value )
    { defaultConfiguration_ = value; }

    //! initialization flag
    static bool initialized_;

    //! default configuration
    static OxygenConfiguration defaultConfiguration_;

    //! exceptions
    static OxygenExceptionList exceptions_;

  };

}

#endif
