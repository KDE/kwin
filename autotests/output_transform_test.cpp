/*
    SPDX-FileCopyrightText: 2023 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include <QTest>

#include "core/output.h"

using namespace KWin;

class TestOutputTransform : public QObject
{
    Q_OBJECT

public:
    TestOutputTransform();

private Q_SLOTS:
    void mapSizeF_data();
    void mapSizeF();
    void mapSize_data();
    void mapSize();
    void mapRectF_data();
    void mapRectF();
    void mapRect_data();
    void mapRect();
    void mapPointF_data();
    void mapPointF();
    void mapPoint_data();
    void mapPoint();
    void inverted_data();
    void inverted();
    void combine_data();
    void combine();
    void matrix_data();
    void matrix();
};

TestOutputTransform::TestOutputTransform()
{
}

void TestOutputTransform::mapSizeF_data()
{
    QTest::addColumn<OutputTransform::Kind>("kind");
    QTest::addColumn<QSizeF>("source");
    QTest::addColumn<QSizeF>("target");

    QTest::addRow("rotate-0") << OutputTransform::Normal << QSizeF(10, 20) << QSizeF(10, 20);
    QTest::addRow("rotate-90") << OutputTransform::Rotate90 << QSizeF(10, 20) << QSizeF(20, 10);
    QTest::addRow("rotate-180") << OutputTransform::Rotate180 << QSizeF(10, 20) << QSizeF(10, 20);
    QTest::addRow("rotate-270") << OutputTransform::Rotate270 << QSizeF(10, 20) << QSizeF(20, 10);
    QTest::addRow("flip-x-0") << OutputTransform::FlipX << QSizeF(10, 20) << QSizeF(10, 20);
    QTest::addRow("flip-x-90") << OutputTransform::FlipX90 << QSizeF(10, 20) << QSizeF(20, 10);
    QTest::addRow("flip-x-180") << OutputTransform::FlipX180 << QSizeF(10, 20) << QSizeF(10, 20);
    QTest::addRow("flip-x-270") << OutputTransform::FlipX270 << QSizeF(10, 20) << QSizeF(20, 10);
}

void TestOutputTransform::mapSizeF()
{
    QFETCH(OutputTransform::Kind, kind);
    QFETCH(QSizeF, source);
    QFETCH(QSizeF, target);

    QCOMPARE(OutputTransform(kind).map(source), target);
}

void TestOutputTransform::mapSize_data()
{
    QTest::addColumn<OutputTransform::Kind>("kind");
    QTest::addColumn<QSize>("source");
    QTest::addColumn<QSize>("target");

    QTest::addRow("rotate-0") << OutputTransform::Normal << QSize(10, 20) << QSize(10, 20);
    QTest::addRow("rotate-90") << OutputTransform::Rotate90 << QSize(10, 20) << QSize(20, 10);
    QTest::addRow("rotate-180") << OutputTransform::Rotate180 << QSize(10, 20) << QSize(10, 20);
    QTest::addRow("rotate-270") << OutputTransform::Rotate270 << QSize(10, 20) << QSize(20, 10);
    QTest::addRow("flip-x-0") << OutputTransform::FlipX << QSize(10, 20) << QSize(10, 20);
    QTest::addRow("flip-x-90") << OutputTransform::FlipX90 << QSize(10, 20) << QSize(20, 10);
    QTest::addRow("flip-x-180") << OutputTransform::FlipX180 << QSize(10, 20) << QSize(10, 20);
    QTest::addRow("flip-x-270") << OutputTransform::FlipX270 << QSize(10, 20) << QSize(20, 10);
}

void TestOutputTransform::mapSize()
{
    QFETCH(OutputTransform::Kind, kind);
    QFETCH(QSize, source);
    QFETCH(QSize, target);

    QCOMPARE(OutputTransform(kind).map(source), target);
}

void TestOutputTransform::mapRectF_data()
{
    QTest::addColumn<OutputTransform::Kind>("kind");
    QTest::addColumn<QRectF>("source");
    QTest::addColumn<QRectF>("target");

    QTest::addRow("rotate-0") << OutputTransform::Normal << QRectF(10, 20, 30, 40) << QRectF(10, 20, 30, 40);
    QTest::addRow("rotate-90") << OutputTransform::Rotate90 << QRectF(10, 20, 30, 40) << QRectF(20, 60, 40, 30);
    QTest::addRow("rotate-180") << OutputTransform::Rotate180 << QRectF(10, 20, 30, 40) << QRectF(60, 140, 30, 40);
    QTest::addRow("rotate-270") << OutputTransform::Rotate270 << QRectF(10, 20, 30, 40) << QRectF(140, 10, 40, 30);
    QTest::addRow("flip-x-0") << OutputTransform::FlipX << QRectF(10, 20, 30, 40) << QRectF(60, 20, 30, 40);
    QTest::addRow("flip-x-90") << OutputTransform::FlipX90 << QRectF(10, 20, 30, 40) << QRectF(20, 10, 40, 30);
    QTest::addRow("flip-x-180") << OutputTransform::FlipX180 << QRectF(10, 20, 30, 40) << QRectF(10, 140, 30, 40);
    QTest::addRow("flip-x-270") << OutputTransform::FlipX270 << QRectF(10, 20, 30, 40) << QRectF(140, 60, 40, 30);
}

void TestOutputTransform::mapRectF()
{
    QFETCH(OutputTransform::Kind, kind);
    QFETCH(QRectF, source);
    QFETCH(QRectF, target);

    QCOMPARE(OutputTransform(kind).map(source, QSizeF(100, 200)), target);
}

void TestOutputTransform::mapRect_data()
{
    QTest::addColumn<OutputTransform::Kind>("kind");
    QTest::addColumn<QRect>("source");
    QTest::addColumn<QRect>("target");

    QTest::addRow("rotate-0") << OutputTransform::Normal << QRect(10, 20, 30, 40) << QRect(10, 20, 30, 40);
    QTest::addRow("rotate-90") << OutputTransform::Rotate90 << QRect(10, 20, 30, 40) << QRect(20, 60, 40, 30);
    QTest::addRow("rotate-180") << OutputTransform::Rotate180 << QRect(10, 20, 30, 40) << QRect(60, 140, 30, 40);
    QTest::addRow("rotate-270") << OutputTransform::Rotate270 << QRect(10, 20, 30, 40) << QRect(140, 10, 40, 30);
    QTest::addRow("flip-x-0") << OutputTransform::FlipX << QRect(10, 20, 30, 40) << QRect(60, 20, 30, 40);
    QTest::addRow("flip-x-90") << OutputTransform::FlipX90 << QRect(10, 20, 30, 40) << QRect(20, 10, 40, 30);
    QTest::addRow("flip-x-180") << OutputTransform::FlipX180 << QRect(10, 20, 30, 40) << QRect(10, 140, 30, 40);
    QTest::addRow("flip-x-270") << OutputTransform::FlipX270 << QRect(10, 20, 30, 40) << QRect(140, 60, 40, 30);
}

void TestOutputTransform::mapRect()
{
    QFETCH(OutputTransform::Kind, kind);
    QFETCH(QRect, source);
    QFETCH(QRect, target);

    QCOMPARE(OutputTransform(kind).map(source, QSize(100, 200)), target);
}

void TestOutputTransform::mapPointF_data()
{
    QTest::addColumn<OutputTransform::Kind>("kind");
    QTest::addColumn<QPointF>("source");
    QTest::addColumn<QPointF>("target");

    QTest::addRow("rotate-0") << OutputTransform::Normal << QPointF(10, 20) << QPointF(10, 20);
    QTest::addRow("rotate-90") << OutputTransform::Rotate90 << QPointF(10, 20) << QPointF(20, 90);
    QTest::addRow("rotate-180") << OutputTransform::Rotate180 << QPointF(10, 20) << QPointF(90, 180);
    QTest::addRow("rotate-270") << OutputTransform::Rotate270 << QPointF(10, 20) << QPointF(180, 10);
    QTest::addRow("flip-x-0") << OutputTransform::FlipX << QPointF(10, 20) << QPointF(90, 20);
    QTest::addRow("flip-x-90") << OutputTransform::FlipX90 << QPointF(10, 20) << QPointF(20, 10);
    QTest::addRow("flip-x-180") << OutputTransform::FlipX180 << QPointF(10, 20) << QPointF(10, 180);
    QTest::addRow("flip-x-270") << OutputTransform::FlipX270 << QPointF(10, 20) << QPointF(180, 90);
}

void TestOutputTransform::mapPointF()
{
    QFETCH(OutputTransform::Kind, kind);
    QFETCH(QPointF, source);
    QFETCH(QPointF, target);

    const OutputTransform transform(kind);

    QCOMPARE(transform.map(source, QSizeF(100, 200)), target);
    QCOMPARE(transform.map(QRectF(source, QSizeF(0, 0)), QSizeF(100, 200)), QRectF(target, QSizeF(0, 0)));
}

void TestOutputTransform::mapPoint_data()
{
    QTest::addColumn<OutputTransform::Kind>("kind");
    QTest::addColumn<QPoint>("source");
    QTest::addColumn<QPoint>("target");

    QTest::addRow("rotate-0") << OutputTransform::Normal << QPoint(10, 20) << QPoint(10, 20);
    QTest::addRow("rotate-90") << OutputTransform::Rotate90 << QPoint(10, 20) << QPoint(20, 90);
    QTest::addRow("rotate-180") << OutputTransform::Rotate180 << QPoint(10, 20) << QPoint(90, 180);
    QTest::addRow("rotate-270") << OutputTransform::Rotate270 << QPoint(10, 20) << QPoint(180, 10);
    QTest::addRow("flip-x-0") << OutputTransform::FlipX << QPoint(10, 20) << QPoint(90, 20);
    QTest::addRow("flip-x-90") << OutputTransform::FlipX90 << QPoint(10, 20) << QPoint(20, 10);
    QTest::addRow("flip-x-180") << OutputTransform::FlipX180 << QPoint(10, 20) << QPoint(10, 180);
    QTest::addRow("flip-x-270") << OutputTransform::FlipX270 << QPoint(10, 20) << QPoint(180, 90);
}

void TestOutputTransform::mapPoint()
{
    QFETCH(OutputTransform::Kind, kind);
    QFETCH(QPoint, source);
    QFETCH(QPoint, target);

    const OutputTransform transform(kind);

    QCOMPARE(transform.map(source, QSize(100, 200)), target);
    QCOMPARE(transform.map(QRect(source, QSize(0, 0)), QSize(100, 200)), QRect(target, QSize(0, 0)));
}

void TestOutputTransform::inverted_data()
{
    QTest::addColumn<OutputTransform::Kind>("kind");
    QTest::addColumn<OutputTransform::Kind>("inverted");

    QTest::addRow("rotate-0") << OutputTransform::Normal << OutputTransform::Normal;
    QTest::addRow("rotate-90") << OutputTransform::Rotate90 << OutputTransform::Rotate270;
    QTest::addRow("rotate-180") << OutputTransform::Rotate180 << OutputTransform::Rotate180;
    QTest::addRow("rotate-270") << OutputTransform::Rotate270 << OutputTransform::Rotate90;
    QTest::addRow("flip-x-0") << OutputTransform::FlipX << OutputTransform::FlipX;
    QTest::addRow("flip-x-90") << OutputTransform::FlipX90 << OutputTransform::FlipX90;
    QTest::addRow("flip-x-180") << OutputTransform::FlipX180 << OutputTransform::FlipX180;
    QTest::addRow("flip-x-270") << OutputTransform::FlipX270 << OutputTransform::FlipX270;
}

void TestOutputTransform::inverted()
{
    QFETCH(OutputTransform::Kind, kind);
    QFETCH(OutputTransform::Kind, inverted);

    QCOMPARE(OutputTransform(kind).inverted(), OutputTransform(inverted));
}

void TestOutputTransform::combine_data()
{
    QTest::addColumn<OutputTransform::Kind>("first");
    QTest::addColumn<OutputTransform::Kind>("second");
    QTest::addColumn<OutputTransform::Kind>("result");

    QTest::addRow("rotate-0 | rotate-0") << OutputTransform::Normal << OutputTransform::Normal << OutputTransform::Normal;
    QTest::addRow("rotate-90 | rotate-0") << OutputTransform::Rotate90 << OutputTransform::Normal << OutputTransform::Rotate90;
    QTest::addRow("rotate-180 | rotate-0") << OutputTransform::Rotate180 << OutputTransform::Normal << OutputTransform::Rotate180;
    QTest::addRow("rotate-270 | rotate-0") << OutputTransform::Rotate270 << OutputTransform::Normal << OutputTransform::Rotate270;
    QTest::addRow("flip-x-0 | rotate-0") << OutputTransform::FlipX << OutputTransform::Normal << OutputTransform::FlipX;
    QTest::addRow("flip-x-90 | rotate-0") << OutputTransform::FlipX90 << OutputTransform::Normal << OutputTransform::FlipX90;
    QTest::addRow("flip-x-180 | rotate-0") << OutputTransform::FlipX180 << OutputTransform::Normal << OutputTransform::FlipX180;
    QTest::addRow("flip-x-270 | rotate-0") << OutputTransform::FlipX270 << OutputTransform::Normal << OutputTransform::FlipX270;

    QTest::addRow("rotate-0 | rotate-90") << OutputTransform::Normal << OutputTransform::Rotate90 << OutputTransform::Rotate90;
    QTest::addRow("rotate-90 | rotate-90") << OutputTransform::Rotate90 << OutputTransform::Rotate90 << OutputTransform::Rotate180;
    QTest::addRow("rotate-180 | rotate-90") << OutputTransform::Rotate180 << OutputTransform::Rotate90 << OutputTransform::Rotate270;
    QTest::addRow("rotate-270 | rotate-90") << OutputTransform::Rotate270 << OutputTransform::Rotate90 << OutputTransform::Normal;
    QTest::addRow("flip-x-0 | rotate-90") << OutputTransform::FlipX << OutputTransform::Rotate90 << OutputTransform::FlipX90;
    QTest::addRow("flip-x-90 | rotate-90") << OutputTransform::FlipX90 << OutputTransform::Rotate90 << OutputTransform::FlipX180;
    QTest::addRow("flip-x-180 | rotate-90") << OutputTransform::FlipX180 << OutputTransform::Rotate90 << OutputTransform::FlipX270;
    QTest::addRow("flip-x-270 | rotate-90") << OutputTransform::FlipX270 << OutputTransform::Rotate90 << OutputTransform::FlipX;

    QTest::addRow("rotate-0 | rotate-180") << OutputTransform::Normal << OutputTransform::Rotate180 << OutputTransform::Rotate180;
    QTest::addRow("rotate-90 | rotate-180") << OutputTransform::Rotate90 << OutputTransform::Rotate180 << OutputTransform::Rotate270;
    QTest::addRow("rotate-180 | rotate-180") << OutputTransform::Rotate180 << OutputTransform::Rotate180 << OutputTransform::Normal;
    QTest::addRow("rotate-270 | rotate-180") << OutputTransform::Rotate270 << OutputTransform::Rotate180 << OutputTransform::Rotate90;
    QTest::addRow("flip-x-0 | rotate-180") << OutputTransform::FlipX << OutputTransform::Rotate180 << OutputTransform::FlipX180;
    QTest::addRow("flip-x-90 | rotate-180") << OutputTransform::FlipX90 << OutputTransform::Rotate180 << OutputTransform::FlipX270;
    QTest::addRow("flip-x-180 | rotate-180") << OutputTransform::FlipX180 << OutputTransform::Rotate180 << OutputTransform::FlipX;
    QTest::addRow("flip-x-270 | rotate-180") << OutputTransform::FlipX270 << OutputTransform::Rotate180 << OutputTransform::FlipX90;

    QTest::addRow("rotate-0 | rotate-270") << OutputTransform::Normal << OutputTransform::Rotate270 << OutputTransform::Rotate270;
    QTest::addRow("rotate-90 | rotate-270") << OutputTransform::Rotate90 << OutputTransform::Rotate270 << OutputTransform::Normal;
    QTest::addRow("rotate-180 | rotate-270") << OutputTransform::Rotate180 << OutputTransform::Rotate270 << OutputTransform::Rotate90;
    QTest::addRow("rotate-270 | rotate-270") << OutputTransform::Rotate270 << OutputTransform::Rotate270 << OutputTransform::Rotate180;
    QTest::addRow("flip-x-0 | rotate-270") << OutputTransform::FlipX << OutputTransform::Rotate270 << OutputTransform::FlipX270;
    QTest::addRow("flip-x-90 | rotate-270") << OutputTransform::FlipX90 << OutputTransform::Rotate270 << OutputTransform::FlipX;
    QTest::addRow("flip-x-180 | rotate-270") << OutputTransform::FlipX180 << OutputTransform::Rotate270 << OutputTransform::FlipX90;
    QTest::addRow("flip-x-270 | rotate-270") << OutputTransform::FlipX270 << OutputTransform::Rotate270 << OutputTransform::FlipX180;

    QTest::addRow("rotate-0 | flip-x-0") << OutputTransform::Normal << OutputTransform::FlipX << OutputTransform::FlipX;
    QTest::addRow("rotate-90 | flip-x-0") << OutputTransform::Rotate90 << OutputTransform::FlipX << OutputTransform::FlipX270;
    QTest::addRow("rotate-180 | flip-x-0") << OutputTransform::Rotate180 << OutputTransform::FlipX << OutputTransform::FlipX180;
    QTest::addRow("rotate-270 | flip-x-0") << OutputTransform::Rotate270 << OutputTransform::FlipX << OutputTransform::FlipX90;
    QTest::addRow("flip-x-0 | flip-x-0") << OutputTransform::FlipX << OutputTransform::FlipX << OutputTransform::Normal;
    QTest::addRow("flip-x-90 | flip-x-0") << OutputTransform::FlipX90 << OutputTransform::FlipX << OutputTransform::Rotate270;
    QTest::addRow("flip-x-180 | flip-x-0") << OutputTransform::FlipX180 << OutputTransform::FlipX << OutputTransform::Rotate180;
    QTest::addRow("flip-x-270 | flip-x-0") << OutputTransform::FlipX270 << OutputTransform::FlipX << OutputTransform::Rotate90;

    QTest::addRow("rotate-0 | flip-x-90") << OutputTransform::Normal << OutputTransform::FlipX90 << OutputTransform::FlipX90;
    QTest::addRow("rotate-90 | flip-x-90") << OutputTransform::Rotate90 << OutputTransform::FlipX90 << OutputTransform::FlipX;
    QTest::addRow("rotate-180 | flip-x-90") << OutputTransform::Rotate180 << OutputTransform::FlipX90 << OutputTransform::FlipX270;
    QTest::addRow("rotate-270 | flip-x-90") << OutputTransform::Rotate270 << OutputTransform::FlipX90 << OutputTransform::FlipX180;
    QTest::addRow("flip-x-0 | flip-x-90") << OutputTransform::FlipX << OutputTransform::FlipX90 << OutputTransform::Rotate90;
    QTest::addRow("flip-x-90 | flip-x-90") << OutputTransform::FlipX90 << OutputTransform::FlipX90 << OutputTransform::Normal;
    QTest::addRow("flip-x-180 | flip-x-90") << OutputTransform::FlipX180 << OutputTransform::FlipX90 << OutputTransform::Rotate270;
    QTest::addRow("flip-x-270 | flip-x-90") << OutputTransform::FlipX270 << OutputTransform::FlipX90 << OutputTransform::Rotate180;

    QTest::addRow("rotate-0 | flip-x-180") << OutputTransform::Normal << OutputTransform::FlipX180 << OutputTransform::FlipX180;
    QTest::addRow("rotate-90 | flip-x-180") << OutputTransform::Rotate90 << OutputTransform::FlipX180 << OutputTransform::FlipX90;
    QTest::addRow("rotate-180 | flip-x-180") << OutputTransform::Rotate180 << OutputTransform::FlipX180 << OutputTransform::FlipX;
    QTest::addRow("rotate-270 | flip-x-180") << OutputTransform::Rotate270 << OutputTransform::FlipX180 << OutputTransform::FlipX270;
    QTest::addRow("flip-x-0 | flip-x-180") << OutputTransform::FlipX << OutputTransform::FlipX180 << OutputTransform::Rotate180;
    QTest::addRow("flip-x-90 | flip-x-180") << OutputTransform::FlipX90 << OutputTransform::FlipX180 << OutputTransform::Rotate90;
    QTest::addRow("flip-x-180 | flip-x-180") << OutputTransform::FlipX180 << OutputTransform::FlipX180 << OutputTransform::Normal;
    QTest::addRow("flip-x-270 | flip-x-180") << OutputTransform::FlipX270 << OutputTransform::FlipX180 << OutputTransform::Rotate270;

    QTest::addRow("rotate-0 | flip-x-270") << OutputTransform::Normal << OutputTransform::FlipX270 << OutputTransform::FlipX270;
    QTest::addRow("rotate-90 | flip-x-270") << OutputTransform::Rotate90 << OutputTransform::FlipX270 << OutputTransform::FlipX180;
    QTest::addRow("rotate-180 | flip-x-270") << OutputTransform::Rotate180 << OutputTransform::FlipX270 << OutputTransform::FlipX90;
    QTest::addRow("rotate-270 | flip-x-270") << OutputTransform::Rotate270 << OutputTransform::FlipX270 << OutputTransform::FlipX;
    QTest::addRow("flip-x-0 | flip-x-270") << OutputTransform::FlipX << OutputTransform::FlipX270 << OutputTransform::Rotate270;
    QTest::addRow("flip-x-90 | flip-x-270") << OutputTransform::FlipX90 << OutputTransform::FlipX270 << OutputTransform::Rotate180;
    QTest::addRow("flip-x-180 | flip-x-270") << OutputTransform::FlipX180 << OutputTransform::FlipX270 << OutputTransform::Rotate90;
    QTest::addRow("flip-x-270 | flip-x-270") << OutputTransform::FlipX270 << OutputTransform::FlipX270 << OutputTransform::Normal;
}

void TestOutputTransform::combine()
{
    QFETCH(OutputTransform::Kind, first);
    QFETCH(OutputTransform::Kind, second);
    QFETCH(OutputTransform::Kind, result);

    const OutputTransform firstTransform(first);
    const OutputTransform secondTransform(second);
    const OutputTransform combinedTransform = firstTransform.combine(secondTransform);
    QCOMPARE(combinedTransform.kind(), result);

    const QRectF box(10, 20, 30, 40);
    const QSizeF bounds(100, 200);
    QCOMPARE(combinedTransform.map(box, bounds), secondTransform.map(firstTransform.map(box, bounds), firstTransform.map(bounds)));
}

void TestOutputTransform::matrix_data()
{
    QTest::addColumn<OutputTransform::Kind>("kind");
    QTest::addColumn<QRectF>("source");
    QTest::addColumn<QRectF>("target");

    QTest::addRow("rotate-0") << OutputTransform::Normal << QRectF(10, 20, 30, 40) << QRectF(10, 20, 30, 40);
    QTest::addRow("rotate-90") << OutputTransform::Rotate90 << QRectF(10, 20, 30, 40) << QRectF(20, 60, 40, 30);
    QTest::addRow("rotate-180") << OutputTransform::Rotate180 << QRectF(10, 20, 30, 40) << QRectF(60, 140, 30, 40);
    QTest::addRow("rotate-270") << OutputTransform::Rotate270 << QRectF(10, 20, 30, 40) << QRectF(140, 10, 40, 30);
    QTest::addRow("flip-x-0") << OutputTransform::FlipX << QRectF(10, 20, 30, 40) << QRectF(60, 20, 30, 40);
    QTest::addRow("flip-x-90") << OutputTransform::FlipX90 << QRectF(10, 20, 30, 40) << QRectF(20, 10, 40, 30);
    QTest::addRow("flip-x-180") << OutputTransform::FlipX180 << QRectF(10, 20, 30, 40) << QRectF(10, 140, 30, 40);
    QTest::addRow("flip-x-270") << OutputTransform::FlipX270 << QRectF(10, 20, 30, 40) << QRectF(140, 60, 40, 30);
}

void TestOutputTransform::matrix()
{
    QFETCH(OutputTransform::Kind, kind);
    QFETCH(QRectF, source);
    QFETCH(QRectF, target);

    const OutputTransform transform = kind;
    const QSizeF sourceBounds = QSizeF(100, 200);
    const QSizeF targetBounds = transform.map(sourceBounds);

    QMatrix4x4 matrix;
    matrix.scale(targetBounds.width(), targetBounds.height());
    matrix.translate(0.5, 0.5);
    matrix.scale(0.5, -0.5);
    matrix.scale(1, -1); // flip the y axis back
    matrix *= transform.toMatrix();
    matrix.scale(1, -1); // undo ortho() flipping the y axis
    matrix.ortho(QRectF(0, 0, sourceBounds.width(), sourceBounds.height()));

    const QRectF mapped = matrix.mapRect(source);
    QCOMPARE(mapped, target);
    QCOMPARE(mapped, transform.map(source, sourceBounds));
}

QTEST_MAIN(TestOutputTransform)

#include "output_transform_test.moc"
