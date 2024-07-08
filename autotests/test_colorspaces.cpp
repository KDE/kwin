/*
    SPDX-FileCopyrightText: 2023 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include <QTest>

#include "core/colorpipeline.h"
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
    void testIdentityTransformation_data();
    void testIdentityTransformation();
    void testColorPipeline();
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
    QTest::addColumn<TransferFunction::Type>("srcTransferFunction");
    QTest::addColumn<NamedColorimetry>("dstColorimetry");
    QTest::addColumn<TransferFunction::Type>("dstTransferFunction");
    QTest::addColumn<double>("requiredAccuracy");

    QTest::addRow("BT709 (sRGB) <-> BT2020 (linear)") << NamedColorimetry::BT709 << TransferFunction::sRGB << NamedColorimetry::BT2020 << TransferFunction::linear << s_resolution10bit;
    QTest::addRow("BT709 (gamma 2.2) <-> BT2020 (linear)") << NamedColorimetry::BT709 << TransferFunction::gamma22 << NamedColorimetry::BT2020 << TransferFunction::linear << s_resolution10bit;
    QTest::addRow("BT709 (scRGB) <-> BT2020 (linear)") << NamedColorimetry::BT709 << TransferFunction::scRGB << NamedColorimetry::BT2020 << TransferFunction::linear << s_resolution10bit;
    QTest::addRow("BT709 (linear) <-> BT2020 (linear)") << NamedColorimetry::BT709 << TransferFunction::linear << NamedColorimetry::BT2020 << TransferFunction::linear << s_resolution10bit;
    QTest::addRow("BT709 (PQ) <-> BT2020 (linear)") << NamedColorimetry::BT709 << TransferFunction::PerceptualQuantizer << NamedColorimetry::BT2020 << TransferFunction::linear << 3 * s_resolution10bit;
}

void TestColorspaces::roundtripConversion()
{
    QFETCH(NamedColorimetry, srcColorimetry);
    QFETCH(TransferFunction::Type, srcTransferFunction);
    QFETCH(NamedColorimetry, dstColorimetry);
    QFETCH(TransferFunction::Type, dstTransferFunction);
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
    const auto to = Colorimetry(Colorimetry::xyToXYZ(from.red()) * 2, Colorimetry::xyToXYZ(from.green()) * 2, Colorimetry::xyToXYZ(from.blue()) * 2, Colorimetry::xyToXYZ(from.white()) * 2);

    const auto convertedWhite = from.toOther(to) * QVector3D(1, 1, 1);
    QCOMPARE_LE(std::abs(1 - convertedWhite.x()), s_resolution10bit);
    QCOMPARE_LE(std::abs(1 - convertedWhite.y()), s_resolution10bit);
    QCOMPARE_LE(std::abs(1 - convertedWhite.z()), s_resolution10bit);
}

void TestColorspaces::testIdentityTransformation_data()
{
    QTest::addColumn<NamedColorimetry>("colorimetry");
    QTest::addColumn<TransferFunction::Type>("transferFunction");

    QTest::addRow("BT709 (sRGB)") << NamedColorimetry::BT709 << TransferFunction::sRGB;
    QTest::addRow("BT709 (gamma22)") << NamedColorimetry::BT709 << TransferFunction::gamma22;
    QTest::addRow("BT709 (PQ)") << NamedColorimetry::BT709 << TransferFunction::PerceptualQuantizer;
    QTest::addRow("BT709 (linear)") << NamedColorimetry::BT709 << TransferFunction::linear;
    QTest::addRow("BT2020 (sRGB)") << NamedColorimetry::BT2020 << TransferFunction::sRGB;
    QTest::addRow("BT2020 (gamma22)") << NamedColorimetry::BT2020 << TransferFunction::gamma22;
    QTest::addRow("BT2020 (PQ)") << NamedColorimetry::BT2020 << TransferFunction::PerceptualQuantizer;
    QTest::addRow("BT2020 (linear)") << NamedColorimetry::BT2020 << TransferFunction::linear;
}

void TestColorspaces::testIdentityTransformation()
{
    QFETCH(NamedColorimetry, colorimetry);
    QFETCH(TransferFunction::Type, transferFunction);
    const ColorDescription color(colorimetry, transferFunction, 100, 0, 100, 100);

    const auto pipeline = ColorPipeline::create(color, color);
    if (!pipeline.isIdentity()) {
        qWarning() << pipeline;
    }
    QVERIFY(pipeline.isIdentity());
}

void TestColorspaces::testColorPipeline()
{
    const ColorDescription from(NamedColorimetry::BT709, TransferFunction::gamma22, 100, 0, 100, 100);
    const ColorDescription to(NamedColorimetry::BT2020, TransferFunction::PerceptualQuantizer, 500, 0, 500, 500);

    const auto pipeline = ColorPipeline::create(from, to);
    QVERIFY(compareVectors(pipeline.evaluate(QVector3D(0, 0, 0)), QVector3D(0.044, 0.044, 0.044), s_resolution10bit));
    QVERIFY(compareVectors(pipeline.evaluate(QVector3D(0.5, 0.5, 0.5)), QVector3D(0.517, 0.517, 0.517), s_resolution10bit));
    QVERIFY(compareVectors(pipeline.evaluate(QVector3D(1, 1, 1)), QVector3D(0.677, 0.677, 0.677), s_resolution10bit));

    const auto inversePipeline = ColorPipeline::create(to, from);
    QVERIFY(compareVectors(inversePipeline.evaluate(QVector3D(0.044, 0.044, 0.044)), QVector3D(0, 0, 0), s_resolution10bit));
    QVERIFY(compareVectors(inversePipeline.evaluate(QVector3D(0.517, 0.517, 0.517)), QVector3D(0.5, 0.5, 0.5), s_resolution10bit));
    QVERIFY(compareVectors(inversePipeline.evaluate(QVector3D(0.677, 0.677, 0.677)), QVector3D(1, 1, 1), s_resolution10bit));
}

QTEST_MAIN(TestColorspaces)

#include "test_colorspaces.moc"
