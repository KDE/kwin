/* Plastik KWin window decoration
  Copyright (C) 2003 Sandro Giessl <ceebx@users.sourceforge.net>

  based on the window decoration "Web":
  Copyright (C) 2001 Rik Hemsley (rikkus) <rik@kde.org>

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public
  License as published by the Free Software Foundation; either
  version 2 of the License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; see the file COPYING.  If not, write to
  the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
  Boston, MA 02111-1307, USA.
 */

#ifndef PLASTIKBUTTON_H
#define PLASTIKBUTTON_H

#include <qbutton.h>
#include <qimage.h>
#include "plastik.h"

class QTimer;

namespace KWinPlastik {

class PlastikClient;

class PlastikButton : public QButton
{
    Q_OBJECT
public:
    PlastikButton(PlastikClient *parent, const char *name, const QString &tip, ButtonType type, int size, bool toggle = false, int btns = LeftButton);
    ~PlastikButton();

    QSize sizeHint() const; ///< Return size hint.
    int lastMousePress() const { return m_lastMouse; }
    void reset() { repaint(false); }
    PlastikClient * client() { return m_client; }
    virtual void setOn(bool on);
    void setDeco();
    void setTipText(const QString &tip);
    void setSize(const int s);

protected slots:
    void animate();

private:
    void enterEvent(QEvent *e);
    void leaveEvent(QEvent *e);
    void mousePressEvent(QMouseEvent *e);
    void mouseReleaseEvent(QMouseEvent *e);
    void drawButton(QPainter *painter);

private:
    PlastikClient *m_client;
    int m_lastMouse;
    int m_realizeButtons;

    int m_size;

    ButtonType m_type;
    QImage m_aDecoLight,m_iDecoLight,m_aDecoDark,m_iDecoDark;
    bool hover;

    QTimer *animTmr;
    uint animProgress;
};

} // namespace KWinPlastik

#endif // PLASTIKBUTTON_H
