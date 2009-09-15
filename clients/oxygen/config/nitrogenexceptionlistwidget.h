#ifndef _NitrogenExceptionListWidget_h_
#define _NitrogenExceptionListWidget_h_
//////////////////////////////////////////////////////////////////////////////
// NitrogenExceptionListWidget.h
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

#include <KPushButton>
#include <QTreeView>
#include <KDialog>

#include "nitrogenexceptionmodel.h"
#include "../nitrogenexceptionlist.h"

//! QDialog used to commit selected files
namespace Nitrogen
{

  class NitrogenExceptionListWidget: public QWidget
  {

    //! Qt meta object
    Q_OBJECT

    public:

    //! constructor
    NitrogenExceptionListWidget( QWidget* = 0, NitrogenConfiguration default_configuration = NitrogenConfiguration() );

    //! set exceptions
    void setExceptions( const NitrogenExceptionList& );

    //! get exceptions
    NitrogenExceptionList exceptions( void ) const;

    signals:

    //! emited when list is changed
    void changed( void );

    protected:

    //! list
    QTreeView& _list() const
    { return *list_; }

    //! model
    const NitrogenExceptionModel& _model() const
    { return model_; }

    //! model
    NitrogenExceptionModel& _model()
    { return model_; }

    protected slots:

    //! update button states
    virtual void _updateButtons( void );

    //! add
    virtual void _add( void );

    //! edit
    virtual void _edit( void );

    //! remove
    virtual void _remove( void );

    //! toggle
    virtual void _toggle( const QModelIndex& );

    //! move up
    virtual void _up( void );

    //! move down
    virtual void _down( void );

    private:

    //! resize columns
    void _resizeColumns( void ) const;

    //! check exception
    bool _checkException( NitrogenException& );

    //! default configuration
    NitrogenConfiguration default_configuration_;

    //! list of files
    QTreeView* list_;

    //! model
    NitrogenExceptionModel model_;

    //! add
    KPushButton* add_button_;

    //! edit
    KPushButton* edit_button_;

    //! remove
    KPushButton* remove_button_;

    //! move up
    KPushButton* up_button_;

    //! move down
    KPushButton* down_button_;

  };

}

#endif
