//////////////////////////////////////////////////////////////////////////////
// oxygenclient.h
// -------------------
// Oxygen window decoration for KDE
// -------------------
// Copyright (c) 2003, 2004 David Johnson <david@usermode.org>
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

#ifndef OXYGENCLIENT_H
#define OXYGENCLIENT_H

// #include <q3button.h>
#include <qlayout.h>
//Added by qt3to4:
#include <QResizeEvent>
#include <QShowEvent>
#include <QEvent>
#include <QMouseEvent>
#include <QPaintEvent>
#include <kdecoration.h>
#include <kdecorationfactory.h>

#include "oxygenbutton.h"
// #include "oxygen.h"

class QSpacerItem;
class QPoint;

namespace Oxygen {

// class OxygenClient;
class OxygenButton;
// OxygenButton //////////////////////////////////////////////////////////////



// inline int OxygenButton::lastMousePress() const
//     { return lastmouse_; }
// 
// inline void OxygenButton::reset()
//     { repaint(); }

// OxygenClient //////////////////////////////////////////////////////////////

class OxygenClient : public KDecoration
{
    Q_OBJECT
public:
    OxygenClient(KDecorationBridge *b, KDecorationFactory *f);
    virtual ~OxygenClient();

    virtual void init();

    virtual void activeChange();
    virtual void desktopChange();
    virtual void captionChange();
    virtual void iconChange();
    virtual void maximizeChange();
    virtual void shadeChange();

    virtual void borders(int &l, int &r, int &t, int &b) const;
    virtual void resize(const QSize &size);
    virtual QSize minimumSize() const;
    virtual Position mousePosition(const QPoint &point) const;

private:
    void addButtons(QHBoxLayout* layout, const QString& buttons);

    bool eventFilter(QObject *obj, QEvent *e);
    void mouseDoubleClickEvent(QMouseEvent *e);
    void paintEvent(QPaintEvent *e);
    void showEvent(QShowEvent *);
    void doShape();
private slots:
    void maxButtonPressed();
    void menuButtonPressed();

private:
    OxygenButton *button[ButtonTypeCount];
    QSpacerItem *titlebar_;
};


} // namespace Oxygen

#endif // EXAMPLECLIENT_H
