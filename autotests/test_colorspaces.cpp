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
    void nonNormalizedPrimaries();
    void testBlackPointCompensation_data();
    void testBlackPointCompensation();
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

static const double s_resolution10bit = std::pow(1.0 / 2.0, 10);

void TestColorspaces::roundtripConversion_data()
{
    QTest::addColumn<NamedColorimetry>("srcColorimetry");
    QTest::addColumn<NamedTransferFunction>("srcTransferFunction");
    QTest::addColumn<NamedColorimetry>("dstColorimetry");
    QTest::addColumn<NamedTransferFunction>("dstTransferFunction");
    QTest::addColumn<double>("requiredAccuracy");

    QTest::addRow("BT709 (sRGB) <-> BT2020 (linear)") << NamedColorimetry::BT709 << NamedTransferFunction::sRGB << NamedColorimetry::BT2020 << NamedTransferFunction::linear << s_resolution10bit;
    QTest::addRow("BT709 (gamma 2.2) <-> BT2020 (linear)") << NamedColorimetry::BT709 << NamedTransferFunction::gamma22 << NamedColorimetry::BT2020 << NamedTransferFunction::linear << s_resolution10bit;
    QTest::addRow("BT709 (scRGB) <-> BT2020 (linear)") << NamedColorimetry::BT709 << NamedTransferFunction::scRGB << NamedColorimetry::BT2020 << NamedTransferFunction::linear << s_resolution10bit;
    QTest::addRow("BT709 (linear) <-> BT2020 (linear)") << NamedColorimetry::BT709 << NamedTransferFunction::linear << NamedColorimetry::BT2020 << NamedTransferFunction::linear << s_resolution10bit;
    QTest::addRow("BT709 (PQ) <-> BT2020 (linear)") << NamedColorimetry::BT709 << NamedTransferFunction::PerceptualQuantizer << NamedColorimetry::BT2020 << NamedTransferFunction::linear << 3 * s_resolution10bit;
}

void TestColorspaces::roundtripConversion()
{
    QFETCH(NamedColorimetry, srcColorimetry);
    QFETCH(NamedTransferFunction, srcTransferFunction);
    QFETCH(NamedColorimetry, dstColorimetry);
    QFETCH(NamedTransferFunction, dstTransferFunction);
    QFETCH(double, requiredAccuracy);

    const auto src = ColorDescription(srcColorimetry, srcTransferFunction, 100, 0, 100, 100);
    const auto dst = ColorDescription(dstColorimetry, dstTransferFunction, 100, 0, 100, 100);

    const QVector3D red(1, 0, 0);
    const QVector3D green(0, 1, 0);
    const QVector3D blue(0, 0, 1);
    const QVector3D white(1, 1, 1);

    QVERIFY(compareVectors(dst.mapTo(src.mapTo(red, dst), src), red, requiredAccuracy));
    QVERIFY(compareVectors(dst.mapTo(src.mapTo(green, dst), src), green, requiredAccuracy));
    QVERIFY(compareVectors(dst.mapTo(src.mapTo(blue, dst), src), blue, requiredAccuracy));
    QVERIFY(compareVectors(dst.mapTo(src.mapTo(white, dst), src), white, requiredAccuracy));
}

void TestColorspaces::nonNormalizedPrimaries()
{
    // this test ensures that non-normalized primaries don't mess up the transformations between color spaces
    const auto from = Colorimetry::fromName(NamedColorimetry::BT709);
    const auto to = Colorimetry(Colorimetry::xyToXYZ(from.red()) * 2, Colorimetry::xyToXYZ(from.green()) * 2, Colorimetry::xyToXYZ(from.blue()) * 2, Colorimetry::xyToXYZ(from.white()) * 2, QVector3D(0, 0, 0));

    const auto convertedWhite = from.toOther(to) * QVector3D(1, 1, 1);
    QCOMPARE_LE(std::abs(1 - convertedWhite.x()), s_resolution10bit);
    QCOMPARE_LE(std::abs(1 - convertedWhite.y()), s_resolution10bit);
    QCOMPARE_LE(std::abs(1 - convertedWhite.z()), s_resolution10bit);
}

void TestColorspaces::testBlackPointCompensation_data()
{
    QTest::addColumn<QVector3D>("input");
    QTest::addColumn<QVector3D>("output");

    for (double input = 0; input < 1.05; input += 0.1) {
        const double output = 0.1 + input * 0.9;
        QTest::addRow("%f -> %f", input, output) << QVector3D(input, input, input) << QVector3D(output, output, output);
    }
}

void TestColorspaces::testBlackPointCompensation()
{
    const auto from = Colorimetry::fromName(NamedColorimetry::BT709);
    const auto to = Colorimetry(from.red(), from.green(), from.blue(), from.white(), QVector3D(0.1, 0.1, 0.1));

    QFETCH(QVector3D, input);
    QFETCH(QVector3D, output);

    const auto converted = from.toOther(to) * input;
    QCOMPARE_LE(std::abs(converted.x() - output.x()), 2 * s_resolution10bit);
    QCOMPARE_LE(std::abs(converted.y() - output.y()), 2 * s_resolution10bit);
    QCOMPARE_LE(std::abs(converted.z() - output.z()), 2 * s_resolution10bit);
}

QTEST_MAIN(TestColorspaces)

#include "test_colorspaces.moc"
