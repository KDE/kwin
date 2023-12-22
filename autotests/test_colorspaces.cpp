/*
    SPDX-FileCopyrightText: 2023 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include <QTest>

#include "core/colorspace.h"

using namespace KWin;

class TestColorspaces : public QObject
{
    Q_OBJECT

public:
    TestColorspaces() = default;

private Q_SLOTS:
    void roundtripConversion_data();
    void roundtripConversion();
};

static bool compareVectors(const QVector3D &one, const QVector3D &two, float maxDifference)
{
    const bool ret = std::abs(one.x() - two.x()) <= maxDifference
        && std::abs(one.y() - two.y()) <= maxDifference
        && std::abs(one.z() - two.z()) <= maxDifference;
    if (!ret) {
        qWarning() << one << "!=" << two << "within" << maxDifference;
    }
    return ret;
}

void TestColorspaces::roundtripConversion_data()
{
    QTest::addColumn<NamedColorimetry>("srcColorimetry");
    QTest::addColumn<NamedTransferFunction>("srcTransferFunction");
    QTest::addColumn<NamedColorimetry>("dstColorimetry");
    QTest::addColumn<NamedTransferFunction>("dstTransferFunction");
    QTest::addColumn<double>("requiredAccuracy");

    const double resolution10bit = std::pow(1.0 / 2.0, 10);
    QTest::addRow("BT709 (sRGB) <-> BT2020 (linear)") << NamedColorimetry::BT709 << NamedTransferFunction::sRGB << NamedColorimetry::BT2020 << NamedTransferFunction::linear << resolution10bit;
    QTest::addRow("BT709 (gamma 2.2) <-> BT2020 (linear)") << NamedColorimetry::BT709 << NamedTransferFunction::gamma22 << NamedColorimetry::BT2020 << NamedTransferFunction::linear << resolution10bit;
    QTest::addRow("BT709 (scRGB) <-> BT2020 (linear)") << NamedColorimetry::BT709 << NamedTransferFunction::scRGB << NamedColorimetry::BT2020 << NamedTransferFunction::linear << resolution10bit;
    QTest::addRow("BT709 (linear) <-> BT2020 (linear)") << NamedColorimetry::BT709 << NamedTransferFunction::linear << NamedColorimetry::BT2020 << NamedTransferFunction::linear << resolution10bit;
    QTest::addRow("BT709 (PQ) <-> BT2020 (linear)") << NamedColorimetry::BT709 << NamedTransferFunction::PerceptualQuantizer << NamedColorimetry::BT2020 << NamedTransferFunction::linear << 3 * resolution10bit;
}

void TestColorspaces::roundtripConversion()
{
    QFETCH(NamedColorimetry, srcColorimetry);
    QFETCH(NamedTransferFunction, srcTransferFunction);
    QFETCH(NamedColorimetry, dstColorimetry);
    QFETCH(NamedTransferFunction, dstTransferFunction);
    QFETCH(double, requiredAccuracy);

    const auto src = ColorDescription(srcColorimetry, srcTransferFunction, 100, 0, 100, 100, 0);
    const auto dst = ColorDescription(dstColorimetry, dstTransferFunction, 100, 0, 100, 100, 0);

    const QVector3D red(1, 0, 0);
    const QVector3D green(0, 1, 0);
    const QVector3D blue(0, 0, 1);
    const QVector3D white(1, 1, 1);

    QVERIFY(compareVectors(dst.mapTo(src.mapTo(red, dst), src), red, requiredAccuracy));
    QVERIFY(compareVectors(dst.mapTo(src.mapTo(green, dst), src), green, requiredAccuracy));
    QVERIFY(compareVectors(dst.mapTo(src.mapTo(blue, dst), src), blue, requiredAccuracy));
    QVERIFY(compareVectors(dst.mapTo(src.mapTo(white, dst), src), white, requiredAccuracy));
}

QTEST_MAIN(TestColorspaces)

#include "test_colorspaces.moc"
