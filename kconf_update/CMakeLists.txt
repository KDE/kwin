# SPDX-FileCopyrightText: 2023 Niccolò Venerandi <niccolo.venerandi@kde.org>
# SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL

install(FILES kwin.upd
        DESTINATION ${KDE_INSTALL_KCONFUPDATEDIR})
install(PROGRAMS kwin-6.0-overview-activities-shortcuts.py
        DESTINATION ${KDE_INSTALL_KCONFUPDATEDIR})
