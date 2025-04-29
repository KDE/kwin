/*
    SPDX-FileCopyrightText: 2025 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "plugins/nightlight/suntransit.h"

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
    QDateTime sunrise;
    QDateTime sunset;
    QDateTime civilDusk;
    qreal tolerance = 5 * 60;
};

class SunTransitTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void pole();
    void transitTime_data();
    void transitTime();
};

void SunTransitTest::pole()
{
    for (qreal latitude : {-90, 90}) {
        const SunTransit transit(QDateTime::currentDateTime(), latitude, 0);

        QVERIFY(transit.dateTime(SunTransit::CivilDawn).isNull());
        QVERIFY(transit.dateTime(SunTransit::Sunrise).isNull());
        QVERIFY(transit.dateTime(SunTransit::Sunset).isNull());
        QVERIFY(transit.dateTime(SunTransit::CivilDusk).isNull());
    }
}

static QTimeZone makeTimeZone(int hours, int minutes = 0)
{
    return QTimeZone::fromSecondsAheadOfUtc(hours * 60 * 60 + minutes * 60);
}

void SunTransitTest::transitTime_data()
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
        .sunrise = QDateTime(QDate(2025, 4, 30), QTime(6, 50), makeTimeZone(9, 30)),
        .sunset = QDateTime(QDate(2025, 4, 30), QTime(17, 35), makeTimeZone(9, 30)),
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
        .sunrise = QDateTime(QDate(2025, 4, 30), QTime(7, 10), makeTimeZone(12, 0)),
        .sunset = QDateTime(QDate(2025, 4, 30), QTime(17, 30), makeTimeZone(12, 0)),
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
        .sunrise = QDateTime(QDate(2025, 4, 30), QTime(5, 35), makeTimeZone(3, 0)),
        .sunset = QDateTime(QDate(2025, 4, 30), QTime(20, 15), makeTimeZone(3, 0)),
        .civilDusk = QDateTime(QDate(2025, 4, 30), QTime(20, 55), makeTimeZone(3, 0)),
    };

    QTest::addRow("London on 4/30/25") << Sample{
        .date = QDate(2025, 4, 30),
        .firstHour = 2,
        .lastHour = 25,
        .timeZone = makeTimeZone(1, 0),
        .latitude = 51.507222,
        .longitude = -0.1275,
        .civilDawn = QDateTime(QDate(2025, 4, 30), QTime(4, 55), makeTimeZone(1, 0)),
        .sunrise = QDateTime(QDate(2025, 4, 30), QTime(5, 35), makeTimeZone(1, 0)),
        .sunset = QDateTime(QDate(2025, 4, 30), QTime(20, 20), makeTimeZone(1, 0)),
        .civilDusk = QDateTime(QDate(2025, 4, 30), QTime(21, 0), makeTimeZone(1, 0)),
    };

    QTest::addRow("Oulu on 4/30/25") << Sample{
        .date = QDate(2025, 4, 30),
        .firstHour = 2,
        .lastHour = 25,
        .timeZone = makeTimeZone(3, 0),
        .latitude = 65.014167,
        .longitude = 25.471944,
        .civilDawn = QDateTime(QDate(2025, 4, 30), QTime(3, 30), makeTimeZone(3, 0)),
        .sunrise = QDateTime(QDate(2025, 4, 30), QTime(4, 45), makeTimeZone(3, 0)),
        .sunset = QDateTime(QDate(2025, 4, 30), QTime(21, 45), makeTimeZone(3, 0)),
        .civilDusk = QDateTime(QDate(2025, 4, 30), QTime(23, 0), makeTimeZone(3, 0)),
        .tolerance = 10 * 60,
    };

    QTest::addRow("Denver on 4/30/25") << Sample{
        .date = QDate(2025, 4, 30),
        .firstHour = 1,
        .lastHour = 24,
        .timeZone = makeTimeZone(-6, 0),
        .latitude = 39.7392,
        .longitude = -104.9849,
        .civilDawn = QDateTime(QDate(2025, 4, 30), QTime(5, 30), makeTimeZone(-6, 0)),
        .sunrise = QDateTime(QDate(2025, 4, 30), QTime(6, 0), makeTimeZone(-6, 0)),
        .sunset = QDateTime(QDate(2025, 4, 30), QTime(19, 55), makeTimeZone(-6, 0)),
        .civilDusk = QDateTime(QDate(2025, 4, 30), QTime(20, 25), makeTimeZone(-6, 0)),
    };

    QTest::addRow("San Francisco on 4/30/25") << Sample{
        .date = QDate(2025, 4, 30),
        .firstHour = 2,
        .lastHour = 25,
        .timeZone = makeTimeZone(-7, 0),
        .latitude = 37.783333,
        .longitude = -122.416667,
        .civilDawn = QDateTime(QDate(2025, 4, 30), QTime(5, 45), makeTimeZone(-7, 0)),
        .sunrise = QDateTime(QDate(2025, 4, 30), QTime(6, 15), makeTimeZone(-7, 0)),
        .sunset = QDateTime(QDate(2025, 4, 30), QTime(20, 0), makeTimeZone(-7, 0)),
        .civilDusk = QDateTime(QDate(2025, 4, 30), QTime(20, 30), makeTimeZone(-7, 0)),
    };

    QTest::addRow("New York on 4/30/25") << Sample{
        .date = QDate(2025, 4, 30),
        .firstHour = 1,
        .lastHour = 24,
        .timeZone = makeTimeZone(-4, 0),
        .latitude = 40.712778,
        .longitude = -74.006111,
        .civilDawn = QDateTime(QDate(2025, 4, 30), QTime(5, 25), makeTimeZone(-4, 0)),
        .sunrise = QDateTime(QDate(2025, 4, 30), QTime(5, 55), makeTimeZone(-4, 0)),
        .sunset = QDateTime(QDate(2025, 4, 30), QTime(19, 50), makeTimeZone(-4, 0)),
        .civilDusk = QDateTime(QDate(2025, 4, 30), QTime(20, 20), makeTimeZone(-4, 0)),
    };

    QTest::addRow("Tokyo on 4/30/25") << Sample{
        .date = QDate(2025, 4, 30),
        .firstHour = 0,
        .lastHour = 23,
        .timeZone = makeTimeZone(9, 0),
        .latitude = 35.689722,
        .longitude = 139.692222,
        .civilDawn = QDateTime(QDate(2025, 4, 30), QTime(4, 20), makeTimeZone(9, 0)),
        .sunrise = QDateTime(QDate(2025, 4, 30), QTime(4, 50), makeTimeZone(9, 0)),
        .sunset = QDateTime(QDate(2025, 4, 30), QTime(18, 25), makeTimeZone(9, 0)),
        .civilDusk = QDateTime(QDate(2025, 4, 30), QTime(18, 55), makeTimeZone(9, 0)),
    };

    QTest::addRow("Beijing on 4/30/25") << Sample{
        .date = QDate(2025, 4, 30),
        .firstHour = 1,
        .lastHour = 24,
        .timeZone = makeTimeZone(8, 0),
        .latitude = 39.906667,
        .longitude = 116.3975,
        .civilDawn = QDateTime(QDate(2025, 4, 30), QTime(4, 45), makeTimeZone(8, 0)),
        .sunrise = QDateTime(QDate(2025, 4, 30), QTime(5, 15), makeTimeZone(8, 0)),
        .sunset = QDateTime(QDate(2025, 4, 30), QTime(19, 5), makeTimeZone(8, 0)),
        .civilDusk = QDateTime(QDate(2025, 4, 30), QTime(19, 35), makeTimeZone(8, 0)),
    };

    QTest::addRow("Cape Town on 4/30/25") << Sample{
        .date = QDate(2025, 4, 30),
        .firstHour = 1,
        .lastHour = 24,
        .timeZone = makeTimeZone(2, 0),
        .latitude = -33.925278,
        .longitude = 18.423889,
        .civilDawn = QDateTime(QDate(2025, 4, 30), QTime(6, 55), makeTimeZone(2, 0)),
        .sunrise = QDateTime(QDate(2025, 4, 30), QTime(7, 20), makeTimeZone(2, 0)),
        .sunset = QDateTime(QDate(2025, 4, 30), QTime(18, 5), makeTimeZone(2, 0)),
        .civilDusk = QDateTime(QDate(2025, 4, 30), QTime(18, 30), makeTimeZone(2, 0)),
    };

    QTest::addRow("Sao Paulo on 4/30/25") << Sample{
        .date = QDate(2025, 4, 30),
        .firstHour = 1,
        .lastHour = 24,
        .timeZone = makeTimeZone(-3, 0),
        .latitude = -23.55,
        .longitude = -46.633333,
        .civilDawn = QDateTime(QDate(2025, 4, 30), QTime(6, 5), makeTimeZone(-3, 0)),
        .sunrise = QDateTime(QDate(2025, 4, 30), QTime(6, 25), makeTimeZone(-3, 0)),
        .sunset = QDateTime(QDate(2025, 4, 30), QTime(17, 40), makeTimeZone(-3, 0)),
        .civilDusk = QDateTime(QDate(2025, 4, 30), QTime(18, 5), makeTimeZone(-3, 0)),
    };
}

#define COMPARE_DATETIME(context, actual, expected, tolerance)                               \
    do {                                                                                     \
        const int difference = std::abs(actual.secsTo(expected));                            \
        if (difference > tolerance) {                                                        \
            QFAIL(QStringLiteral("%1: %2 (expected %3)")                                     \
                      .arg((context).toString(), (actual).toString(), (expected).toString()) \
                      .toUtf8());                                                            \
        }                                                                                    \
    } while (false);

void SunTransitTest::transitTime()
{
    QFETCH(Sample, sample);

    for (int cycle = -1; cycle <= 1; ++cycle) {
        for (int hour = sample.firstHour; hour <= sample.lastHour; ++hour) {
            const QDateTime dateTime = QDateTime(sample.date, QTime(0, 0), sample.timeZone)
                                           .addDays(cycle)
                                           .addSecs(hour * 60 * 60);

            const SunTransit transit(dateTime, sample.latitude, sample.longitude);

            COMPARE_DATETIME(dateTime, transit.dateTime(SunTransit::CivilDawn), sample.civilDawn.addDays(cycle), sample.tolerance);
            COMPARE_DATETIME(dateTime, transit.dateTime(SunTransit::Sunrise), sample.sunrise.addDays(cycle), sample.tolerance);
            COMPARE_DATETIME(dateTime, transit.dateTime(SunTransit::Sunset), sample.sunset.addDays(cycle), sample.tolerance);
            COMPARE_DATETIME(dateTime, transit.dateTime(SunTransit::CivilDusk), sample.civilDusk.addDays(cycle), sample.tolerance);
        }
    }
}

QTEST_MAIN(SunTransitTest)
#include "suntransit_test.moc"
