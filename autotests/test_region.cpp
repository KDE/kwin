/*
    SPDX-FileCopyrightText: 2025 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include <QTest>

#include "core/region.h"

using namespace KWin;

static int xyToPow2(int x, int y, int stride)
{
    return std::pow(2, y * stride + x);
}

static QMap<int, Rect> enumerateRects(const QSize &gridSize, const QSize &unit = QSize(5, 5))
{
    Q_ASSERT(gridSize.width() * gridSize.height() <= sizeof(int) * 4);

    QList<int> sumAreaTable;
    sumAreaTable.resize((gridSize.width() + 1) * (gridSize.height() + 1));

    const int satStride = gridSize.width() + 1;
    for (int x = 0; x <= gridSize.width(); ++x) {
        sumAreaTable[x] = 0;
    }

    for (int y = 0; y < gridSize.height(); ++y) {
        sumAreaTable[(y + 1) * satStride] = 0;

        for (int x = 0; x < gridSize.width(); ++x) {
            sumAreaTable[(y + 1) * satStride + x + 1] = sumAreaTable[(y + 1) * satStride + x]
                - sumAreaTable[y * satStride + x] + sumAreaTable[y * satStride + x + 1] + xyToPow2(x, y, gridSize.width());
        }
    }

    QMap<int, Rect> rects;
    rects.insert(0, Rect());

    for (int y1 = 0; y1 < gridSize.height(); ++y1) {
        for (int y2 = y1 + 1; y2 <= gridSize.height(); ++y2) {
            for (int x1 = 0; x1 < gridSize.width(); ++x1) {
                for (int x2 = x1 + 1; x2 <= gridSize.width(); ++x2) {
                    const int a = y1 * satStride + x1;
                    const int b = y1 * satStride + x2;
                    const int c = y2 * satStride + x1;
                    const int d = y2 * satStride + x2;
                    const int variant = sumAreaTable[d] + sumAreaTable[a] - sumAreaTable[b] - sumAreaTable[c];
                    rects.insert(variant, Rect(QPoint(x1 * unit.width(), y1 * unit.height()), QPoint(x2 * unit.width(), y2 * unit.height())));
                }
            }
        }
    }

    return rects;
}

static int coalesce(QList<Rect> &rects, int previousStart, int currentStart)
{
    const int previousCount = currentStart - previousStart;
    const int currentCount = rects.size() - currentStart;
    if (!currentCount || previousCount != currentCount) {
        return currentStart;
    }

    if (rects[previousStart].bottom() != rects[currentStart].top()) {
        return currentStart;
    }

    for (int i = 0; i < currentCount; ++i) {
        const Rect &a = rects[previousStart + i];
        const Rect &b = rects[currentStart + i];
        if (a.left() != b.left() || a.right() != b.right()) {
            return currentStart;
        }
    }

    const int currentBottom = rects[currentStart].bottom();
    for (int i = 0; i < previousCount; ++i) {
        rects[previousStart + i].setBottom(currentBottom);
    }

    rects.remove(currentStart, currentCount);
    return previousStart;
}

/**
 * Returns a region that corresponds to the pattern indicated by the @a bits argument.
 *
 * The regions are sampled from a two dimensional grid of size @a xSize by @a ySize. For example,
 * consider a 3x3 grid:
 *
 *  a b c
 *  d e f
 *  g h i
 *
 * Every item in the grid has a bit assigned to it, "a" is assigned to bit 0, "b" is assigned to
 * bit 1, and so on. Then numbers between 0 and 2 ^ (xSize * ySize) describe all possible combinations
 * of rectangles that can be used to construct a region.
 *
 * Also note that bit operations such as OR, XOR, and AND on bit patterns directly map to the
 * corresponding region operations.
 */
static Region bitsToRegion(int bits, const QSize &gridSize, const QSize &unit = QSize(5, 5))
{
    QList<Rect> rects;
    rects.reserve(gridSize.width() * gridSize.height());

    int current = 0;
    for (int i = 0; i < gridSize.height(); ++i) {
        std::optional<Rect> currentRect;

        const int previous = current;
        current = rects.size();

        for (int j = 0; j < gridSize.width(); ++j) {
            const int bitIndex = i * gridSize.width() + j;
            const int bitValue = bits & (1 << bitIndex);

            if (bitValue) {
                if (currentRect) {
                    currentRect->setRight((j + 1) * unit.width());
                } else {
                    currentRect = Rect(j * unit.width(), i * unit.height(), unit.width(), unit.height());
                }
            } else if (currentRect) {
                rects.append(*currentRect);
                currentRect = std::nullopt;
            }
        }

        if (currentRect) {
            rects.append(*currentRect);
        }

        current = coalesce(rects, previous, current);
    }

    return Region::fromSortedRects(rects);
}

static QList<Region> enumerateRegions(const QSize &gridSize, const QSize &unit = QSize(5, 5))
{
    Q_ASSERT(gridSize.width() * gridSize.height() <= sizeof(int) * 4);
    const int maxVariations = std::pow(2, gridSize.width() * gridSize.height());

    QList<Region> regions;
    regions.reserve(maxVariations);
    for (int i = 0; i < maxVariations; ++i) {
        regions.append(bitsToRegion(i, gridSize, unit));
    }

    return regions;
}

static QSize testGridSize()
{
    const QString text = qEnvironmentVariable("KWIN_TEST_REGION_GRID_SIZE");
    if (text.isEmpty()) {
        return QSize(3, 3);
    }

    const qsizetype x = text.indexOf('x');
    Q_ASSERT(x != -1);

    return QSize(text.left(x).toInt(), text.mid(x + 1).toInt());
}

class TestRegion : public QObject
{
    Q_OBJECT

public:
    TestRegion() = default;

private Q_SLOTS:
    void equals_data();
    void equals();
    void notEquals_data();
    void notEquals();
    void empty_data();
    void empty();
    void boundingRect_data();
    void boundingRect();
    void containsRect();
    void containsPoint_data();
    void containsPoint();
    void intersectsRect();
    void intersectsRegion();
    void united();
    void unitedRect();
    void subtracted();
    void subtractedRect();
    void xored();
    void xoredRect();
    void intersected();
    void intersectedRect();
    void translated_data();
    void translated();
    void scaledAndRoundedOut_data();
    void scaledAndRoundedOut();
    void fromSortedRects();
    void fromUnsortedRects();
    void fromRectsSortedByY();
    void fromAndToQRegion_data();
    void fromAndToQRegion();

private:
    const QSize gridSize = testGridSize();

    const QList<Region> m_regions = enumerateRegions(gridSize);
    const QMap<int, Rect> m_rects = enumerateRects(gridSize);
};

void TestRegion::equals_data()
{
    QTest::addColumn<Region>("region1");
    QTest::addColumn<Region>("region2");
    QTest::addColumn<bool>("expected");

    QTest::addRow("default and default") << Region() << Region() << true;
    QTest::addRow("(1,2 3x4) and default") << Region(Rect(1, 2, 3, 4)) << Region() << false;
    QTest::addRow("(1,2 3x4) and (1,2 3x4)") << Region(Rect(1, 2, 3, 4)) << Region(Rect(1, 2, 3, 4)) << true;

    QTest::addRow("[(1,2 3x4), (5,6 7x8)] and (1,2 3x4)") << (Region(Rect(1, 2, 3, 4)) | Region(Rect(5, 6, 7, 8))) << Region(Rect(1, 2, 3, 4)) << false;
    QTest::addRow("[(1,2 3x4), (5,6 7x8)] and (5,6 7x8)") << (Region(Rect(1, 2, 3, 4)) | Region(Rect(5, 6, 7, 8))) << Region(Rect(5, 6, 7, 8)) << false;
    QTest::addRow("[(1,2 3x4), (5,6 7x8)] and [(1,2 3x4), (5,6 7x8)]") << (Region(Rect(1, 2, 3, 4)) | Region(Rect(5, 6, 7, 8))) << (Region(Rect(1, 2, 3, 4)) | Region(Rect(5, 6, 7, 8))) << true;
}

void TestRegion::equals()
{
    QFETCH(Region, region1);
    QFETCH(Region, region2);
    QFETCH(bool, expected);

    QCOMPARE(region1 == region2, expected);
    QCOMPARE(region2 == region1, expected);
}

void TestRegion::notEquals_data()
{
    QTest::addColumn<Region>("region1");
    QTest::addColumn<Region>("region2");
    QTest::addColumn<bool>("expected");

    QTest::addRow("default and default") << Region() << Region() << false;
    QTest::addRow("(1,2 3x4) and default") << Region(Rect(1, 2, 3, 4)) << Region() << true;
    QTest::addRow("(1,2 3x4) and (1,2 3x4)") << Region(Rect(1, 2, 3, 4)) << Region(Rect(1, 2, 3, 4)) << false;

    QTest::addRow("[(1,2 3x4), (5,6 7x8)] and (1,2 3x4)") << (Region(Rect(1, 2, 3, 4)) | Region(Rect(5, 6, 7, 8))) << Region(Rect(1, 2, 3, 4)) << true;
    QTest::addRow("[(1,2 3x4), (5,6 7x8)] and (5,6 7x8)") << (Region(Rect(1, 2, 3, 4)) | Region(Rect(5, 6, 7, 8))) << Region(Rect(5, 6, 7, 8)) << true;
    QTest::addRow("[(1,2 3x4), (5,6 7x8)] and [(1,2 3x4), (5,6 7x8)]") << (Region(Rect(1, 2, 3, 4)) | Region(Rect(5, 6, 7, 8))) << (Region(Rect(1, 2, 3, 4)) | Region(Rect(5, 6, 7, 8))) << false;
}

void TestRegion::notEquals()
{
    QFETCH(Region, region1);
    QFETCH(Region, region2);
    QFETCH(bool, expected);

    QCOMPARE(region1 != region2, expected);
    QCOMPARE(region2 != region1, expected);
}

void TestRegion::empty_data()
{
    QTest::addColumn<Region>("region");
    QTest::addColumn<bool>("empty");

    QTest::addRow("default") << Region() << true;
    QTest::addRow("0,0 0x0") << Region(0, 0, 0, 0) << true;
    QTest::addRow("0,0 1x0") << Region(0, 0, 1, 0) << true;
    QTest::addRow("0,0 0x1") << Region(0, 0, 0, 1) << true;
    QTest::addRow("0,0 1x1") << Region(0, 0, 1, 1) << false;
}

void TestRegion::empty()
{
    QFETCH(Region, region);

    QTEST(region.isEmpty(), "empty");
}

void TestRegion::boundingRect_data()
{
    QTest::addColumn<Region>("region");
    QTest::addColumn<Rect>("boundingRect");

    QTest::addRow("default") << Region() << Rect();
    QTest::addRow("(1,2 0x0)") << Region(1, 2, 0, 0) << Rect(1, 2, 0, 0);
    QTest::addRow("(1,2 3x4)") << Region(1, 2, 3, 4) << Rect(1, 2, 3, 4);
    QTest::addRow("[(1,2 3x4), (5,6 7x8)]") << (Region(1, 2, 3, 4) | Region(5, 6, 7, 8)) << Rect(1, 2, 11, 12);
}

void TestRegion::boundingRect()
{
    QFETCH(Region, region);
    QTEST(region.boundingRect(), "boundingRect");
}

void TestRegion::containsRect()
{
    for (int i = 0; i < m_regions.size(); ++i) {
        for (const auto &[j, rect] : m_rects.asKeyValueRange()) {
            QCOMPARE(m_regions[i].contains(rect), j && (i & j) == j);
        }
    }
}

void TestRegion::containsPoint_data()
{
    QTest::addColumn<Region>("region");
    QTest::addColumn<QPoint>("point");
    QTest::addColumn<bool>("contains");

    QTest::addRow("empty region contains 0,0") << Region() << QPoint(0, 0) << false;
    QTest::addRow("empty region contains 1,1") << Region() << QPoint(1, 1) << false;

    const Region simpleRegion = Region(0, 0, 5, 5);
    {
        QTest::addRow("above the simple region") << simpleRegion << QPoint(0, -1) << false;
        QTest::addRow("below the simple region") << simpleRegion << QPoint(0, 6) << false;
        QTest::addRow("to the left of the simple region") << simpleRegion << QPoint(-1, 0) << false;
        QTest::addRow("to the right of the simple region") << simpleRegion << QPoint(6, 0) << false;

        QTest::addRow("at the top edge of the simple region") << simpleRegion << QPoint(2, 0) << true;
        QTest::addRow("at the bottom edge of the simple region") << simpleRegion << QPoint(2, 5) << false;
        QTest::addRow("at the left edge of the simple region") << simpleRegion << QPoint(0, 2) << true;
        QTest::addRow("at the right edge of the simple region") << simpleRegion << QPoint(5, 2) << false;

        QTest::addRow("inside the simple region") << simpleRegion << QPoint(2, 2) << true;
    }

    const Region complexRegion = Region(0, 0, 10, 6) | Region(20, 0, 10, 6) | Region(0, 12, 10, 6) | Region(20, 12, 10, 6);
    {
        QTest::addRow("above the top-left rect in the complex region") << complexRegion << QPoint(5, -1) << false;
        QTest::addRow("below the top-left rect in the complex region") << complexRegion << QPoint(5, 7) << false;
        QTest::addRow("to the left of the top-left rect in the complex region") << complexRegion << QPoint(-1, 3) << false;
        QTest::addRow("to the right of the top-left rect in the complex region") << complexRegion << QPoint(11, 3) << false;

        QTest::addRow("at the top edge of the top-left rect in the complex region") << complexRegion << QPoint(5, 0) << true;
        QTest::addRow("at the bottom edge of the top-left rect in the complex region") << complexRegion << QPoint(5, 6) << false;
        QTest::addRow("at the left edge of the top-left rect in the complex region") << complexRegion << QPoint(0, 3) << true;
        QTest::addRow("at the right edge of the top-left rect in the complex region") << complexRegion << QPoint(10, 3) << false;

        QTest::addRow("inside the top-left rect in the complex region") << complexRegion << QPoint(5, 3) << true;
    }

    {
        QTest::addRow("above the top-right rect in the complex region") << complexRegion << QPoint(25, -1) << false;
        QTest::addRow("below the top-right rect in the complex region") << complexRegion << QPoint(25, 7) << false;
        QTest::addRow("to the left of the top-right rect in the complex region") << complexRegion << QPoint(19, 3) << false;
        QTest::addRow("to the right of the top-right rect in the complex region") << complexRegion << QPoint(31, 3) << false;

        QTest::addRow("at the top edge of the top-right rect in the complex region") << complexRegion << QPoint(25, 0) << true;
        QTest::addRow("at the bottom edge of the top-right rect in the complex region") << complexRegion << QPoint(25, 6) << false;
        QTest::addRow("at the left edge of the top-right rect in the complex region") << complexRegion << QPoint(20, 3) << true;
        QTest::addRow("at the right edge of the top-right rect in the complex region") << complexRegion << QPoint(30, 3) << false;

        QTest::addRow("inside the top-right rect in the complex region") << complexRegion << QPoint(25, 3) << true;
    }

    {
        QTest::addRow("above the bottom-left rect in the complex region") << complexRegion << QPoint(5, 11) << false;
        QTest::addRow("below the bottom-left rect in the complex region") << complexRegion << QPoint(5, 19) << false;
        QTest::addRow("to the left of the bottom-left rect in the complex region") << complexRegion << QPoint(-1, 15) << false;
        QTest::addRow("to the right of the bottom-left rect in the complex region") << complexRegion << QPoint(11, 15) << false;

        QTest::addRow("at the top edge of the bottom-left rect in the complex region") << complexRegion << QPoint(5, 12) << true;
        QTest::addRow("at the bottom edge of the bottom-left rect in the complex region") << complexRegion << QPoint(5, 18) << false;
        QTest::addRow("at the left edge of the bottom-left rect in the complex region") << complexRegion << QPoint(0, 15) << true;
        QTest::addRow("at the right edge of the bottom-left rect in the complex region") << complexRegion << QPoint(10, 15) << false;

        QTest::addRow("inside the bottom-left rect in the complex region") << complexRegion << QPoint(5, 15) << true;
    }

    {
        QTest::addRow("above the bottom-right rect in the complex region") << complexRegion << QPoint(25, 11) << false;
        QTest::addRow("below the bottom-right rect in the complex region") << complexRegion << QPoint(25, 19) << false;
        QTest::addRow("to the left of the bottom-right rect in the complex region") << complexRegion << QPoint(19, 15) << false;
        QTest::addRow("to the right of the bottom-right rect in the complex region") << complexRegion << QPoint(31, 15) << false;

        QTest::addRow("at the top edge of the bottom-right rect in the complex region") << complexRegion << QPoint(25, 12) << true;
        QTest::addRow("at the bottom edge of the bottom-right rect in the complex region") << complexRegion << QPoint(25, 18) << false;
        QTest::addRow("at the left edge of the bottom-right rect in the complex region") << complexRegion << QPoint(20, 15) << true;
        QTest::addRow("at the right edge of the bottom-right rect in the complex region") << complexRegion << QPoint(30, 15) << false;

        QTest::addRow("inside the bottom-right rect in the complex region") << complexRegion << QPoint(25, 15) << true;
    }

    QTest::addRow("above the gap between top-left and top-right rects in the complex region") << complexRegion << QPoint(15, -1) << false;
    QTest::addRow("below the gap between bottom-left and bottom-right rects in the complex region") << complexRegion << QPoint(15, 19) << false;
    QTest::addRow("to the left of the gap between top-left and bottom-left rects in the complex region") << complexRegion << QPoint(-1, 9) << false;
    QTest::addRow("to the right of the gap between top-right and bottom-right rects in the complex region") << complexRegion << QPoint(31, 9) << false;
    QTest::addRow("inside gap between four rects in the complex region") << complexRegion << QPoint(15, 9) << false;
}

void TestRegion::containsPoint()
{
    QFETCH(Region, region);
    QFETCH(QPoint, point);
    QTEST(region.contains(point), "contains");
}

void TestRegion::intersectsRect()
{
    for (int i = 0; i < m_regions.size(); ++i) {
        for (const auto &[j, rect] : m_rects.asKeyValueRange()) {
            QCOMPARE(m_regions[i].intersects(rect), bool(i & j));
        }
    }
}

void TestRegion::intersectsRegion()
{
    for (int i = 0; i < m_regions.size(); ++i) {
        for (int j = 0; j <= i; ++j) {
            QCOMPARE(m_regions[i].intersects(m_regions[j]), bool(i & j));
            QCOMPARE(m_regions[j].intersects(m_regions[i]), bool(i & j));
        }
    }
}

void TestRegion::united()
{
    for (int i = 0; i < m_regions.size(); ++i) {
        for (int j = 0; j <= i; ++j) {
            const Region expected = m_regions[i | j];
            QCOMPARE(m_regions[i].united(m_regions[j]), expected);
            QCOMPARE(m_regions[j].united(m_regions[i]), expected);
        }
    }
}

void TestRegion::unitedRect()
{
    for (int i = 0; i < m_regions.size(); ++i) {
        for (const auto &[j, rect] : m_rects.asKeyValueRange()) {
            const Region expected = m_regions[i | j];
            QCOMPARE(m_regions[i].united(rect), expected);
        }
    }
}

void TestRegion::subtracted()
{
    for (int i = 0; i < m_regions.size(); ++i) {
        for (int j = 0; j <= i; ++j) {
            QCOMPARE(m_regions[i].subtracted(m_regions[j]), m_regions[i & ~j]);
            QCOMPARE(m_regions[j].subtracted(m_regions[i]), m_regions[j & ~i]);
        }
    }
}

void TestRegion::subtractedRect()
{
    for (int i = 0; i < m_regions.size(); ++i) {
        for (const auto &[j, rect] : m_rects.asKeyValueRange()) {
            const Region expected = m_regions[i & ~j];
            QCOMPARE(m_regions[i].subtracted(rect), expected);
        }
    }
}

void TestRegion::xored()
{
    for (int i = 0; i < m_regions.size(); ++i) {
        for (int j = 0; j <= i; ++j) {
            const Region expected = m_regions[i ^ j];
            QCOMPARE(m_regions[i].xored(m_regions[j]), expected);
            QCOMPARE(m_regions[j].xored(m_regions[i]), expected);
        }
    }
}

void TestRegion::xoredRect()
{
    for (int i = 0; i < m_regions.size(); ++i) {
        for (const auto &[j, rect] : m_rects.asKeyValueRange()) {
            const Region expected = m_regions[i ^ j];
            QCOMPARE(m_regions[i].xored(rect), expected);
        }
    }
}

void TestRegion::intersected()
{
    for (int i = 0; i < m_regions.size(); ++i) {
        for (int j = 0; j <= i; ++j) {
            const Region expected = m_regions[i & j];
            QCOMPARE(m_regions[i].intersected(m_regions[j]), expected);
            QCOMPARE(m_regions[j].intersected(m_regions[i]), expected);
        }
    }
}

void TestRegion::intersectedRect()
{
    for (int i = 0; i < m_regions.size(); ++i) {
        for (const auto &[j, rect] : m_rects.asKeyValueRange()) {
            const Region expected = m_regions[i & j];
            QCOMPARE(m_regions[i].intersected(rect), expected);
        }
    }
}

void TestRegion::translated_data()
{
    QTest::addColumn<Region>("region");
    QTest::addColumn<QPoint>("translation");
    QTest::addColumn<Region>("expected");

    QTest::addRow("empty") << Region() << QPoint(1, 2) << Region();
    QTest::addRow("simple") << Region(1, 2, 3, 4) << QPoint(10, 11) << Region(11, 13, 3, 4);
    QTest::addRow("complex") << (Region(1, 2, 3, 4) | Region(5, 6, 7, 8)) << QPoint(10, 11) << (Region(11, 13, 3, 4) | Region(15, 17, 7, 8));
}

void TestRegion::translated()
{
    QFETCH(Region, region);
    QFETCH(QPoint, translation);

    {
        Region translated = region;
        translated.translate(translation.x(), translation.y());
        QTEST(translated, "expected");
    }

    {
        Region translated = region;
        translated.translate(translation);
        QTEST(translated, "expected");
    }

    {
        const Region translated = region.translated(translation.x(), translation.y());
        QTEST(translated, "expected");
    }

    {
        const Region translated = region.translated(translation);
        QTEST(translated, "expected");
    }
}

void TestRegion::scaledAndRoundedOut_data()
{
    QTest::addColumn<Region>("region");
    QTest::addColumn<qreal>("scale");
    QTest::addColumn<Region>("expected");

    QTest::addRow("empty @ 1.0") << Region() << 1.0 << Region();
    QTest::addRow("empty @ 1.25") << Region() << 1.25 << Region();
    QTest::addRow("empty @ 1.5") << Region() << 1.5 << Region();
    QTest::addRow("empty @ 1.75") << Region() << 1.75 << Region();
    QTest::addRow("empty @ 2.0") << Region() << 2.0 << Region();

    const Region simple = Region(1, 2, 3, 4);
    QTest::addRow("simple @ 1.0") << simple << 1.0 << simple;
    QTest::addRow("simple @ 1.25") << simple << 1.25 << Region(1, 2, 4, 6);
    QTest::addRow("simple @ 1.5") << simple << 1.5 << Region(1, 3, 5, 6);
    QTest::addRow("simple @ 1.75") << simple << 1.75 << Region(1, 3, 6, 8);
    QTest::addRow("simple @ 2.0") << simple << 2.0 << Region(2, 4, 6, 8);

    const Region complex = Region(1, 2, 3, 4) | Region(5, 6, 7, 8);
    QTest::addRow("complex @ 1.0") << complex << 1.0 << complex;
    QTest::addRow("complex @ 1.25") << complex << 1.25 << (Region(1, 2, 4, 6) | Region(6, 7, 9, 11));
    QTest::addRow("complex @ 1.5") << complex << 1.5 << (Region(1, 3, 5, 6) | Region(7, 9, 11, 12));
    QTest::addRow("complex @ 1.75") << complex << 1.75 << (Region(1, 3, 6, 8) | Region(8, 10, 13, 15));
    QTest::addRow("complex @ 2.0") << complex << 2.0 << (Region(2, 4, 6, 8) | Region(10, 12, 14, 16));
}

void TestRegion::scaledAndRoundedOut()
{
    QFETCH(Region, region);
    QFETCH(qreal, scale);
    QTEST(region.scaledAndRoundedOut(scale), "expected");
}

template<typename T>
static QList<T> spanToList(const QSpan<const T> &span)
{
    QList<T> list;
    list.assign(span.begin(), span.end());
    return list;
}

void TestRegion::fromSortedRects()
{
    for (int i = 0; i < m_regions.size(); ++i) {
        QCOMPARE(Region::fromSortedRects(spanToList(m_regions[i].rects())), m_regions[i]);
    }
}

void TestRegion::fromUnsortedRects()
{
    for (int i = 0; i < m_regions.size(); ++i) {
        for (int j = 0; j < m_regions.size(); ++j) {
            const QList<Rect> rects = spanToList(m_regions[i].rects()) + spanToList(m_regions[j].rects());
            QCOMPARE(Region::fromUnsortedRects(rects), m_regions[i | j]);
        }
    }
}

void TestRegion::fromRectsSortedByY()
{
    for (int i = 0; i < m_regions.size(); ++i) {
        for (int j = 0; j < m_regions.size(); ++j) {
            QList<Rect> rects = spanToList(m_regions[i].rects()) + spanToList(m_regions[j].rects());
            std::sort(rects.begin(), rects.end(), [](const Rect &a, const Rect &b) {
                return a.top() < b.top();
            });

            QCOMPARE(Region::fromRectsSortedByY(rects), m_regions[i | j]);
        }
    }
}

void TestRegion::fromAndToQRegion_data()
{
    QTest::addColumn<Region>("region");
    QTest::addColumn<QRegion>("qtRegion");

    QTest::addRow("empty") << Region() << QRegion();
    QTest::addRow("simple") << Region(1, 2, 3, 4) << QRegion(1, 2, 3, 4);
    QTest::addRow("complex") << (Region(1, 2, 3, 4) | Region(5, 6, 7, 8)) << (QRegion(1, 2, 3, 4) | QRegion(5, 6, 7, 8));
}

void TestRegion::fromAndToQRegion()
{
    QFETCH(Region, region);
    QFETCH(QRegion, qtRegion);

    QCOMPARE(Region(qtRegion), region);
    QCOMPARE(QRegion(region), qtRegion);
}

QTEST_MAIN(TestRegion)

#include "test_region.moc"
