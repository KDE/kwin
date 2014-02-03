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

#include "screenpreviewwidget.h"

#include <QResizeEvent>
#include <QPaintEvent>
#include <QPainter>

#include <QDebug>
#include <QMimeData>

#include <Plasma/FrameSvg>
#include <kurlmimedata.h>


class ScreenPreviewWidgetPrivate
{
public:
    ScreenPreviewWidgetPrivate(ScreenPreviewWidget *screen)
          : q(screen),
            ratio(1)
    {}

    ~ScreenPreviewWidgetPrivate()
    {}

    void updateRect(const QRectF& rect)
    {
        q->update(rect.toRect());
    }

    void updateScreenGraphics()
    {
        int bottomElements = screenGraphics->elementSize("base").height() + screenGraphics->marginSize(Plasma::Types::BottomMargin);
        QRect bounds(QPoint(0,0), QSize(q->size().width(), q->height() - bottomElements));

        QSize monitorSize(q->size().width(), q->size().width()/ratio);
        monitorSize.scale(bounds.size(), Qt::KeepAspectRatio);

        if (monitorSize.isEmpty()) {
            return;
        }

        monitorRect = QRect(QPoint(0,0), monitorSize);
        monitorRect.moveCenter(bounds.center());

        screenGraphics->resizeFrame(monitorRect.size());

        previewRect = screenGraphics->contentsRect().toRect();
        previewRect.moveCenter(bounds.center());
    }

    ScreenPreviewWidget *q;
    Plasma::FrameSvg *screenGraphics;
    QPixmap preview;
    QRect monitorRect;
    qreal ratio;
    QRect previewRect;
};

ScreenPreviewWidget::ScreenPreviewWidget(QWidget *parent)
    : QWidget(parent),
      d(new ScreenPreviewWidgetPrivate(this))
{
    d->screenGraphics = new Plasma::FrameSvg(this);
    d->screenGraphics->setImagePath("widgets/monitor");
    d->updateScreenGraphics();
}

ScreenPreviewWidget::~ScreenPreviewWidget()
{
   delete d;
}

void ScreenPreviewWidget::setPreview(const QPixmap &preview)
{
    d->preview = preview;

    update();
}

const QPixmap ScreenPreviewWidget::preview() const
{
    return d->preview;
}

void ScreenPreviewWidget::setRatio(const qreal ratio)
{
    d->ratio = ratio;
    d->updateScreenGraphics();
}

qreal ScreenPreviewWidget::ratio() const
{
    return d->ratio;
}

QRect ScreenPreviewWidget::previewRect() const
{
    return d->previewRect;
}

void ScreenPreviewWidget::resizeEvent(QResizeEvent *e)
{
    Q_UNUSED(e)
    d->updateScreenGraphics();
}

void ScreenPreviewWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)
    if (d->monitorRect.size().isEmpty()) {
        return;
    }

    QPainter painter(this);
    QPoint standPosition(d->monitorRect.center().x() - d->screenGraphics->elementSize("base").width()/2, d->previewRect.bottom());

    d->screenGraphics->paint(&painter, QRect(standPosition, d->screenGraphics->elementSize("base")), "base");
    d->screenGraphics->paintFrame(&painter, d->monitorRect.topLeft());

    painter.save();
    if (!d->preview.isNull()) {
        painter.setRenderHint(QPainter::SmoothPixmapTransform);
        painter.drawPixmap(d->previewRect, d->preview, d->preview.rect());
    }
    painter.restore();

    d->screenGraphics->paint(&painter, d->previewRect, "glass");
}

void ScreenPreviewWidget::dropEvent(QDropEvent *e)
{
    if (!e->mimeData()->hasUrls())
        return;

    QList<QUrl> uris(KUrlMimeData::urlsFromMimeData(e->mimeData()));
    if (!uris.isEmpty()) {
        // TODO: Download remote file
        if (uris.first().isLocalFile())
           emit imageDropped(uris.first().path());
    }
}

#include "moc_screenpreviewwidget.cpp"
