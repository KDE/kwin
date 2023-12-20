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
    QTest::addRow("rotate-90") << OutputTransform::Rotated90 << QSizeF(10, 20) << QSizeF(20, 10);
    QTest::addRow("rotate-180") << OutputTransform::Rotated180 << QSizeF(10, 20) << QSizeF(10, 20);
    QTest::addRow("rotate-270") << OutputTransform::Rotated270 << QSizeF(10, 20) << QSizeF(20, 10);
    QTest::addRow("flip-0") << OutputTransform::Flipped << QSizeF(10, 20) << QSizeF(10, 20);
    QTest::addRow("flip-90") << OutputTransform::Flipped90 << QSizeF(10, 20) << QSizeF(20, 10);
    QTest::addRow("flip-180") << OutputTransform::Flipped180 << QSizeF(10, 20) << QSizeF(10, 20);
    QTest::addRow("flip-270") << OutputTransform::Flipped270 << QSizeF(10, 20) << QSizeF(20, 10);
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
    QTest::addRow("rotate-90") << OutputTransform::Rotated90 << QSize(10, 20) << QSize(20, 10);
    QTest::addRow("rotate-180") << OutputTransform::Rotated180 << QSize(10, 20) << QSize(10, 20);
    QTest::addRow("rotate-270") << OutputTransform::Rotated270 << QSize(10, 20) << QSize(20, 10);
    QTest::addRow("flip-0") << OutputTransform::Flipped << QSize(10, 20) << QSize(10, 20);
    QTest::addRow("flip-90") << OutputTransform::Flipped90 << QSize(10, 20) << QSize(20, 10);
    QTest::addRow("flip-180") << OutputTransform::Flipped180 << QSize(10, 20) << QSize(10, 20);
    QTest::addRow("flip-270") << OutputTransform::Flipped270 << QSize(10, 20) << QSize(20, 10);
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
    QTest::addRow("rotate-90") << OutputTransform::Rotated90 << QRectF(10, 20, 30, 40) << QRectF(140, 10, 40, 30);
    QTest::addRow("rotate-180") << OutputTransform::Rotated180 << QRectF(10, 20, 30, 40) << QRectF(60, 140, 30, 40);
    QTest::addRow("rotate-270") << OutputTransform::Rotated270 << QRectF(10, 20, 30, 40) << QRectF(20, 60, 40, 30);
    QTest::addRow("flip-0") << OutputTransform::Flipped << QRectF(10, 20, 30, 40) << QRectF(60, 20, 30, 40);
    QTest::addRow("flip-90") << OutputTransform::Flipped90 << QRectF(10, 20, 30, 40) << QRectF(20, 10, 40, 30);
    QTest::addRow("flip-180") << OutputTransform::Flipped180 << QRectF(10, 20, 30, 40) << QRectF(10, 140, 30, 40);
    QTest::addRow("flip-270") << OutputTransform::Flipped270 << QRectF(10, 20, 30, 40) << QRectF(140, 60, 40, 30);
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
    QTest::addRow("rotate-90") << OutputTransform::Rotated90 << QRect(10, 20, 30, 40) << QRect(140, 10, 40, 30);
    QTest::addRow("rotate-180") << OutputTransform::Rotated180 << QRect(10, 20, 30, 40) << QRect(60, 140, 30, 40);
    QTest::addRow("rotate-270") << OutputTransform::Rotated270 << QRect(10, 20, 30, 40) << QRect(20, 60, 40, 30);
    QTest::addRow("flip-0") << OutputTransform::Flipped << QRect(10, 20, 30, 40) << QRect(60, 20, 30, 40);
    QTest::addRow("flip-90") << OutputTransform::Flipped90 << QRect(10, 20, 30, 40) << QRect(20, 10, 40, 30);
    QTest::addRow("flip-180") << OutputTransform::Flipped180 << QRect(10, 20, 30, 40) << QRect(10, 140, 30, 40);
    QTest::addRow("flip-270") << OutputTransform::Flipped270 << QRect(10, 20, 30, 40) << QRect(140, 60, 40, 30);
}

void TestOutputTransform::mapRect()
{
    QFETCH(OutputTransform::Kind, kind);
    QFETCH(QRect, source);
    QFETCH(QRect, target);

    QCOMPARE(OutputTransform(kind).map(source, QSize(100, 200)), target);
}

void TestOutputTransform::inverted_data()
{
    QTest::addColumn<OutputTransform::Kind>("kind");
    QTest::addColumn<OutputTransform::Kind>("inverted");

    QTest::addRow("rotate-0") << OutputTransform::Normal << OutputTransform::Normal;
    QTest::addRow("rotate-90") << OutputTransform::Rotated90 << OutputTransform::Rotated270;
    QTest::addRow("rotate-180") << OutputTransform::Rotated180 << OutputTransform::Rotated180;
    QTest::addRow("rotate-270") << OutputTransform::Rotated270 << OutputTransform::Rotated90;
    QTest::addRow("flip-0") << OutputTransform::Flipped << OutputTransform::Flipped;
    QTest::addRow("flip-90") << OutputTransform::Flipped90 << OutputTransform::Flipped90;
    QTest::addRow("flip-180") << OutputTransform::Flipped180 << OutputTransform::Flipped180;
    QTest::addRow("flip-270") << OutputTransform::Flipped270 << OutputTransform::Flipped270;
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
    QTest::addRow("rotate-90 | rotate-0") << OutputTransform::Rotated90 << OutputTransform::Normal << OutputTransform::Rotated90;
    QTest::addRow("rotate-180 | rotate-0") << OutputTransform::Rotated180 << OutputTransform::Normal << OutputTransform::Rotated180;
    QTest::addRow("rotate-270 | rotate-0") << OutputTransform::Rotated270 << OutputTransform::Normal << OutputTransform::Rotated270;
    QTest::addRow("flip-0 | rotate-0") << OutputTransform::Flipped << OutputTransform::Normal << OutputTransform::Flipped;
    QTest::addRow("flip-90 | rotate-0") << OutputTransform::Flipped90 << OutputTransform::Normal << OutputTransform::Flipped90;
    QTest::addRow("flip-180 | rotate-0") << OutputTransform::Flipped180 << OutputTransform::Normal << OutputTransform::Flipped180;
    QTest::addRow("flip-270 | rotate-0") << OutputTransform::Flipped270 << OutputTransform::Normal << OutputTransform::Flipped270;

    QTest::addRow("rotate-0 | rotate-90") << OutputTransform::Normal << OutputTransform::Rotated90 << OutputTransform::Rotated90;
    QTest::addRow("rotate-90 | rotate-90") << OutputTransform::Rotated90 << OutputTransform::Rotated90 << OutputTransform::Rotated180;
    QTest::addRow("rotate-180 | rotate-90") << OutputTransform::Rotated180 << OutputTransform::Rotated90 << OutputTransform::Rotated270;
    QTest::addRow("rotate-270 | rotate-90") << OutputTransform::Rotated270 << OutputTransform::Rotated90 << OutputTransform::Normal;
    QTest::addRow("flip-0 | rotate-90") << OutputTransform::Flipped << OutputTransform::Rotated90 << OutputTransform::Flipped270;
    QTest::addRow("flip-90 | rotate-90") << OutputTransform::Flipped90 << OutputTransform::Rotated90 << OutputTransform::Flipped;
    QTest::addRow("flip-180 | rotate-90") << OutputTransform::Flipped180 << OutputTransform::Rotated90 << OutputTransform::Flipped90;
    QTest::addRow("flip-270 | rotate-90") << OutputTransform::Flipped270 << OutputTransform::Rotated90 << OutputTransform::Flipped180;

    QTest::addRow("rotate-0 | rotate-180") << OutputTransform::Normal << OutputTransform::Rotated180 << OutputTransform::Rotated180;
    QTest::addRow("rotate-90 | rotate-180") << OutputTransform::Rotated90 << OutputTransform::Rotated180 << OutputTransform::Rotated270;
    QTest::addRow("rotate-180 | rotate-180") << OutputTransform::Rotated180 << OutputTransform::Rotated180 << OutputTransform::Normal;
    QTest::addRow("rotate-270 | rotate-180") << OutputTransform::Rotated270 << OutputTransform::Rotated180 << OutputTransform::Rotated90;
    QTest::addRow("flip-0 | rotate-180") << OutputTransform::Flipped << OutputTransform::Rotated180 << OutputTransform::Flipped180;
    QTest::addRow("flip-90 | rotate-180") << OutputTransform::Flipped90 << OutputTransform::Rotated180 << OutputTransform::Flipped270;
    QTest::addRow("flip-180 | rotate-180") << OutputTransform::Flipped180 << OutputTransform::Rotated180 << OutputTransform::Flipped;
    QTest::addRow("flip-270 | rotate-180") << OutputTransform::Flipped270 << OutputTransform::Rotated180 << OutputTransform::Flipped90;

    QTest::addRow("rotate-0 | rotate-270") << OutputTransform::Normal << OutputTransform::Rotated270 << OutputTransform::Rotated270;
    QTest::addRow("rotate-90 | rotate-270") << OutputTransform::Rotated90 << OutputTransform::Rotated270 << OutputTransform::Normal;
    QTest::addRow("rotate-180 | rotate-270") << OutputTransform::Rotated180 << OutputTransform::Rotated270 << OutputTransform::Rotated90;
    QTest::addRow("rotate-270 | rotate-270") << OutputTransform::Rotated270 << OutputTransform::Rotated270 << OutputTransform::Rotated180;
    QTest::addRow("flip-0 | rotate-270") << OutputTransform::Flipped << OutputTransform::Rotated270 << OutputTransform::Flipped90;
    QTest::addRow("flip-90 | rotate-270") << OutputTransform::Flipped90 << OutputTransform::Rotated270 << OutputTransform::Flipped180;
    QTest::addRow("flip-180 | rotate-270") << OutputTransform::Flipped180 << OutputTransform::Rotated270 << OutputTransform::Flipped270;
    QTest::addRow("flip-270 | rotate-270") << OutputTransform::Flipped270 << OutputTransform::Rotated270 << OutputTransform::Flipped;

    QTest::addRow("rotate-0 | flip-0") << OutputTransform::Normal << OutputTransform::Flipped << OutputTransform::Flipped;
    QTest::addRow("rotate-90 | flip-0") << OutputTransform::Rotated90 << OutputTransform::Flipped << OutputTransform::Flipped90;
    QTest::addRow("rotate-180 | flip-0") << OutputTransform::Rotated180 << OutputTransform::Flipped << OutputTransform::Flipped180;
    QTest::addRow("rotate-270 | flip-0") << OutputTransform::Rotated270 << OutputTransform::Flipped << OutputTransform::Flipped270;
    QTest::addRow("flip-0 | flip-0") << OutputTransform::Flipped << OutputTransform::Flipped << OutputTransform::Normal;
    QTest::addRow("flip-90 | flip-0") << OutputTransform::Flipped90 << OutputTransform::Flipped << OutputTransform::Rotated90;
    QTest::addRow("flip-180 | flip-0") << OutputTransform::Flipped180 << OutputTransform::Flipped << OutputTransform::Rotated180;
    QTest::addRow("flip-270 | flip-0") << OutputTransform::Flipped270 << OutputTransform::Flipped << OutputTransform::Rotated270;

    QTest::addRow("rotate-0 | flip-90") << OutputTransform::Normal << OutputTransform::Flipped90 << OutputTransform::Flipped90;
    QTest::addRow("rotate-90 | flip-90") << OutputTransform::Rotated90 << OutputTransform::Flipped90 << OutputTransform::Flipped180;
    QTest::addRow("rotate-180 | flip-90") << OutputTransform::Rotated180 << OutputTransform::Flipped90 << OutputTransform::Flipped270;
    QTest::addRow("rotate-270 | flip-90") << OutputTransform::Rotated270 << OutputTransform::Flipped90 << OutputTransform::Flipped;
    QTest::addRow("flip-0 | flip-90") << OutputTransform::Flipped << OutputTransform::Flipped90 << OutputTransform::Rotated270;
    QTest::addRow("flip-90 | flip-90") << OutputTransform::Flipped90 << OutputTransform::Flipped90 << OutputTransform::Normal;
    QTest::addRow("flip-180 | flip-90") << OutputTransform::Flipped180 << OutputTransform::Flipped90 << OutputTransform::Rotated90;
    QTest::addRow("flip-270 | flip-90") << OutputTransform::Flipped270 << OutputTransform::Flipped90 << OutputTransform::Rotated180;

    QTest::addRow("rotate-0 | flip-180") << OutputTransform::Normal << OutputTransform::Flipped180 << OutputTransform::Flipped180;
    QTest::addRow("rotate-90 | flip-180") << OutputTransform::Rotated90 << OutputTransform::Flipped180 << OutputTransform::Flipped270;
    QTest::addRow("rotate-180 | flip-180") << OutputTransform::Rotated180 << OutputTransform::Flipped180 << OutputTransform::Flipped;
    QTest::addRow("rotate-270 | flip-180") << OutputTransform::Rotated270 << OutputTransform::Flipped180 << OutputTransform::Flipped90;
    QTest::addRow("flip-0 | flip-180") << OutputTransform::Flipped << OutputTransform::Flipped180 << OutputTransform::Rotated180;
    QTest::addRow("flip-90 | flip-180") << OutputTransform::Flipped90 << OutputTransform::Flipped180 << OutputTransform::Rotated270;
    QTest::addRow("flip-180 | flip-180") << OutputTransform::Flipped180 << OutputTransform::Flipped180 << OutputTransform::Normal;
    QTest::addRow("flip-270 | flip-180") << OutputTransform::Flipped270 << OutputTransform::Flipped180 << OutputTransform::Rotated90;

    QTest::addRow("rotate-0 | flip-270") << OutputTransform::Normal << OutputTransform::Flipped270 << OutputTransform::Flipped270;
    QTest::addRow("rotate-90 | flip-270") << OutputTransform::Rotated90 << OutputTransform::Flipped270 << OutputTransform::Flipped;
    QTest::addRow("rotate-180 | flip-270") << OutputTransform::Rotated180 << OutputTransform::Flipped270 << OutputTransform::Flipped90;
    QTest::addRow("rotate-270 | flip-270") << OutputTransform::Rotated270 << OutputTransform::Flipped270 << OutputTransform::Flipped180;
    QTest::addRow("flip-0 | flip-270") << OutputTransform::Flipped << OutputTransform::Flipped270 << OutputTransform::Rotated90;
    QTest::addRow("flip-90 | flip-270") << OutputTransform::Flipped90 << OutputTransform::Flipped270 << OutputTransform::Rotated180;
    QTest::addRow("flip-180 | flip-270") << OutputTransform::Flipped180 << OutputTransform::Flipped270 << OutputTransform::Rotated270;
    QTest::addRow("flip-270 | flip-270") << OutputTransform::Flipped270 << OutputTransform::Flipped270 << OutputTransform::Normal;
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
    QTest::addRow("rotate-90") << OutputTransform::Rotated90 << QRectF(10, 20, 30, 40) << QRectF(140, 10, 40, 30);
    QTest::addRow("rotate-180") << OutputTransform::Rotated180 << QRectF(10, 20, 30, 40) << QRectF(60, 140, 30, 40);
    QTest::addRow("rotate-270") << OutputTransform::Rotated270 << QRectF(10, 20, 30, 40) << QRectF(20, 60, 40, 30);
    QTest::addRow("flip-0") << OutputTransform::Flipped << QRectF(10, 20, 30, 40) << QRectF(60, 20, 30, 40);
    QTest::addRow("flip-90") << OutputTransform::Flipped90 << QRectF(10, 20, 30, 40) << QRectF(20, 10, 40, 30);
    QTest::addRow("flip-180") << OutputTransform::Flipped180 << QRectF(10, 20, 30, 40) << QRectF(10, 140, 30, 40);
    QTest::addRow("flip-270") << OutputTransform::Flipped270 << QRectF(10, 20, 30, 40) << QRectF(140, 60, 40, 30);
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
