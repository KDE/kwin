/*
    SPDX-FileCopyrightText: 2009 Marco Martin <notmart@gmail.com>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "screenpreviewwidget.h"

#include <QPaintEvent>
#include <QPainter>
#include <QResizeEvent>

#include <QDebug>
#include <QMimeData>

#include <Plasma/FrameSvg>
#include <kurlmimedata.h>

class ScreenPreviewWidgetPrivate
{
public:
    ScreenPreviewWidgetPrivate(ScreenPreviewWidget *screen)
        : q(screen)
        , ratio(1)
        , minimumContentWidth(0)
    {
    }

    ~ScreenPreviewWidgetPrivate()
    {
    }

    void updateRect(const QRectF &rect)
    {
        q->update(rect.toRect());
    }

    void updateScreenGraphics()
    {
        int bottomElements = screenGraphics->elementSize("base").height() + screenGraphics->marginSize(Plasma::Types::BottomMargin);
        QRect bounds(QPoint(0, 0), QSize(q->width(), q->height() - bottomElements));

        QSizeF monitorSize(1.0, 1.0 / ratio);
        monitorSize.scale(bounds.size(), Qt::KeepAspectRatio);

        if (monitorSize.isEmpty()) {
            return;
        }

        const auto minFrameWidth = minimumContentWidth + screenGraphics->marginSize(Plasma::Types::LeftMargin) + screenGraphics->marginSize(Plasma::Types::RightMargin);
        if (monitorSize.width() < minFrameWidth) {
            monitorSize.setWidth(minFrameWidth);
        }

        monitorRect = QRect(QPoint(0, 0), monitorSize.toSize());
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
    qreal minimumContentWidth;
    QRect previewRect;
};

ScreenPreviewWidget::ScreenPreviewWidget(QWidget *parent)
    : QWidget(parent)
    , d(std::make_unique<ScreenPreviewWidgetPrivate>(this))
{
    d->screenGraphics = new Plasma::FrameSvg(this);
    d->screenGraphics->setImagePath("widgets/monitor");
    d->updateScreenGraphics();
}

ScreenPreviewWidget::~ScreenPreviewWidget() = default;

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

void ScreenPreviewWidget::setMinimumContentWidth(const qreal minw)
{
    d->minimumContentWidth = minw;
    d->updateScreenGraphics();
}

qreal ScreenPreviewWidget::minimumContentWidth() const
{
    return d->minimumContentWidth;
}

QRect ScreenPreviewWidget::previewRect() const
{
    return d->previewRect;
}

void ScreenPreviewWidget::resizeEvent(QResizeEvent *e)
{
    d->updateScreenGraphics();
}

void ScreenPreviewWidget::paintEvent(QPaintEvent *event)
{
    if (d->monitorRect.size().isEmpty()) {
        return;
    }

    QPainter painter(this);
    QPoint standPosition(d->monitorRect.center().x() - d->screenGraphics->elementSize("base").width() / 2, d->previewRect.bottom());

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
    if (!e->mimeData()->hasUrls()) {
        return;
    }

    QList<QUrl> uris(KUrlMimeData::urlsFromMimeData(e->mimeData()));
    if (!uris.isEmpty()) {
        // TODO: Download remote file
        if (uris.first().isLocalFile()) {
            Q_EMIT imageDropped(uris.first().path());
        }
    }
}

#include "moc_screenpreviewwidget.cpp"
