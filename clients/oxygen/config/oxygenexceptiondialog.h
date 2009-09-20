#ifndef oxygenexceptiondialog_h
#define oxygenexceptiondialog_h
//////////////////////////////////////////////////////////////////////////////
// oxygenexceptiondialog.h
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

#include <KDialog>
#include <KLineEdit>
#include <KComboBox>
#include <QtGui/QCheckBox>

#include <map>

#include "../oxygenexception.h"

namespace Oxygen
{

  class DetectDialog;

  //! oxygen exceptions list
  class OxygenExceptionDialog: public KDialog
  {

    Q_OBJECT

    public:

    //! constructor
    OxygenExceptionDialog( QWidget* parent );

    //! set exception
    void setException( OxygenException );

    //! get exception
    OxygenException exception( void ) const;

    private slots:

    void selectWindowProperties( void );

    void readWindowProperties( bool );

    private:

    //! line editor
    KLineEdit* exceptionEditor;

    //! blend combobox
    KComboBox* exceptionType;

    //! border size
    KComboBox* frameBorder;

    //! blend combobox
    KComboBox* blendColor;

    //! size grip
    KComboBox* sizeGripMode;

    //! map mask and checkbox
    typedef std::map< OxygenException::AttributesMask, QCheckBox*> CheckBoxMap;

    //! map mask and checkbox
    CheckBoxMap checkboxes_;

    //! internal exception
    OxygenException exception_;

    //! local combobox to handle configuration checkboxes
    class ComboBox: public KComboBox
    {
      public:

      static const QString Yes;
      static const QString No;

      //! constructor
      ComboBox( QWidget* parent );

      //! set value
      void setValue( bool checked );

      //! true if yes is checkd
      bool isChecked( void ) const;


    };

    //! overwrite colors
    ComboBox* titleOutline;

    //! draw separator
    ComboBox* drawSeparator;

    //! detection dialog
    DetectDialog* detectDialog;

  };

}

#endif
