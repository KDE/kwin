/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2011 Martin Gräßlin <mgraesslin@kde.org>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/
#ifndef KWIN_TABBOX_DECLARATIVE_H
#define KWIN_TABBOX_DECLARATIVE_H
// includes
#include <QtDeclarative/QDeclarativeImageProvider>
#include <QtDeclarative/QDeclarativeView>
#include "tabboxconfig.h"

// forward declaration
class QAbstractItemModel;
class QModelIndex;
class QPos;

namespace Plasma
{
class FrameSvg;
}

namespace KWin
{
namespace TabBox
{

class ImageProvider : public QDeclarativeImageProvider
{
public:
    ImageProvider(QAbstractItemModel *model);
    virtual QPixmap requestPixmap(const QString &id, QSize *size, const QSize &requestedSize);

private:
    QAbstractItemModel *m_model;
};

class DeclarativeView : public QDeclarativeView
{
    Q_OBJECT
public:
    DeclarativeView(QAbstractItemModel *model, TabBoxConfig::TabBoxMode mode, QWidget *parent = NULL);
    virtual void showEvent(QShowEvent *event);
    virtual void resizeEvent(QResizeEvent *event);
    void setCurrentIndex(const QModelIndex &index, bool disableAnimation = false);
    bool sendKeyEvent(QKeyEvent *event);

protected:
    virtual void hideEvent(QHideEvent *event);
    virtual bool x11Event(XEvent *e);

public Q_SLOTS:
    void slotUpdateGeometry();
    void slotEmbeddedChanged(bool enabled);
private Q_SLOTS:
    void updateQmlSource(bool force = false);
    void currentIndexChanged(int row);
    void slotWindowChanged(WId wId, unsigned int properties);
private:
    QAbstractItemModel *m_model;
    TabBoxConfig::TabBoxMode m_mode;
    QRect m_currentScreenGeometry;
    /**
    * Background Frame required for setting the blur mask
    */
    Plasma::FrameSvg* m_frame;
    QString m_currentLayout;
    int m_cachedWidth;
    int m_cachedHeight;
    //relative position to the embedding window
    QPoint m_relativePos;
};

} // namespace TabBox
} // namespace KWin
#endif
