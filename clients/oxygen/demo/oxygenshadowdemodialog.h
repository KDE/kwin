#ifndef oxygenshadowdemodialog_h
#define oxygenshadowdemodialog_h

//////////////////////////////////////////////////////////////////////////////
// oxygenshadowdemodialog.h
// oxygen shadow demo dialog
// -------------------
//
// Copyright (c) 2010 Hugo Pereira Da Costa <hugo@oxygen-icons.org>
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

#include "ui_oxygenshadowdemo.h"
#include "../oxygendecohelper.h"
#include "../oxygenshadowcache.h"

#include <KDialog>
#include <QtGui/QCheckBox>

namespace Oxygen
{
    class ShadowDemoDialog: public KDialog
    {

        Q_OBJECT

        public:

        //! constructor
        explicit ShadowDemoDialog( QWidget* = 0 );

        //! destructor
        virtual ~ShadowDemoDialog( void )
        {}

        protected slots:

        //! reparse configuration
        void reparseConfiguration( void );

        //! save
        void save( void );

        private:

        //! ui
        Ui_ShadowDemo ui;

        //! helper
        DecoHelper _helper;

        //! shadow cache
        ShadowCache _cache;

        //! enable state checkbox
        QCheckBox* _backgroundCheckBox;

    };

}

#endif
