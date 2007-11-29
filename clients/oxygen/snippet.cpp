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

class OxygenWidget : public QWidget
{
protected:
    virtual void paintEvent ( QPaintEvent * event );
};

void OxygenWidget::paintEvent ( QPaintEvent * )
{
    QPainter painter(this);

    QRadialGradient grad1(QPointF(5.0, 5.0),5.0);
    grad1.setColorAt(0.9, QColor(0,0,0,100));
    grad1.setColorAt(1.0, QColor(0,0,0,0));
    QRadialGradient grad2(QPointF(5.0, 5.0),5.0);
    grad2.setColorAt(0.0, QColor(235,235,235));
    grad2.setColorAt(1.0, QColor(220,220,220));
    QRadialGradient grad3(QPointF(5.0, 3.75), 3.5,QPointF(5.0, 2.5));
    grad3.setColorAt(0, QColor(255,255,255,50));
    grad3.setColorAt(1, QColor(255,255,255,0));
    QRadialGradient grad4(QPointF(5.0, 3.3), 3.5, QPointF(5.0, 2.1));
    grad4.setColorAt(0, QColor(255,255,255,50));
    grad4.setColorAt(1, QColor(255,255,255,0));
    QBrush brush1(grad1);
    QBrush brush2(grad2);
    QBrush brush3(grad3);
    QBrush brush4(grad4);

    painter.scale(1.6, 1.6);
    painter.setRenderHint(QPainter::Antialiasing,true);

    QPainterPath path1;
    path1.addEllipse(0.0, 0.0, 10.0, 10.0);
    painter.fillPath(path1, brush1);

    QPainterPath path2;
    path2.addEllipse(0.5, 0.5, 9.0, 9.0);
    painter.fillPath(path2, brush2);

    QPainterPath path3;
    path3.addEllipse(1.5, 0.5, 7.0, 6.0);
    painter.fillPath(path3, brush3);

    QPainterPath path4;
    path4.addEllipse(1.5, 0.5, 7.0, 6.0);
    painter.fillPath(path4, brush4);
}
