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

#ifndef KNIFTYCLIENT_H
#define KNIFTYCLIENT_H

#include <kdecoration.h>
#include "plastik.h"

class QSpacerItem;
class QVBoxLayout;
class QBoxLayout;

namespace KWinPlastik {

class PlastikButton;

class PlastikClient : public KDecoration
{
    Q_OBJECT
public:
    PlastikClient(KDecorationBridge* bridge, KDecorationFactory* factory);
    ~PlastikClient();

    virtual void init();

    virtual void borders( int& left, int& right, int& top, int& bottom ) const;
    virtual void resize(const QSize&);
    virtual QSize minimumSize() const;
    virtual void show();
    virtual bool eventFilter( QObject* o, QEvent* e );

    QPixmap getTitleBarTile(bool active) const
    {
        return active ? *aTitleBarTile : *iTitleBarTile;
    }
protected:
    virtual void resizeEvent();
    virtual void paintEvent(QPaintEvent *e);
    virtual void showEvent(QShowEvent *);
    virtual void mouseDoubleClickEvent(QMouseEvent *e);

    virtual void maximizeChange();
    virtual void desktopChange();
    virtual void shadeChange();
    virtual void doShape();

    virtual void reset( unsigned long changed );

    virtual void captionChange();
    virtual void iconChange();
    virtual void activeChange();
    virtual Position mousePosition(const QPoint &point) const;

private slots:
    void keepAboveChange(bool above);
    void keepBelowChange(bool below);
    void slotMaximize();
    void slotShade();
    void slotKeepAbove();
    void slotKeepBelow();
    void menuButtonPressed();
    void menuButtonReleased();
    bool isTool();
private:
    void _resetLayout();
    void addButtons(QBoxLayout* layout, const QString& buttons, int buttonSize = 18);

    QVBoxLayout *mainLayout_;
    QSpacerItem *topSpacer_,
                *titleSpacer_,
                *leftTitleSpacer_, *rightTitleSpacer_,
                *decoSpacer_,
                *leftSpacer_, *rightSpacer_,
                *bottomSpacer_;

    QPixmap *aCaptionBuffer, *iCaptionBuffer;
    void update_captionBuffer();

    QPixmap *aTitleBarTile, *iTitleBarTile, *aTitleBarTopTile, *iTitleBarTopTile;
    bool pixmaps_created;
    void create_pixmaps();
    void delete_pixmaps();

    PlastikButton *m_button[NumButtons];

    bool captionBufferDirty;

    bool closing;

    // settings...
    int   s_titleHeight;
    QFont s_titleFont;
};

} // KWinPlastik

#endif // KNIFTCLIENT_H
