#ifndef oxygenconfigurationui_h
#define oxygenconfigurationui_h
//////////////////////////////////////////////////////////////////////////////
// oxygenconfigurationui.h
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

#include <kdeversion.h>
#include <KComboBox>
#include <QtGui/QWidget>
#include <QtGui/QCheckBox>
#include <QtCore/QVector>

#include "oxygenshadowconfigurationui.h"
#include "oxygenexceptionlistwidget.h"

namespace Oxygen
{

  //_____________________________________________
  class OxygenConfigurationUI: public QWidget
  {

    Q_OBJECT

    public:

    //! constructor
    OxygenConfigurationUI( QWidget* );

    //! setup ui
    void setupUI( void );

    //! title alignment
    KComboBox *titleAlignment;

    //! button size
    KComboBox* buttonSize;

    //! frame border
    KComboBox *frameBorder;

    //! blend color
    KComboBox *blendColor;

    //! size grip
    KComboBox *sizeGripMode;

    //! draw separator
    QCheckBox *drawSeparator;

    //! active window title outline
    QCheckBox *titleOutline;

    //! oxygen shadow
    QCheckBox *useOxygenShadows;

    //! about oxygen
    QPushButton *aboutOxygen;

    // shadow configuration
    QVector<OxygenShadowConfigurationUI*> shadowConfigurations;

    //! exceptions
    OxygenExceptionListWidget *exceptions;

    signals:

    //! emmited when changed
    bool changed( void );

  };

}

#endif
