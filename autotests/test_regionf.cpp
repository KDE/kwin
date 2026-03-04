/*
    SPDX-FileCopyrightText: 2025 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include <QTest>

#include "core/region.h"

using namespace KWin;

template<typename T>
static constexpr int bitCount()
{
    return sizeof(T) * 8;
}

static int xyToPow2(int x, int y, int stride)
{
    return std::pow(2, y * stride + x);
}

static QMap<int, RectF> enumerateRects(const QSize &gridSize, const QSizeF &unit = QSizeF(0.5, 0.5))
{
    Q_ASSERT(gridSize.width() * gridSize.height() <= bitCount<int>());

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

    QMap<int, RectF> rects;
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
                    rects.insert(variant, RectF(QPointF(x1 * unit.width(), y1 * unit.height()), QPointF(x2 * unit.width(), y2 * unit.height())));
                }
            }
        }
    }

    return rects;
}

static int coalesce(QList<RectF> &rects, int previousStart, int currentStart)
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
        const RectF &a = rects[previousStart + i];
        const RectF &b = rects[currentStart + i];
        if (a.left() != b.left() || a.right() != b.right()) {
            return currentStart;
        }
    }

    const qreal currentBottom = rects[currentStart].bottom();
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
static RegionF bitsToRegion(int bits, const QSize &gridSize, const QSizeF &unit = QSizeF(0.5, 0.5))
{
    QList<RectF> rects;
    rects.reserve(gridSize.width() * gridSize.height());

    int current = 0;
    for (int i = 0; i < gridSize.height(); ++i) {
        std::optional<RectF> currentRect;

        const int previous = current;
        current = rects.size();

        for (int j = 0; j < gridSize.width(); ++j) {
            const int bitIndex = i * gridSize.width() + j;
            const int bitValue = bits & (1 << bitIndex);

            if (bitValue) {
                if (currentRect) {
                    currentRect->setRight((j + 1) * unit.width());
                } else {
                    currentRect = RectF(QPointF(j * unit.width(), i * unit.height()), QPointF((j + 1) * unit.width(), (i + 1) * unit.height()));
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

    return RegionF::fromSortedRects(rects);
}

static QList<RegionF> enumerateRegions(const QSize &gridSize, const QSizeF &unit = QSizeF(0.5, 0.5))
{
    Q_ASSERT(gridSize.width() * gridSize.height() <= bitCount<int>());
    const int maxVariations = std::pow(2, gridSize.width() * gridSize.height());

    QList<RegionF> regions;
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

class TestRegionF : public QObject
{
    Q_OBJECT

public:
    TestRegionF() = default;

private Q_SLOTS:
    void equals_data();
    void equals();
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
    void scaled_data();
    void scaled();
    void fromSortedRects();
    void fromUnsortedRects();
    void fromRectsSortedByY();
    void rounded_data();
    void rounded();
    void roundedIn_data();
    void roundedIn();
    void roundedOut_data();
    void roundedOut();
    void grownBy_data();
    void grownBy();

private:
    const QSize gridSize = testGridSize();

    const QList<RegionF> m_regions = enumerateRegions(gridSize);
    const QMap<int, RectF> m_rects = enumerateRects(gridSize);
};

void TestRegionF::equals_data()
{
    QTest::addColumn<RegionF>("region1");
    QTest::addColumn<RegionF>("region2");
    QTest::addColumn<bool>("expected");

    QTest::addRow("default and default") << RegionF() << RegionF() << true;
    QTest::addRow("(0.1,0.2 0.3x0.4) and default") << RegionF(RectF(0.1, 0.2, 0.3, 0.4)) << RegionF() << false;
    QTest::addRow("(0.1,0.2 0.3x0.4) and (0.1,0.2 0.3x0.4)") << RegionF(RectF(0.1, 0.2, 0.3, 0.4)) << RegionF(RectF(0.1, 0.2, 0.3, 0.4)) << true;

    QTest::addRow("[(0.1,0.2 0.3x0.4), (0.5,0.6 0.7x0.8)] and (0.1,0.2 0.3x0.4)") << (RegionF(RectF(0.1, 0.2, 0.3, 0.4)) | RegionF(RectF(0.5, 0.6, 0.7, 0.8))) << RegionF(RectF(0.1, 0.2, 0.3, 0.4)) << false;
    QTest::addRow("[(0.1,0.2 0.3x0.4), (0.5,0.6 0.7x0.8)] and (0.5,0.6 0.7x0.8)") << (RegionF(RectF(0.1, 0.2, 0.3, 0.4)) | RegionF(RectF(0.5, 0.6, 0.7, 0.8))) << RegionF(RectF(0.5, 0.6, 0.7, 0.8)) << false;
    QTest::addRow("[(0.1,0.2 0.3x0.4), (0.5,0.6 0.7x0.8)] and [(0.1,0.2 0.3x0.4), (0.5,0.6 0.7x0.8)]") << (RegionF(RectF(0.1, 0.2, 0.3, 0.4)) | RegionF(RectF(0.5, 0.6, 0.7, 0.8))) << (RegionF(RectF(0.1, 0.2, 0.3, 0.4)) | RegionF(RectF(0.5, 0.6, 0.7, 0.8))) << true;
}

void TestRegionF::equals()
{
    QFETCH(RegionF, region1);
    QFETCH(RegionF, region2);
    QFETCH(bool, expected);

    QCOMPARE(region1 == region2, expected);
    QCOMPARE(region2 == region1, expected);

    QCOMPARE(region1 != region2, !expected);
    QCOMPARE(region2 != region1, !expected);
}

void TestRegionF::empty_data()
{
    QTest::addColumn<RegionF>("region");
    QTest::addColumn<bool>("empty");

    QTest::addRow("default") << RegionF() << true;
    QTest::addRow("0,0 0,0") << RegionF(RectF(QPointF(0, 0), QPointF(0, 0))) << true;
    QTest::addRow("0,0 0.1,0") << RegionF(RectF(QPointF(0, 0), QPointF(0.1, 0))) << true;
    QTest::addRow("0,0 0,0.1") << RegionF(RectF(QPointF(0, 0), QPointF(0, 0.1))) << true;
    QTest::addRow("0,0 0.1,0.1") << RegionF(RectF(QPointF(0, 0), QPointF(0.1, 0.1))) << false;
}

void TestRegionF::empty()
{
    QFETCH(RegionF, region);

    QTEST(region.isEmpty(), "empty");
}

void TestRegionF::boundingRect_data()
{
    QTest::addColumn<RegionF>("region");
    QTest::addColumn<RectF>("boundingRect");

    QTest::addRow("default") << RegionF() << RectF();
    QTest::addRow("(0.1,0.2 0.1,0.2)") << RegionF(RectF(QPointF(0.1, 0.2), QPointF(0.1, 0.2))) << RectF(QPointF(0.1, 0.2), QPointF(0.1, 0.2));
    QTest::addRow("(0.1,0.2 0.4,0.6)") << RegionF(RectF(QPointF(0.1, 0.2), QPointF(0.4, 0.6))) << RectF(QPointF(0.1, 0.2), QPointF(0.4, 0.6));
    QTest::addRow("[(0.1,0.2 0.4,0.6), (0.5,0.6 1.2,1.4)]") << (RegionF(RectF(QPointF(0.1, 0.2), QPointF(0.4, 0.6))) | RegionF(RectF(QPointF(0.5, 0.6), QPointF(1.2, 1.4)))) << RectF(QPointF(0.1, 0.2), QPointF(1.2, 1.4));
}

void TestRegionF::boundingRect()
{
    QFETCH(RegionF, region);
    QTEST(region.boundingRect(), "boundingRect");
}

void TestRegionF::containsRect()
{
    for (int i = 0; i < m_regions.size(); ++i) {
        for (const auto &[j, rect] : m_rects.asKeyValueRange()) {
            QCOMPARE(m_regions[i].contains(rect), j && (i & j) == j);
        }
    }
}

void TestRegionF::containsPoint_data()
{
    QTest::addColumn<RegionF>("region");
    QTest::addColumn<QPointF>("point");
    QTest::addColumn<bool>("contains");

    QTest::addRow("empty region contains 0,0") << RegionF() << QPointF(0, 0) << false;
    QTest::addRow("empty region contains 1,1") << RegionF() << QPointF(1, 1) << false;

    const RegionF simpleRegion = RegionF(0, 0, 5, 5);
    {
        QTest::addRow("above the simple region") << simpleRegion << QPointF(0, -1) << false;
        QTest::addRow("below the simple region") << simpleRegion << QPointF(0, 6) << false;
        QTest::addRow("to the left of the simple region") << simpleRegion << QPointF(-1, 0) << false;
        QTest::addRow("to the right of the simple region") << simpleRegion << QPointF(6, 0) << false;

        QTest::addRow("at the top edge of the simple region") << simpleRegion << QPointF(2, 0) << true;
        QTest::addRow("at the bottom edge of the simple region") << simpleRegion << QPointF(2, 5) << false;
        QTest::addRow("at the left edge of the simple region") << simpleRegion << QPointF(0, 2) << true;
        QTest::addRow("at the right edge of the simple region") << simpleRegion << QPointF(5, 2) << false;

        QTest::addRow("inside the simple region") << simpleRegion << QPointF(2, 2) << true;
    }

    const RegionF complexRegion = RegionF(0, 0, 10, 6) | RegionF(20, 0, 10, 6) | RegionF(0, 12, 10, 6) | RegionF(20, 12, 10, 6);
    {
        QTest::addRow("above the top-left rect in the complex region") << complexRegion << QPointF(5, -1) << false;
        QTest::addRow("below the top-left rect in the complex region") << complexRegion << QPointF(5, 7) << false;
        QTest::addRow("to the left of the top-left rect in the complex region") << complexRegion << QPointF(-1, 3) << false;
        QTest::addRow("to the right of the top-left rect in the complex region") << complexRegion << QPointF(11, 3) << false;

        QTest::addRow("at the top edge of the top-left rect in the complex region") << complexRegion << QPointF(5, 0) << true;
        QTest::addRow("at the bottom edge of the top-left rect in the complex region") << complexRegion << QPointF(5, 6) << false;
        QTest::addRow("at the left edge of the top-left rect in the complex region") << complexRegion << QPointF(0, 3) << true;
        QTest::addRow("at the right edge of the top-left rect in the complex region") << complexRegion << QPointF(10, 3) << false;

        QTest::addRow("inside the top-left rect in the complex region") << complexRegion << QPointF(5, 3) << true;
    }

    {
        QTest::addRow("above the top-right rect in the complex region") << complexRegion << QPointF(25, -1) << false;
        QTest::addRow("below the top-right rect in the complex region") << complexRegion << QPointF(25, 7) << false;
        QTest::addRow("to the left of the top-right rect in the complex region") << complexRegion << QPointF(19, 3) << false;
        QTest::addRow("to the right of the top-right rect in the complex region") << complexRegion << QPointF(31, 3) << false;

        QTest::addRow("at the top edge of the top-right rect in the complex region") << complexRegion << QPointF(25, 0) << true;
        QTest::addRow("at the bottom edge of the top-right rect in the complex region") << complexRegion << QPointF(25, 6) << false;
        QTest::addRow("at the left edge of the top-right rect in the complex region") << complexRegion << QPointF(20, 3) << true;
        QTest::addRow("at the right edge of the top-right rect in the complex region") << complexRegion << QPointF(30, 3) << false;

        QTest::addRow("inside the top-right rect in the complex region") << complexRegion << QPointF(25, 3) << true;
    }

    {
        QTest::addRow("above the bottom-left rect in the complex region") << complexRegion << QPointF(5, 11) << false;
        QTest::addRow("below the bottom-left rect in the complex region") << complexRegion << QPointF(5, 19) << false;
        QTest::addRow("to the left of the bottom-left rect in the complex region") << complexRegion << QPointF(-1, 15) << false;
        QTest::addRow("to the right of the bottom-left rect in the complex region") << complexRegion << QPointF(11, 15) << false;

        QTest::addRow("at the top edge of the bottom-left rect in the complex region") << complexRegion << QPointF(5, 12) << true;
        QTest::addRow("at the bottom edge of the bottom-left rect in the complex region") << complexRegion << QPointF(5, 18) << false;
        QTest::addRow("at the left edge of the bottom-left rect in the complex region") << complexRegion << QPointF(0, 15) << true;
        QTest::addRow("at the right edge of the bottom-left rect in the complex region") << complexRegion << QPointF(10, 15) << false;

        QTest::addRow("inside the bottom-left rect in the complex region") << complexRegion << QPointF(5, 15) << true;
    }

    {
        QTest::addRow("above the bottom-right rect in the complex region") << complexRegion << QPointF(25, 11) << false;
        QTest::addRow("below the bottom-right rect in the complex region") << complexRegion << QPointF(25, 19) << false;
        QTest::addRow("to the left of the bottom-right rect in the complex region") << complexRegion << QPointF(19, 15) << false;
        QTest::addRow("to the right of the bottom-right rect in the complex region") << complexRegion << QPointF(31, 15) << false;

        QTest::addRow("at the top edge of the bottom-right rect in the complex region") << complexRegion << QPointF(25, 12) << true;
        QTest::addRow("at the bottom edge of the bottom-right rect in the complex region") << complexRegion << QPointF(25, 18) << false;
        QTest::addRow("at the left edge of the bottom-right rect in the complex region") << complexRegion << QPointF(20, 15) << true;
        QTest::addRow("at the right edge of the bottom-right rect in the complex region") << complexRegion << QPointF(30, 15) << false;

        QTest::addRow("inside the bottom-right rect in the complex region") << complexRegion << QPointF(25, 15) << true;
    }

    QTest::addRow("above the gap between top-left and top-right rects in the complex region") << complexRegion << QPointF(15, -1) << false;
    QTest::addRow("below the gap between bottom-left and bottom-right rects in the complex region") << complexRegion << QPointF(15, 19) << false;
    QTest::addRow("to the left of the gap between top-left and bottom-left rects in the complex region") << complexRegion << QPointF(-1, 9) << false;
    QTest::addRow("to the right of the gap between top-right and bottom-right rects in the complex region") << complexRegion << QPointF(31, 9) << false;
    QTest::addRow("inside gap between four rects in the complex region") << complexRegion << QPointF(15, 9) << false;
}

void TestRegionF::containsPoint()
{
    QFETCH(RegionF, region);
    QFETCH(QPointF, point);
    QTEST(region.contains(point), "contains");
}

void TestRegionF::intersectsRect()
{
    for (int i = 0; i < m_regions.size(); ++i) {
        for (const auto &[j, rect] : m_rects.asKeyValueRange()) {
            QCOMPARE(m_regions[i].intersects(rect), bool(i & j));
        }
    }
}

void TestRegionF::intersectsRegion()
{
    for (int i = 0; i < m_regions.size(); ++i) {
        for (int j = 0; j <= i; ++j) {
            QCOMPARE(m_regions[i].intersects(m_regions[j]), bool(i & j));
            QCOMPARE(m_regions[j].intersects(m_regions[i]), bool(i & j));
        }
    }
}

void TestRegionF::united()
{
    for (int i = 0; i < m_regions.size(); ++i) {
        for (int j = 0; j <= i; ++j) {
            const RegionF expected = m_regions[i | j];
            QCOMPARE(m_regions[i].united(m_regions[j]), expected);
            QCOMPARE(m_regions[j].united(m_regions[i]), expected);
        }
    }
}

void TestRegionF::unitedRect()
{
    for (int i = 0; i < m_regions.size(); ++i) {
        for (const auto &[j, rect] : m_rects.asKeyValueRange()) {
            const RegionF expected = m_regions[i | j];
            QCOMPARE(m_regions[i].united(rect), expected);
        }
    }
}

void TestRegionF::subtracted()
{
    for (int i = 0; i < m_regions.size(); ++i) {
        for (int j = 0; j <= i; ++j) {
            QCOMPARE(m_regions[i].subtracted(m_regions[j]), m_regions[i & ~j]);
            QCOMPARE(m_regions[j].subtracted(m_regions[i]), m_regions[j & ~i]);
        }
    }
}

void TestRegionF::subtractedRect()
{
    for (int i = 0; i < m_regions.size(); ++i) {
        for (const auto &[j, rect] : m_rects.asKeyValueRange()) {
            const RegionF expected = m_regions[i & ~j];
            QCOMPARE(m_regions[i].subtracted(rect), expected);
        }
    }
}

void TestRegionF::xored()
{
    for (int i = 0; i < m_regions.size(); ++i) {
        for (int j = 0; j <= i; ++j) {
            const RegionF expected = m_regions[i ^ j];
            QCOMPARE(m_regions[i].xored(m_regions[j]), expected);
            QCOMPARE(m_regions[j].xored(m_regions[i]), expected);
        }
    }
}

void TestRegionF::xoredRect()
{
    for (int i = 0; i < m_regions.size(); ++i) {
        for (const auto &[j, rect] : m_rects.asKeyValueRange()) {
            const RegionF expected = m_regions[i ^ j];
            QCOMPARE(m_regions[i].xored(rect), expected);
        }
    }
}

void TestRegionF::intersected()
{
    for (int i = 0; i < m_regions.size(); ++i) {
        for (int j = 0; j <= i; ++j) {
            const RegionF expected = m_regions[i & j];
            QCOMPARE(m_regions[i].intersected(m_regions[j]), expected);
            QCOMPARE(m_regions[j].intersected(m_regions[i]), expected);
        }
    }
}

void TestRegionF::intersectedRect()
{
    for (int i = 0; i < m_regions.size(); ++i) {
        for (const auto &[j, rect] : m_rects.asKeyValueRange()) {
            const RegionF expected = m_regions[i & j];
            QCOMPARE(m_regions[i].intersected(rect), expected);
        }
    }
}

void TestRegionF::translated_data()
{
    QTest::addColumn<RegionF>("region");
    QTest::addColumn<QPointF>("translation");
    QTest::addColumn<RegionF>("expected");

    QTest::addRow("empty") << RegionF() << QPointF(1, 2) << RegionF();

    QTest::addRow("simple")
        << RegionF(RectF(QPointF(0.1, 0.2), QPointF(0.4, 0.6)))
        << QPointF(10, 11)
        << RegionF(RectF(QPointF(0.1, 0.2), QPointF(0.4, 0.6)).translated(10, 11));

    QTest::addRow("complex")
        << (RegionF(RectF(QPointF(0.1, 0.2), QPointF(0.4, 0.6))) | RegionF(RectF(QPointF(0.5, 0.6), QPointF(1.2, 1.4))))
        << QPointF(10, 11)
        << (RegionF(RectF(QPointF(0.1, 0.2), QPointF(0.4, 0.6)).translated(10, 11)) | RegionF(RectF(QPointF(0.5, 0.6), QPointF(1.2, 1.4)).translated(10, 11)));
}

void TestRegionF::translated()
{
    QFETCH(RegionF, region);
    QFETCH(QPointF, translation);

    {
        RegionF translated = region;
        translated.translate(translation.x(), translation.y());
        QTEST(translated, "expected");
    }

    {
        RegionF translated = region;
        translated.translate(translation);
        QTEST(translated, "expected");
    }

    {
        const RegionF translated = region.translated(translation.x(), translation.y());
        QTEST(translated, "expected");
    }

    {
        const RegionF translated = region.translated(translation);
        QTEST(translated, "expected");
    }
}

void TestRegionF::scaled_data()
{
    QTest::addColumn<RegionF>("region");
    QTest::addColumn<qreal>("scale");
    QTest::addColumn<RegionF>("expected");

    QTest::addRow("empty") << RegionF() << 42.73 << RegionF();

    QTest::addRow("simple")
        << RegionF(RectF(QPointF(0.1, 0.2), QPointF(0.4, 0.6)))
        << 42.73
        << RegionF(RectF(QPointF(0.1, 0.2), QPointF(0.4, 0.6)).scaled(42.73));

    QTest::addRow("complex")
        << (RegionF(RectF(QPointF(0.1, 0.2), QPointF(0.4, 0.6))) | RegionF(RectF(QPointF(0.5, 0.6), QPointF(1.2, 1.4))))
        << 42.73
        << (RegionF(RectF(QPointF(0.1, 0.2), QPointF(0.4, 0.6)).scaled(42.73)) | RegionF(RectF(QPointF(0.5, 0.6), QPointF(1.2, 1.4)).scaled(42.73)));
}

void TestRegionF::scaled()
{
    QFETCH(RegionF, region);
    QFETCH(qreal, scale);

    {
        RegionF scaled = region;
        scaled.scale(scale);
        QTEST(scaled, "expected");
    }

    {
        const RegionF scaled = region.scaled(scale);
        QTEST(scaled, "expected");
    }
}

template<typename T>
static QList<T> spanToList(const QSpan<const T> &span)
{
    QList<T> list;
    list.assign(span.begin(), span.end());
    return list;
}

void TestRegionF::fromSortedRects()
{
    for (int i = 0; i < m_regions.size(); ++i) {
        QCOMPARE(RegionF::fromSortedRects(spanToList(m_regions[i].rects())), m_regions[i]);
    }
}

void TestRegionF::fromUnsortedRects()
{
    for (int i = 0; i < m_regions.size(); ++i) {
        for (int j = 0; j < m_regions.size(); ++j) {
            const QList<RectF> rects = spanToList(m_regions[i].rects()) + spanToList(m_regions[j].rects());
            QCOMPARE(RegionF::fromUnsortedRects(rects), m_regions[i | j]);
        }
    }
}

void TestRegionF::fromRectsSortedByY()
{
    for (int i = 0; i < m_regions.size(); ++i) {
        for (int j = 0; j < m_regions.size(); ++j) {
            QList<RectF> rects = spanToList(m_regions[i].rects()) + spanToList(m_regions[j].rects());
            std::sort(rects.begin(), rects.end(), [](const RectF &a, const RectF &b) {
                return a.top() < b.top();
            });

            QCOMPARE(RegionF::fromRectsSortedByY(rects), m_regions[i | j]);
        }
    }
}

void TestRegionF::rounded_data()
{
    QTest::addColumn<RegionF>("region");
    QTest::addColumn<Region>("expected");

    QTest::addRow("empty") << RegionF() << Region();

    QTest::addRow("very small") << RegionF(RectF(QPointF(0.1, 0.1), QPointF(0.2, 0.2))) << Region();

    QTest::addRow("1,2 3,4") << RegionF(RectF(QPointF(1, 2), QPointF(3, 4))) << Region(Rect(QPoint(1, 2), QPoint(3, 4)));
    QTest::addRow("1.1,2.1 3.1,4.1") << RegionF(RectF(QPointF(1.1, 2.1), QPointF(3.1, 4.1))) << Region(Rect(QPoint(1, 2), QPoint(3, 4)));
    QTest::addRow("1.5,2.5 3.5,4.5") << RegionF(RectF(QPointF(1.5, 2.5), QPointF(3.5, 4.5))) << Region(Rect(QPoint(2, 3), QPoint(4, 5)));
    QTest::addRow("1.9,2.9 3.9,4.9") << RegionF(RectF(QPointF(1.9, 2.9), QPointF(3.9, 4.9))) << Region(Rect(QPoint(2, 3), QPoint(4, 5)));

    QTest::addRow("-3,-4 -1,-2") << RegionF(RectF(QPointF(-3, -4), QPointF(-1, -2))) << Region(Rect(QPoint(-3, -4), QPoint(-1, -2)));
    QTest::addRow("-3.1,-4.1 -1.1,-2.1") << RegionF(RectF(QPointF(-3.1, -4.1), QPointF(-1.1, -2.1))) << Region(Rect(QPoint(-3, -4), QPoint(-1, -2)));
    QTest::addRow("-3.5,-4.5 -1.5,-2.5") << RegionF(RectF(QPointF(-3.5, -4.5), QPointF(-1.5, -2.5))) << Region(Rect(QPoint(-4, -5), QPoint(-2, -3)));
    QTest::addRow("-3.9,-4.9 -1.9,-2.9") << RegionF(RectF(QPointF(-3.9, -4.9), QPointF(-1.9, -2.9))) << Region(Rect(QPoint(-4, -5), QPoint(-2, -3)));
}

void TestRegionF::rounded()
{
    QFETCH(RegionF, region);
    QTEST(region.rounded(), "expected");
}

void TestRegionF::roundedIn_data()
{
    QTest::addColumn<RegionF>("region");
    QTest::addColumn<Region>("expected");

    QTest::addRow("empty") << RegionF() << Region();

    QTest::addRow("very small") << RegionF(RectF(QPointF(0.1, 0.1), QPointF(0.2, 0.2))) << Region(Rect(QPoint(1, 1), QPoint(0, 0)));

    QTest::addRow("1,2 3,4") << RegionF(RectF(QPointF(1, 2), QPointF(3, 4))) << Region(Rect(QPoint(1, 2), QPoint(3, 4)));
    QTest::addRow("1.1,2.1 3.1,4.1") << RegionF(RectF(QPointF(1.1, 2.1), QPointF(3.1, 4.1))) << Region(Rect(QPoint(2, 3), QPoint(3, 4)));
    QTest::addRow("1.5,2.5 3.5,4.5") << RegionF(RectF(QPointF(1.5, 2.5), QPointF(3.5, 4.5))) << Region(Rect(QPoint(2, 3), QPoint(3, 4)));
    QTest::addRow("1.9,2.9 3.9,4.9") << RegionF(RectF(QPointF(1.9, 2.9), QPointF(3.9, 4.9))) << Region(Rect(QPoint(2, 3), QPoint(3, 4)));

    QTest::addRow("-3,-4 -1,-2") << RegionF(RectF(QPointF(-3, -4), QPointF(-1, -2))) << Region(Rect(QPoint(-3, -4), QPoint(-1, -2)));
    QTest::addRow("-3.1,-4.1 -1.1,-2.1") << RegionF(RectF(QPointF(-3.1, -4.1), QPointF(-1.1, -2.1))) << Region(Rect(QPoint(-3, -4), QPoint(-2, -3)));
    QTest::addRow("-3.5,-4.5 -1.5,-2.5") << RegionF(RectF(QPointF(-3.5, -4.5), QPointF(-1.5, -2.5))) << Region(Rect(QPoint(-3, -4), QPoint(-2, -3)));
    QTest::addRow("-3.9,-4.9 -1.9,-2.9") << RegionF(RectF(QPointF(-3.9, -4.9), QPointF(-1.9, -2.9))) << Region(Rect(QPoint(-3, -4), QPoint(-2, -3)));
}

void TestRegionF::roundedIn()
{
    QFETCH(RegionF, region);
    QTEST(region.roundedIn(), "expected");
}

void TestRegionF::roundedOut_data()
{
    QTest::addColumn<RegionF>("region");
    QTest::addColumn<Region>("expected");

    QTest::addRow("empty") << RegionF() << Region();

    QTest::addRow("very small") << RegionF(RectF(QPointF(0.1, 0.1), QPointF(0.2, 0.2))) << Region(Rect(QPoint(0, 0), QPoint(1, 1)));

    QTest::addRow("1,2 3,4") << RegionF(RectF(QPointF(1, 2), QPointF(3, 4))) << Region(Rect(QPoint(1, 2), QPoint(3, 4)));
    QTest::addRow("1.1,2.1 3.1,4.1") << RegionF(RectF(QPointF(1.1, 2.1), QPointF(3.1, 4.1))) << Region(Rect(QPoint(1, 2), QPoint(4, 5)));
    QTest::addRow("1.5,2.5 3.5,4.5") << RegionF(RectF(QPointF(1.5, 2.5), QPointF(3.5, 4.5))) << Region(Rect(QPoint(1, 2), QPoint(4, 5)));
    QTest::addRow("1.9,2.9 3.9,4.9") << RegionF(RectF(QPointF(1.9, 2.9), QPointF(3.9, 4.9))) << Region(Rect(QPoint(1, 2), QPoint(4, 5)));

    QTest::addRow("-3,-4 -1,-2") << RegionF(RectF(QPointF(-3, -4), QPointF(-1, -2))) << Region(Rect(QPoint(-3, -4), QPoint(-1, -2)));
    QTest::addRow("-3.1,-4.1 -1.1,-2.1") << RegionF(RectF(QPointF(-3.1, -4.1), QPointF(-1.1, -2.1))) << Region(Rect(QPoint(-4, -5), QPoint(-1, -2)));
    QTest::addRow("-3.5,-4.5 -1.5,-2.5") << RegionF(RectF(QPointF(-3.5, -4.5), QPointF(-1.5, -2.5))) << Region(Rect(QPoint(-4, -5), QPoint(-1, -2)));
    QTest::addRow("-3.9,-4.9 -1.9,-2.9") << RegionF(RectF(QPointF(-3.9, -4.9), QPointF(-1.9, -2.9))) << Region(Rect(QPoint(-4, -5), QPoint(-1, -2)));
}

void TestRegionF::roundedOut()
{
    QFETCH(RegionF, region);
    QTEST(region.roundedOut(), "expected");
}

void TestRegionF::grownBy_data()
{
    QTest::addColumn<RegionF>("region");
    QTest::addColumn<QMarginsF>("margins");
    QTest::addColumn<RegionF>("expected");

    QTest::addRow("empty region, empty margins") << RegionF() << QMarginsF() << RegionF();
    QTest::addRow("empty region, not empty margins") << RegionF() << QMarginsF(1, 2, 3, 4) << RegionF(RectF(QPointF(-1, -2), QPointF(3, 4)));

    QTest::addRow("simple region") << RegionF(RectF(QPointF(0, 0), QPointF(5, 5))) << QMarginsF(1, 2, 3, 4) << RegionF(RectF(QPointF(-1, -2), QPointF(8, 9)));

    QTest::addRow("complex region - conjoined")
        << (RegionF(RectF(QPointF(0, 0), QPointF(6, 6))) | RegionF(RectF(QPointF(6, 3), QPointF(12, 9))))
        << QMarginsF(1, 2, 3, 4)
        << (RegionF(RectF(QPointF(-1, -2), QPointF(9, 10))) | RegionF(RectF(QPointF(5, 1), QPointF(15, 13))));

    QTest::addRow("complex region - disjoint")
        << (RegionF(RectF(QPointF(0, 0), QPointF(6, 6))) | RegionF(RectF(QPointF(10, 10), QPointF(16, 16))))
        << QMarginsF(1, 2, 3, 4)
        << (RegionF(RectF(QPointF(-1, -2), QPointF(9, 10))) | RegionF(RectF(QPointF(9, 8), QPointF(19, 20))));
}

void TestRegionF::grownBy()
{
    QFETCH(RegionF, region);
    QFETCH(QMarginsF, margins);
    QTEST(region.grownBy(margins), "expected");
}

QTEST_MAIN(TestRegionF)

#include "test_regionf.moc"
