#ifndef oxygendetectwidget_h
#define oxygendetectwidget_h

//////////////////////////////////////////////////////////////////////////////
// oxygendetectwidget.h
// Note: this class is a stripped down version of
// /kdebase/workspace/kwin/kcmkwin/kwinrules/detectwidget.h
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

#include <map>
#include <KDialog>
#include <QEvent>
#include <QByteArray>
#include <QCheckBox>
#include <QLabel>

#include <kwindowsystem.h>

#include "../oxygenexception.h"

namespace Oxygen
{

  class DetectWidget: public QWidget
  {

    public:

      //! constructor
      DetectWidget( QWidget* );

    //! window class
    void setWindowClass( QString value )
    { windowClass->setText( value ); }

    //! window title
    void setWindowTitle( QString value )
    { windowTitle->setText( value ); }

    //! window machine
    void setWindowMachine( QString value )
    { windowMachine->setText( value ); }

    //! type
    OxygenException::Type exceptionType( void ) const;

    private:

    QLabel* windowClass;
    QLabel* windowTitle;
    QLabel* windowMachine;

    // map checkboxes against exception type
    typedef std::map<QCheckBox*, OxygenException::Type > CheckBoxMap;
    CheckBoxMap checkboxes;

  };

  class DetectDialog : public KDialog
  {

    Q_OBJECT

    public:

    //! constructor
    DetectDialog( QWidget* );

    //! read window properties or select one from mouse grab
    void detect( WId window );

    //! selected class
    QByteArray selectedClass() const;

    //! window information
    const KWindowInfo& windowInfo() const
    { return info; }

    //! exception type
    OxygenException::Type exceptionType() const
    { return widget->exceptionType(); }

    signals:

    void detectionDone( bool );

    protected:

    virtual bool eventFilter( QObject* o, QEvent* e );

    private:

    //! select window from grab
    void selectWindow();

    //! read window properties
    void readWindow( WId window );

    //! find window under cursor
    WId findWindow();

    //! execute
    void executeDialog( void );

    //! window machine
    QString machine;

    //! main widget
    DetectWidget* widget;

    //! invisible dialog used to grab mouse
    KDialog* grabber;

    //! current window information
    KWindowInfo info;

  };

} // namespace

#endif
