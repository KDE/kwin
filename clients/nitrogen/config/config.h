#ifndef nitrogen_config_h
#define nitrogen_config_h

//////////////////////////////////////////////////////////////////////////////
// config.h
// -------------------
//
// Copyright (c) 2009 Hugo Pereira Da Costa <hugo.pereira@free.fr>
// Copyright (C) 2008 Lubos Lunak <l.lunak@kde.org>
//
// Based on the Quartz configuration module,
//     Copyright (c) 2001 Karol Szwed <gallium@kde.org>
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

#include <KConfig>
#include <QPalette>

#include "nitrogenconfigurationui.h"
#include "../nitrogenexceptionlist.h"

namespace Nitrogen {

  class NitrogenConfiguration;
  class NitrogenShadowConfiguration;

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

    //! about nitrogen
    void aboutNitrogen( void );

    private:

    //! load configuration
    void loadConfiguration( const NitrogenConfiguration& );

    //! load configuration
    void loadShadowConfiguration( QPalette::ColorGroup, const NitrogenShadowConfiguration& );

    //! load configuration
    void saveShadowConfiguration( QPalette::ColorGroup, const NitrogenShadowConfigurationUI& ) const;

    //! user interface
    NitrogenConfigurationUI *userInterface_;

    //! kconfiguration object
    KConfig *configuration_;

  };

} //namespace Nitrogen

#endif
