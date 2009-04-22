/*****************************************************************
This file is part of the KDE project.

Copyright (C) 2009 Lubos Lunak <l.lunak@kde.org>

Permission is hereby granted, free of charge, to any person obtaining a
copy of this software and associated documentation files (the "Software"),
to deal in the Software without restriction, including without limitation
the rights to use, copy, modify, merge, publish, distribute, sublicense,
and/or sell copies of the Software, and to permit persons to whom the
Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
DEALINGS IN THE SOFTWARE.
******************************************************************/

#ifndef PAINTREDIRECTOR_H
#define PAINTREDIRECTOR_H

#include <qregion.h>
#include <qtimer.h>
#include <qwidget.h>

namespace KWin
{

// This class redirects all painting of a given widget (including its children)
// into a paint device (QPixmap).
class PaintRedirector
    : public QObject
    {
    Q_OBJECT
    public:
        PaintRedirector( QWidget* widget );
        QPixmap performPendingPaint();
        virtual bool eventFilter( QObject* o, QEvent* e );
        QRegion pendingRegion() const;
    signals:
        void paintPending();
    private:
        void added( QWidget* widget );
        void removed( QWidget* widget );
        bool isToolTip( QWidget* widget ) const;
        QWidget* widget;
        QRegion pending;
        bool recursionCheck;
        QTimer timer;
    };

} // namespace

#endif
