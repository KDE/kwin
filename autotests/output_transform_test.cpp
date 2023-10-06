/*
    SPDX-FileCopyrightText: 2023 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include <QTest>

#include "core/output.h"

class TestOutputTransform : public QObject
{
    Q_OBJECT

public:
    TestOutputTransform();

private Q_SLOTS:
    void mapSizeF_data();
    void mapSizeF();
    void mapRectF_data();
    void mapRectF();
    void inverted_data();
    void inverted();
    void combine_data();
    void combine();
};

TestOutputTransform::TestOutputTransform()
{
}

void TestOutputTransform::mapSizeF_data()
{
    QTest::addColumn<KWin::OutputTransform::Kind>("kind");
    QTest::addColumn<QSizeF>("source");
    QTest::addColumn<QSizeF>("target");

    QTest::addRow("rotate-0") << KWin::OutputTransform::Normal << QSizeF(10, 20) << QSizeF(10, 20);
    QTest::addRow("rotate-90") << KWin::OutputTransform::Rotated90 << QSizeF(10, 20) << QSizeF(20, 10);
    QTest::addRow("rotate-180") << KWin::OutputTransform::Rotated180 << QSizeF(10, 20) << QSizeF(10, 20);
    QTest::addRow("rotate-270") << KWin::OutputTransform::Rotated270 << QSizeF(10, 20) << QSizeF(20, 10);
    QTest::addRow("flip-0") << KWin::OutputTransform::Flipped << QSizeF(10, 20) << QSizeF(10, 20);
    QTest::addRow("flip-90") << KWin::OutputTransform::Flipped90 << QSizeF(10, 20) << QSizeF(20, 10);
    QTest::addRow("flip-180") << KWin::OutputTransform::Flipped180 << QSizeF(10, 20) << QSizeF(10, 20);
    QTest::addRow("flip-270") << KWin::OutputTransform::Flipped270 << QSizeF(10, 20) << QSizeF(20, 10);
}

void TestOutputTransform::mapSizeF()
{
    QFETCH(KWin::OutputTransform::Kind, kind);
    QFETCH(QSizeF, source);
    QFETCH(QSizeF, target);

    QCOMPARE(KWin::OutputTransform(kind).map(source), target);
}

void TestOutputTransform::mapRectF_data()
{
    QTest::addColumn<KWin::OutputTransform::Kind>("kind");
    QTest::addColumn<QRectF>("source");
    QTest::addColumn<QRectF>("target");

    QTest::addRow("rotate-0") << KWin::OutputTransform::Normal << QRectF(10, 20, 30, 40) << QRectF(10, 20, 30, 40);
    QTest::addRow("rotate-90") << KWin::OutputTransform::Rotated90 << QRectF(10, 20, 30, 40) << QRectF(140, 10, 40, 30);
    QTest::addRow("rotate-180") << KWin::OutputTransform::Rotated180 << QRectF(10, 20, 30, 40) << QRectF(60, 140, 30, 40);
    QTest::addRow("rotate-270") << KWin::OutputTransform::Rotated270 << QRectF(10, 20, 30, 40) << QRectF(20, 60, 40, 30);
    QTest::addRow("flip-0") << KWin::OutputTransform::Flipped << QRectF(10, 20, 30, 40) << QRectF(60, 20, 30, 40);
    QTest::addRow("flip-90") << KWin::OutputTransform::Flipped90 << QRectF(10, 20, 30, 40) << QRectF(20, 10, 40, 30);
    QTest::addRow("flip-180") << KWin::OutputTransform::Flipped180 << QRectF(10, 20, 30, 40) << QRectF(10, 140, 30, 40);
    QTest::addRow("flip-270") << KWin::OutputTransform::Flipped270 << QRectF(10, 20, 30, 40) << QRectF(140, 60, 40, 30);
}

void TestOutputTransform::mapRectF()
{
    QFETCH(KWin::OutputTransform::Kind, kind);
    QFETCH(QRectF, source);
    QFETCH(QRectF, target);

    QCOMPARE(KWin::OutputTransform(kind).map(source, QSizeF(100, 200)), target);
}

void TestOutputTransform::inverted_data()
{
    QTest::addColumn<KWin::OutputTransform::Kind>("kind");
    QTest::addColumn<KWin::OutputTransform::Kind>("inverted");

    QTest::addRow("rotate-0") << KWin::OutputTransform::Normal << KWin::OutputTransform::Normal;
    QTest::addRow("rotate-90") << KWin::OutputTransform::Rotated90 << KWin::OutputTransform::Rotated270;
    QTest::addRow("rotate-180") << KWin::OutputTransform::Rotated180 << KWin::OutputTransform::Rotated180;
    QTest::addRow("rotate-270") << KWin::OutputTransform::Rotated270 << KWin::OutputTransform::Rotated90;
    QTest::addRow("flip-0") << KWin::OutputTransform::Flipped << KWin::OutputTransform::Flipped;
    QTest::addRow("flip-90") << KWin::OutputTransform::Flipped90 << KWin::OutputTransform::Flipped90;
    QTest::addRow("flip-180") << KWin::OutputTransform::Flipped180 << KWin::OutputTransform::Flipped180;
    QTest::addRow("flip-270") << KWin::OutputTransform::Flipped270 << KWin::OutputTransform::Flipped270;
}

void TestOutputTransform::inverted()
{
    QFETCH(KWin::OutputTransform::Kind, kind);
    QFETCH(KWin::OutputTransform::Kind, inverted);

    QCOMPARE(KWin::OutputTransform(kind).inverted(), KWin::OutputTransform(inverted));
}

void TestOutputTransform::combine_data()
{
    QTest::addColumn<KWin::OutputTransform::Kind>("first");
    QTest::addColumn<KWin::OutputTransform::Kind>("second");
    QTest::addColumn<KWin::OutputTransform::Kind>("result");

    QTest::addRow("rotate-0 | rotate-0") << KWin::OutputTransform::Normal << KWin::OutputTransform::Normal << KWin::OutputTransform::Normal;
    QTest::addRow("rotate-90 | rotate-0") << KWin::OutputTransform::Rotated90 << KWin::OutputTransform::Normal << KWin::OutputTransform::Rotated90;
    QTest::addRow("rotate-180 | rotate-0") << KWin::OutputTransform::Rotated180 << KWin::OutputTransform::Normal << KWin::OutputTransform::Rotated180;
    QTest::addRow("rotate-270 | rotate-0") << KWin::OutputTransform::Rotated270 << KWin::OutputTransform::Normal << KWin::OutputTransform::Rotated270;
    QTest::addRow("flip-0 | rotate-0") << KWin::OutputTransform::Flipped << KWin::OutputTransform::Normal << KWin::OutputTransform::Flipped;
    QTest::addRow("flip-90 | rotate-0") << KWin::OutputTransform::Flipped90 << KWin::OutputTransform::Normal << KWin::OutputTransform::Flipped90;
    QTest::addRow("flip-180 | rotate-0") << KWin::OutputTransform::Flipped180 << KWin::OutputTransform::Normal << KWin::OutputTransform::Flipped180;
    QTest::addRow("flip-270 | rotate-0") << KWin::OutputTransform::Flipped270 << KWin::OutputTransform::Normal << KWin::OutputTransform::Flipped270;

    QTest::addRow("rotate-0 | rotate-90") << KWin::OutputTransform::Normal << KWin::OutputTransform::Rotated90 << KWin::OutputTransform::Rotated90;
    QTest::addRow("rotate-90 | rotate-90") << KWin::OutputTransform::Rotated90 << KWin::OutputTransform::Rotated90 << KWin::OutputTransform::Rotated180;
    QTest::addRow("rotate-180 | rotate-90") << KWin::OutputTransform::Rotated180 << KWin::OutputTransform::Rotated90 << KWin::OutputTransform::Rotated270;
    QTest::addRow("rotate-270 | rotate-90") << KWin::OutputTransform::Rotated270 << KWin::OutputTransform::Rotated90 << KWin::OutputTransform::Normal;
    QTest::addRow("flip-0 | rotate-90") << KWin::OutputTransform::Flipped << KWin::OutputTransform::Rotated90 << KWin::OutputTransform::Flipped270;
    QTest::addRow("flip-90 | rotate-90") << KWin::OutputTransform::Flipped90 << KWin::OutputTransform::Rotated90 << KWin::OutputTransform::Flipped;
    QTest::addRow("flip-180 | rotate-90") << KWin::OutputTransform::Flipped180 << KWin::OutputTransform::Rotated90 << KWin::OutputTransform::Flipped90;
    QTest::addRow("flip-270 | rotate-90") << KWin::OutputTransform::Flipped270 << KWin::OutputTransform::Rotated90 << KWin::OutputTransform::Flipped180;

    QTest::addRow("rotate-0 | rotate-180") << KWin::OutputTransform::Normal << KWin::OutputTransform::Rotated180 << KWin::OutputTransform::Rotated180;
    QTest::addRow("rotate-90 | rotate-180") << KWin::OutputTransform::Rotated90 << KWin::OutputTransform::Rotated180 << KWin::OutputTransform::Rotated270;
    QTest::addRow("rotate-180 | rotate-180") << KWin::OutputTransform::Rotated180 << KWin::OutputTransform::Rotated180 << KWin::OutputTransform::Normal;
    QTest::addRow("rotate-270 | rotate-180") << KWin::OutputTransform::Rotated270 << KWin::OutputTransform::Rotated180 << KWin::OutputTransform::Rotated90;
    QTest::addRow("flip-0 | rotate-180") << KWin::OutputTransform::Flipped << KWin::OutputTransform::Rotated180 << KWin::OutputTransform::Flipped180;
    QTest::addRow("flip-90 | rotate-180") << KWin::OutputTransform::Flipped90 << KWin::OutputTransform::Rotated180 << KWin::OutputTransform::Flipped270;
    QTest::addRow("flip-180 | rotate-180") << KWin::OutputTransform::Flipped180 << KWin::OutputTransform::Rotated180 << KWin::OutputTransform::Flipped;
    QTest::addRow("flip-270 | rotate-180") << KWin::OutputTransform::Flipped270 << KWin::OutputTransform::Rotated180 << KWin::OutputTransform::Flipped90;

    QTest::addRow("rotate-0 | rotate-270") << KWin::OutputTransform::Normal << KWin::OutputTransform::Rotated270 << KWin::OutputTransform::Rotated270;
    QTest::addRow("rotate-90 | rotate-270") << KWin::OutputTransform::Rotated90 << KWin::OutputTransform::Rotated270 << KWin::OutputTransform::Normal;
    QTest::addRow("rotate-180 | rotate-270") << KWin::OutputTransform::Rotated180 << KWin::OutputTransform::Rotated270 << KWin::OutputTransform::Rotated90;
    QTest::addRow("rotate-270 | rotate-270") << KWin::OutputTransform::Rotated270 << KWin::OutputTransform::Rotated270 << KWin::OutputTransform::Rotated180;
    QTest::addRow("flip-0 | rotate-270") << KWin::OutputTransform::Flipped << KWin::OutputTransform::Rotated270 << KWin::OutputTransform::Flipped90;
    QTest::addRow("flip-90 | rotate-270") << KWin::OutputTransform::Flipped90 << KWin::OutputTransform::Rotated270 << KWin::OutputTransform::Flipped180;
    QTest::addRow("flip-180 | rotate-270") << KWin::OutputTransform::Flipped180 << KWin::OutputTransform::Rotated270 << KWin::OutputTransform::Flipped270;
    QTest::addRow("flip-270 | rotate-270") << KWin::OutputTransform::Flipped270 << KWin::OutputTransform::Rotated270 << KWin::OutputTransform::Flipped;

    QTest::addRow("rotate-0 | flip-0") << KWin::OutputTransform::Normal << KWin::OutputTransform::Flipped << KWin::OutputTransform::Flipped;
    QTest::addRow("rotate-90 | flip-0") << KWin::OutputTransform::Rotated90 << KWin::OutputTransform::Flipped << KWin::OutputTransform::Flipped90;
    QTest::addRow("rotate-180 | flip-0") << KWin::OutputTransform::Rotated180 << KWin::OutputTransform::Flipped << KWin::OutputTransform::Flipped180;
    QTest::addRow("rotate-270 | flip-0") << KWin::OutputTransform::Rotated270 << KWin::OutputTransform::Flipped << KWin::OutputTransform::Flipped270;
    QTest::addRow("flip-0 | flip-0") << KWin::OutputTransform::Flipped << KWin::OutputTransform::Flipped << KWin::OutputTransform::Normal;
    QTest::addRow("flip-90 | flip-0") << KWin::OutputTransform::Flipped90 << KWin::OutputTransform::Flipped << KWin::OutputTransform::Rotated90;
    QTest::addRow("flip-180 | flip-0") << KWin::OutputTransform::Flipped180 << KWin::OutputTransform::Flipped << KWin::OutputTransform::Rotated180;
    QTest::addRow("flip-270 | flip-0") << KWin::OutputTransform::Flipped270 << KWin::OutputTransform::Flipped << KWin::OutputTransform::Rotated270;

    QTest::addRow("rotate-0 | flip-90") << KWin::OutputTransform::Normal << KWin::OutputTransform::Flipped90 << KWin::OutputTransform::Flipped90;
    QTest::addRow("rotate-90 | flip-90") << KWin::OutputTransform::Rotated90 << KWin::OutputTransform::Flipped90 << KWin::OutputTransform::Flipped180;
    QTest::addRow("rotate-180 | flip-90") << KWin::OutputTransform::Rotated180 << KWin::OutputTransform::Flipped90 << KWin::OutputTransform::Flipped270;
    QTest::addRow("rotate-270 | flip-90") << KWin::OutputTransform::Rotated270 << KWin::OutputTransform::Flipped90 << KWin::OutputTransform::Flipped;
    QTest::addRow("flip-0 | flip-90") << KWin::OutputTransform::Flipped << KWin::OutputTransform::Flipped90 << KWin::OutputTransform::Rotated270;
    QTest::addRow("flip-90 | flip-90") << KWin::OutputTransform::Flipped90 << KWin::OutputTransform::Flipped90 << KWin::OutputTransform::Normal;
    QTest::addRow("flip-180 | flip-90") << KWin::OutputTransform::Flipped180 << KWin::OutputTransform::Flipped90 << KWin::OutputTransform::Rotated90;
    QTest::addRow("flip-270 | flip-90") << KWin::OutputTransform::Flipped270 << KWin::OutputTransform::Flipped90 << KWin::OutputTransform::Rotated180;

    QTest::addRow("rotate-0 | flip-180") << KWin::OutputTransform::Normal << KWin::OutputTransform::Flipped180 << KWin::OutputTransform::Flipped180;
    QTest::addRow("rotate-90 | flip-180") << KWin::OutputTransform::Rotated90 << KWin::OutputTransform::Flipped180 << KWin::OutputTransform::Flipped270;
    QTest::addRow("rotate-180 | flip-180") << KWin::OutputTransform::Rotated180 << KWin::OutputTransform::Flipped180 << KWin::OutputTransform::Flipped;
    QTest::addRow("rotate-270 | flip-180") << KWin::OutputTransform::Rotated270 << KWin::OutputTransform::Flipped180 << KWin::OutputTransform::Flipped90;
    QTest::addRow("flip-0 | flip-180") << KWin::OutputTransform::Flipped << KWin::OutputTransform::Flipped180 << KWin::OutputTransform::Rotated180;
    QTest::addRow("flip-90 | flip-180") << KWin::OutputTransform::Flipped90 << KWin::OutputTransform::Flipped180 << KWin::OutputTransform::Rotated270;
    QTest::addRow("flip-180 | flip-180") << KWin::OutputTransform::Flipped180 << KWin::OutputTransform::Flipped180 << KWin::OutputTransform::Normal;
    QTest::addRow("flip-270 | flip-180") << KWin::OutputTransform::Flipped270 << KWin::OutputTransform::Flipped180 << KWin::OutputTransform::Rotated90;

    QTest::addRow("rotate-0 | flip-270") << KWin::OutputTransform::Normal << KWin::OutputTransform::Flipped270 << KWin::OutputTransform::Flipped270;
    QTest::addRow("rotate-90 | flip-270") << KWin::OutputTransform::Rotated90 << KWin::OutputTransform::Flipped270 << KWin::OutputTransform::Flipped;
    QTest::addRow("rotate-180 | flip-270") << KWin::OutputTransform::Rotated180 << KWin::OutputTransform::Flipped270 << KWin::OutputTransform::Flipped90;
    QTest::addRow("rotate-270 | flip-270") << KWin::OutputTransform::Rotated270 << KWin::OutputTransform::Flipped270 << KWin::OutputTransform::Flipped180;
    QTest::addRow("flip-0 | flip-270") << KWin::OutputTransform::Flipped << KWin::OutputTransform::Flipped270 << KWin::OutputTransform::Rotated90;
    QTest::addRow("flip-90 | flip-270") << KWin::OutputTransform::Flipped90 << KWin::OutputTransform::Flipped270 << KWin::OutputTransform::Rotated180;
    QTest::addRow("flip-180 | flip-270") << KWin::OutputTransform::Flipped180 << KWin::OutputTransform::Flipped270 << KWin::OutputTransform::Rotated270;
    QTest::addRow("flip-270 | flip-270") << KWin::OutputTransform::Flipped270 << KWin::OutputTransform::Flipped270 << KWin::OutputTransform::Normal;
}

void TestOutputTransform::combine()
{
    QFETCH(KWin::OutputTransform::Kind, first);
    QFETCH(KWin::OutputTransform::Kind, second);
    QFETCH(KWin::OutputTransform::Kind, result);

    const KWin::OutputTransform firstTransform(first);
    const KWin::OutputTransform secondTransform(second);
    const KWin::OutputTransform combinedTransform = firstTransform.combine(secondTransform);
    QCOMPARE(combinedTransform.kind(), result);

    const QRectF box(10, 20, 30, 40);
    const QSizeF bounds(100, 200);
    QCOMPARE(combinedTransform.map(box, bounds), secondTransform.map(firstTransform.map(box, bounds), firstTransform.map(bounds)));
}

QTEST_MAIN(TestOutputTransform)

#include "output_transform_test.moc"
