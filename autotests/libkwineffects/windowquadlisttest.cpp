/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2013 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include <QTest>
#include <kwineffects.h>

Q_DECLARE_METATYPE(KWin::WindowQuadList)

class WindowQuadListTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void testMakeGrid_data();
    void testMakeGrid();
    void testMakeRegularGrid_data();
    void testMakeRegularGrid();

private:
    KWin::WindowQuad makeQuad(const QRectF &rect);
};

KWin::WindowQuad WindowQuadListTest::makeQuad(const QRectF &r)
{
    KWin::WindowQuad quad;
    quad[0] = KWin::WindowVertex(r.x(), r.y(), r.x(), r.y());
    quad[1] = KWin::WindowVertex(r.x() + r.width(), r.y(), r.x() + r.width(), r.y());
    quad[2] = KWin::WindowVertex(r.x() + r.width(), r.y() + r.height(), r.x() + r.width(), r.y() + r.height());
    quad[3] = KWin::WindowVertex(r.x(), r.y() + r.height(), r.x(), r.y() + r.height());
    return quad;
}

void WindowQuadListTest::testMakeGrid_data()
{
    QTest::addColumn<KWin::WindowQuadList>("orig");
    QTest::addColumn<int>("quadSize");
    QTest::addColumn<int>("expectedCount");
    QTest::addColumn<KWin::WindowQuadList>("expected");

    KWin::WindowQuadList orig;
    KWin::WindowQuadList expected;

    QTest::newRow("empty") << orig << 10 << 0 << expected;

    orig.append(makeQuad(QRectF(0, 0, 10, 10)));
    expected.append(makeQuad(QRectF(0, 0, 10, 10)));
    QTest::newRow("quadSizeTooLarge") << orig << 10 << 1 << expected;

    expected.clear();
    expected.append(makeQuad(QRectF(0, 0, 5, 5)));
    expected.append(makeQuad(QRectF(0, 5, 5, 5)));
    expected.append(makeQuad(QRectF(5, 0, 5, 5)));
    expected.append(makeQuad(QRectF(5, 5, 5, 5)));
    QTest::newRow("regularGrid") << orig << 5 << 4 << expected;

    expected.clear();
    expected.append(makeQuad(QRectF(0, 0, 9, 9)));
    expected.append(makeQuad(QRectF(0, 9, 9, 1)));
    expected.append(makeQuad(QRectF(9, 0, 1, 9)));
    expected.append(makeQuad(QRectF(9, 9, 1, 1)));
    QTest::newRow("irregularGrid") << orig << 9 << 4 << expected;

    orig.append(makeQuad(QRectF(0, 10, 4, 3)));
    expected.clear();
    expected.append(makeQuad(QRectF(0, 0, 4, 4)));
    expected.append(makeQuad(QRectF(0, 4, 4, 4)));
    expected.append(makeQuad(QRectF(0, 8, 4, 2)));
    expected.append(makeQuad(QRectF(0, 10, 4, 2)));
    expected.append(makeQuad(QRectF(0, 12, 4, 1)));
    expected.append(makeQuad(QRectF(4, 0, 4, 4)));
    expected.append(makeQuad(QRectF(4, 4, 4, 4)));
    expected.append(makeQuad(QRectF(4, 8, 4, 2)));
    expected.append(makeQuad(QRectF(8, 0, 2, 4)));
    expected.append(makeQuad(QRectF(8, 4, 2, 4)));
    expected.append(makeQuad(QRectF(8, 8, 2, 2)));
    QTest::newRow("irregularGrid2") << orig << 4 << 11 << expected;
}

void WindowQuadListTest::testMakeGrid()
{
    QFETCH(KWin::WindowQuadList, orig);
    QFETCH(int, quadSize);
    QFETCH(int, expectedCount);
    KWin::WindowQuadList actual = orig.makeGrid(quadSize);
    QCOMPARE(actual.count(), expectedCount);

    QFETCH(KWin::WindowQuadList, expected);
    for (auto it = actual.constBegin(); it != actual.constEnd(); ++it) {
        bool found = false;
        const KWin::WindowQuad &actualQuad = (*it);
        for (auto it2 = expected.constBegin(); it2 != expected.constEnd(); ++it2) {
            const KWin::WindowQuad &expectedQuad = (*it2);
            auto vertexTest = [actualQuad, expectedQuad](int index) {
                const KWin::WindowVertex &actualVertex = actualQuad[index];
                const KWin::WindowVertex &expectedVertex = expectedQuad[index];
                if (actualVertex.x() != expectedVertex.x()) {
                    return false;
                }
                if (actualVertex.y() != expectedVertex.y()) {
                    return false;
                }
                if (!qFuzzyIsNull(actualVertex.u() - expectedVertex.u())) {
                    return false;
                }
                if (!qFuzzyIsNull(actualVertex.v() - expectedVertex.v())) {
                    return false;
                }
                return true;
            };
            found = vertexTest(0) && vertexTest(1) && vertexTest(2) && vertexTest(3);
            if (found) {
                break;
            }
        }
        QVERIFY2(found, qPrintable(QStringLiteral("%0, %1 / %2, %3").arg(QString::number(actualQuad.left()), QString::number(actualQuad.top()), QString::number(actualQuad.right()), QString::number(actualQuad.bottom()))));
    }
}

void WindowQuadListTest::testMakeRegularGrid_data()
{
    QTest::addColumn<KWin::WindowQuadList>("orig");
    QTest::addColumn<int>("xSubdivisions");
    QTest::addColumn<int>("ySubdivisions");
    QTest::addColumn<int>("expectedCount");
    QTest::addColumn<KWin::WindowQuadList>("expected");

    KWin::WindowQuadList orig;
    KWin::WindowQuadList expected;

    QTest::newRow("empty") << orig << 1 << 1 << 0 << expected;

    orig.append(makeQuad(QRectF(0, 0, 10, 10)));
    expected.append(makeQuad(QRectF(0, 0, 10, 10)));
    QTest::newRow("noSplit") << orig << 1 << 1 << 1 << expected;

    expected.clear();
    expected.append(makeQuad(QRectF(0, 0, 5, 10)));
    expected.append(makeQuad(QRectF(5, 0, 5, 10)));
    QTest::newRow("xSplit") << orig << 2 << 1 << 2 << expected;

    expected.clear();
    expected.append(makeQuad(QRectF(0, 0, 10, 5)));
    expected.append(makeQuad(QRectF(0, 5, 10, 5)));
    QTest::newRow("ySplit") << orig << 1 << 2 << 2 << expected;

    expected.clear();
    expected.append(makeQuad(QRectF(0, 0, 5, 5)));
    expected.append(makeQuad(QRectF(5, 0, 5, 5)));
    expected.append(makeQuad(QRectF(0, 5, 5, 5)));
    expected.append(makeQuad(QRectF(5, 5, 5, 5)));
    QTest::newRow("xySplit") << orig << 2 << 2 << 4 << expected;

    orig.append(makeQuad(QRectF(0, 10, 4, 2)));
    expected.clear();
    expected.append(makeQuad(QRectF(0, 0, 5, 3)));
    expected.append(makeQuad(QRectF(5, 0, 5, 3)));
    expected.append(makeQuad(QRectF(0, 3, 5, 3)));
    expected.append(makeQuad(QRectF(5, 3, 5, 3)));
    expected.append(makeQuad(QRectF(0, 6, 5, 3)));
    expected.append(makeQuad(QRectF(5, 6, 5, 3)));
    expected.append(makeQuad(QRectF(0, 9, 5, 1)));
    expected.append(makeQuad(QRectF(0, 10, 4, 2)));
    expected.append(makeQuad(QRectF(5, 9, 5, 1)));
    QTest::newRow("multipleQuads") << orig << 2 << 4 << 9 << expected;
}

void WindowQuadListTest::testMakeRegularGrid()
{
    QFETCH(KWin::WindowQuadList, orig);
    QFETCH(int, xSubdivisions);
    QFETCH(int, ySubdivisions);
    QFETCH(int, expectedCount);
    KWin::WindowQuadList actual = orig.makeRegularGrid(xSubdivisions, ySubdivisions);
    QCOMPARE(actual.count(), expectedCount);

    QFETCH(KWin::WindowQuadList, expected);
    for (auto it = actual.constBegin(); it != actual.constEnd(); ++it) {
        bool found = false;
        const KWin::WindowQuad &actualQuad = (*it);
        for (auto it2 = expected.constBegin(); it2 != expected.constEnd(); ++it2) {
            const KWin::WindowQuad &expectedQuad = (*it2);
            auto vertexTest = [actualQuad, expectedQuad](int index) {
                const KWin::WindowVertex &actualVertex = actualQuad[index];
                const KWin::WindowVertex &expectedVertex = expectedQuad[index];
                if (actualVertex.x() != expectedVertex.x()) {
                    return false;
                }
                if (actualVertex.y() != expectedVertex.y()) {
                    return false;
                }
                if (!qFuzzyIsNull(actualVertex.u() - expectedVertex.u())) {
                    return false;
                }
                if (!qFuzzyIsNull(actualVertex.v() - expectedVertex.v())) {
                    return false;
                }
                return true;
            };
            found = vertexTest(0) && vertexTest(1) && vertexTest(2) && vertexTest(3);
            if (found) {
                break;
            }
        }
        QVERIFY2(found, qPrintable(QStringLiteral("%0, %1 / %2, %3").arg(QString::number(actualQuad.left()), QString::number(actualQuad.top()), QString::number(actualQuad.right()), QString::number(actualQuad.bottom()))));
    }
}

QTEST_MAIN(WindowQuadListTest)

#include "windowquadlisttest.moc"
