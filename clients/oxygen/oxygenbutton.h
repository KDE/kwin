//////////////////////////////////////////////////////////////////////////////
// oxygenbutton.h
// -------------------
// Oxygen window decoration for KDE. Buttons.
// -------------------
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
// class QAbstractButton;
#ifndef OXYGENBUTTON_H
#define OXYGENBUTTON_H

#include <QAbstractButton>
class QPainterPath;

#include "oxygen.h"

namespace Oxygen
{
class OxygenClient;

enum ButtonStatus {
    Normal,
    Hovered,
    Pressed };

Q_DECLARE_FLAGS(ButtonState, ButtonStatus);

class OxygenButton : public QAbstractButton
{
public:
    OxygenButton(OxygenClient *parent=0,
                  const QString &tip=NULL,
                  ButtonType type=ButtonHelp,
                  const unsigned char *bitmap=0);
    ~OxygenButton();

    void setBitmap(const unsigned char *bitmap);
    QSize sizeHint() const;
    const int lastMousePress(){return lastmouse_;};
    void reset(){repaint();};

private:
    void enterEvent(QEvent *e);
    void leaveEvent(QEvent *e);
    void paintEvent(QPaintEvent *e);

private Q_SLOTS:
    void pressSlot();

private:
    OxygenClient *client_;
    ButtonType type_;
    ButtonState status_;
    QBitmap *deco_;
    int lastmouse_;
};

} //namespace Oxygen
#endif
