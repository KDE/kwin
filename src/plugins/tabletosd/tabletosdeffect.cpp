/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "tabletosdeffect.h"

#include "effect/effecthandler.h"
#include "input.h"
#include "inputsequence.h"

#include <QAction>
#include <QQuickItem>
#include <QTimer>

#include <KGlobalAccel>
#include <KLocalizedString>

extern "C" {
#include <libwacom/libwacom.h>
}

using namespace std::chrono_literals;

namespace KWin
{

TabletOsdEffect::TabletOsdEffect()
{
    qmlRegisterUncreatableType<TabletButton>("org.kde.KWin.Effect.tabletosd", 1, 0, "TabletButton", QStringLiteral("WindowView cannot be created in QML"));

    auto db = libwacom_database_new();
    if (db == nullptr) {
        qWarning() << "Failed to initialize libwacom database!";
    }

    auto devices = KWin::input()->devices();
    for (const auto &device : devices) {
        qInfo() << device->isTabletPad() << device->name();
    }

    const QString sysPath = QStringLiteral("/dev/input/%1").arg(QStringLiteral("event13"));

    WacomError *error = libwacom_error_new();
    auto device = libwacom_new_from_path(db, sysPath.toLatin1().constData(), WFALLBACK_GENERIC, error);
    if (device == nullptr) {
        qWarning() << "Failed to find device in libwacom:" << libwacom_error_get_message(error);
    } else {
        for (int i = 0; i < libwacom_get_num_buttons(device); i++) {
            const WacomButtonFlags flags = libwacom_get_button_flag(device, 'A' + i);

            const auto cfg = KSharedConfig::openConfig("kcminputrc");
            const auto group = cfg->group("ButtonRebinds").group("Tablet").group(QStringLiteral("UGTABLET 21.5 inch PenDisplay Pad"));
            const auto sequence = InputSequence(group.readEntry(QString::number(i), QStringList()));

            if (flags & WACOM_BUTTON_POSITION_BOTTOM) {
                m_bottomButtons.push_back(TabletButton(sequence.toString()));
            } else if (flags & WACOM_BUTTON_POSITION_TOP) {
                m_topButtons.push_back(TabletButton(sequence.toString()));
            } else if (flags & WACOM_BUTTON_POSITION_LEFT) {
                m_leftButtons.push_back(TabletButton(sequence.toString()));
            } else if (flags & WACOM_BUTTON_POSITION_RIGHT) {
                m_rightButtons.push_back(TabletButton(sequence.toString()));
            }
        }
    }
    libwacom_error_free(&error);

    qInfo() << "bottom:" << m_bottomButtons.size();
    qInfo() << "top:" << m_topButtons.size();
    qInfo() << "left:" << m_leftButtons.size();
    qInfo() << "right:" << m_rightButtons.size();

    loadFromModule(QStringLiteral("org.kde.kwin.tabletosd"), QStringLiteral("Main"));
    setRunning(true);
}

TabletOsdEffect::~TabletOsdEffect()
{
}

} // namespace KWin

#include "moc_tabletosdeffect.cpp"
