/* This file is part of the KDE libraries

   Copyright (C) 2009 Marco Martin <notmart@gmail.com>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public License
   along with this library; see the file COPYING.LIB.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/

#ifndef SCREENPREVIEWWIDGET_H
#define SCREENPREVIEWWIDGET_H

#include <QWidget>

class ScreenPreviewWidgetPrivate;

class ScreenPreviewWidget : public QWidget
{
    Q_OBJECT

public:
    ScreenPreviewWidget(QWidget *parent);
    ~ScreenPreviewWidget() override;

    void setPreview(const QPixmap &preview);
    const QPixmap preview() const;
    void setRatio(const qreal ratio);
    qreal ratio() const;

    QRect previewRect() const;

protected:
    void resizeEvent(QResizeEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    void dropEvent(QDropEvent *event) override;

Q_SIGNALS:
    void imageDropped(const QString &);

private:
    ScreenPreviewWidgetPrivate *const d;

    Q_PRIVATE_SLOT(d, void updateRect(const QRectF& rect))
};


#endif
