/*
    SPDX-FileCopyrightText: 2009 Marco Martin <notmart@gmail.com>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/
#pragma once

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

    void setMinimumContentWidth(qreal minw);
    qreal minimumContentWidth() const;

    QRect previewRect() const;

protected:
    void resizeEvent(QResizeEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    void dropEvent(QDropEvent *event) override;

Q_SIGNALS:
    void imageDropped(const QString &);

private:
    void updateRect(const QRectF &rect);

    const std::unique_ptr<ScreenPreviewWidgetPrivate> d;
};
