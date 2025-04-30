/*
    SPDX-FileCopyrightText: 2025 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "suncalc.h"

#include <QTest>

using namespace KWin;

struct Sample
{
    QDate date;
    int firstHour;
    int lastHour;
    QTimeZone timeZone;
    qreal latitude;
    qreal longitude;
    QDateTime civilDawn;
    QDateTime civilDusk;
    qreal tolerance = 5 * 60;
};

class SolarTransitTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void pole();
    void transitTime_data();
    void transitTime();
};

void SolarTransitTest::pole()
{
    for (qreal latitude : {-90, 90}) {
        const auto morning = calculateSunTimings(QDateTime::currentDateTime(), latitude, 0, true);
        QVERIFY(morning.first.isNull());
        QVERIFY(morning.second.isNull());

        const auto evening = calculateSunTimings(QDateTime::currentDateTime(), latitude, 0, false);
        QVERIFY(evening.first.isNull());
        QVERIFY(evening.second.isNull());
    }
}

static QTimeZone makeTimeZone(int hours, int minutes = 0)
{
    return QTimeZone::fromSecondsAheadOfUtc(hours * 60 * 60 + minutes * 60);
}

void SolarTransitTest::transitTime_data()
{
    QTest::addColumn<Sample>("sample");

    QTest::addRow("Adelaide on 4/30/25") << Sample{
        .date = QDate(2025, 4, 30),
        .firstHour = 1,
        .lastHour = 24,
        .timeZone = makeTimeZone(9, 30),
        .latitude = -34.9275,
        .longitude = 138.6,
        .civilDawn = QDateTime(QDate(2025, 4, 30), QTime(6, 25), makeTimeZone(9, 30)),
        .civilDusk = QDateTime(QDate(2025, 4, 30), QTime(18, 0), makeTimeZone(9, 30)),
    };

    QTest::addRow("Wellington on 4/30/25") << Sample{
        .date = QDate(2025, 4, 30),
        .firstHour = 1,
        .lastHour = 24,
        .timeZone = makeTimeZone(12, 0),
        .latitude = -41.3,
        .longitude = 174.783333,
        .civilDawn = QDateTime(QDate(2025, 4, 30), QTime(6, 40), makeTimeZone(12, 0)),
        .civilDusk = QDateTime(QDate(2025, 4, 30), QTime(18, 0), makeTimeZone(12, 0)),
    };

    QTest::addRow("Kyiv on 4/30/25") << Sample{
        .date = QDate(2025, 4, 30),
        .firstHour = 1,
        .lastHour = 24,
        .timeZone = makeTimeZone(3, 0),
        .latitude = 50.45,
        .longitude = 30.523333,
        .civilDawn = QDateTime(QDate(2025, 4, 30), QTime(5, 0), makeTimeZone(3, 0)),
        .civilDusk = QDateTime(QDate(2025, 4, 30), QTime(20, 55), makeTimeZone(3, 0)),
    };

    QTest::addRow("London on 4/30/25") << Sample{
        .date = QDate(2025, 4, 30),
        .firstHour = 2,
        .lastHour = 25,
        .timeZone = makeTimeZone(1, 0),
        .latitude = 51.507222,
        .longitude = -0.1275,
        .civilDawn = QDateTime(QDate(2025, 4, 30), QTime(5, 0), makeTimeZone(1, 0)),
        .civilDusk = QDateTime(QDate(2025, 4, 30), QTime(21, 0), makeTimeZone(1, 0)),
    };

    QTest::addRow("Oulu on 4/30/25") << Sample{
        .date = QDate(2025, 4, 30),
        .firstHour = 2,
        .lastHour = 25,
        .timeZone = makeTimeZone(3, 0),
        .latitude = 65.014167,
        .longitude = 25.471944,
        .civilDawn = QDateTime(QDate(2025, 4, 30), QTime(3, 40), makeTimeZone(3, 0)),
        .civilDusk = QDateTime(QDate(2025, 4, 30), QTime(22, 55), makeTimeZone(3, 0)),
        .tolerance = 10 * 60,
    };

    QTest::addRow("Denver on 4/30/25") << Sample{
        .date = QDate(2025, 4, 30),
        .firstHour = 2,
        .lastHour = 25,
        .timeZone = makeTimeZone(-6, 0),
        .latitude = 39.7392,
        .longitude = -104.9849,
        .civilDawn = QDateTime(QDate(2025, 4, 30), QTime(5, 35), makeTimeZone(-6, 0)),
        .civilDusk = QDateTime(QDate(2025, 4, 30), QTime(20, 25), makeTimeZone(-6, 0)),
    };

    QTest::addRow("San Francisco on 4/30/25") << Sample{
        .date = QDate(2025, 4, 30),
        .firstHour = 2,
        .lastHour = 25,
        .timeZone = makeTimeZone(-7, 0),
        .latitude = 37.783333,
        .longitude = -122.416667,
        .civilDawn = QDateTime(QDate(2025, 4, 30), QTime(5, 50), makeTimeZone(-7, 0)),
        .civilDusk = QDateTime(QDate(2025, 4, 30), QTime(20, 30), makeTimeZone(-7, 0)),
    };

    QTest::addRow("New York on 4/30/25") << Sample{
        .date = QDate(2025, 4, 30),
        .firstHour = 1,
        .lastHour = 24,
        .timeZone = makeTimeZone(-4, 0),
        .latitude = 40.712778,
        .longitude = -74.006111,
        .civilDawn = QDateTime(QDate(2025, 4, 30), QTime(5, 30), makeTimeZone(-4, 0)),
        .civilDusk = QDateTime(QDate(2025, 4, 30), QTime(20, 20), makeTimeZone(-4, 0)),
    };

    QTest::addRow("Tokyo on 4/30/25") << Sample{
        .date = QDate(2025, 4, 30),
        .firstHour = 0,
        .lastHour = 23,
        .timeZone = makeTimeZone(9, 0),
        .latitude = 35.689722,
        .longitude = 139.692222,
        .civilDawn = QDateTime(QDate(2025, 4, 30), QTime(4, 25), makeTimeZone(9, 0)),
        .civilDusk = QDateTime(QDate(2025, 4, 30), QTime(18, 55), makeTimeZone(9, 0)),
    };

    QTest::addRow("Beijing on 4/30/25") << Sample{
        .date = QDate(2025, 4, 30),
        .firstHour = 1,
        .lastHour = 24,
        .timeZone = makeTimeZone(8, 0),
        .latitude = 39.906667,
        .longitude = 116.3975,
        .civilDawn = QDateTime(QDate(2025, 4, 30), QTime(4, 50), makeTimeZone(8, 0)),
        .civilDusk = QDateTime(QDate(2025, 4, 30), QTime(19, 40), makeTimeZone(8, 0)),
    };

    QTest::addRow("Cape Town on 4/30/25") << Sample{
        .date = QDate(2025, 4, 30),
        .firstHour = 1,
        .lastHour = 24,
        .timeZone = makeTimeZone(2, 0),
        .latitude = -33.925278,
        .longitude = 18.423889,
        .civilDawn = QDateTime(QDate(2025, 4, 30), QTime(6, 55), makeTimeZone(2, 0)),
        .civilDusk = QDateTime(QDate(2025, 4, 30), QTime(18, 35), makeTimeZone(2, 0)),
    };

    QTest::addRow("Sao Paulo on 4/30/25") << Sample{
        .date = QDate(2025, 4, 30),
        .firstHour = 1,
        .lastHour = 24,
        .timeZone = makeTimeZone(-3, 0),
        .latitude = -23.55,
        .longitude = -46.633333,
        .civilDawn = QDateTime(QDate(2025, 4, 30), QTime(6, 5), makeTimeZone(-3, 0)),
        .civilDusk = QDateTime(QDate(2025, 4, 30), QTime(18, 5), makeTimeZone(-3, 0)),
    };
}

void SolarTransitTest::transitTime()
{
    QFETCH(Sample, sample);

    for (int cycle = -1; cycle <= 1; ++cycle) {
        for (int hour = sample.firstHour; hour <= sample.lastHour; ++hour) {
            const QDateTime dateTime = QDateTime(sample.date, QTime(0, 0), sample.timeZone)
                                           .addDays(cycle)
                                           .addSecs(hour * 60 * 60);

            const QDateTime expectedCivilDawn = sample.civilDawn.addDays(cycle);
            const QDateTime expectedCivilDusk = sample.civilDusk.addDays(cycle);

            const auto morning = calculateSunTimings(dateTime, sample.latitude, sample.longitude, true);
            const int dawnDiff = std::abs(morning.first.secsTo(expectedCivilDawn));
            if (dawnDiff > sample.tolerance) {
                QFAIL(QStringLiteral("Dawn time is different at %1: %2 (expected %3)")
                          .arg(dateTime.toString(), morning.first.toString(), expectedCivilDawn.toString())
                          .toUtf8());
            }

            const auto evening = calculateSunTimings(dateTime, sample.latitude, sample.longitude, false);
            const int duskDiff = std::abs(evening.second.secsTo(expectedCivilDusk));
            if (duskDiff > sample.tolerance) {
                QFAIL(QStringLiteral("Dusk time is different at %1: %2 (expected %3)")
                          .arg(dateTime.toString(), evening.second.toString(), expectedCivilDusk.toString())
                          .toUtf8());
            }
        }
    }
}

QTEST_MAIN(SolarTransitTest)
#include "suncalc_test.moc"
