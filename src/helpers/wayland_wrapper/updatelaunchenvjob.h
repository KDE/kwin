/*
   Copyright (C) 2020 Kai Uwe Broulik <kde@broulik.de>

   This program is free software; you can redistribute it and/or
   modify it under the terms of the Lesser GNU General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    General Public License for more details.

   You should have received a copy of the Lesser GNU General Public License
   along with this program; see the file COPYING.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/

#pragma once

#include <KJob>
#include <QProcessEnvironment>

class QString;

/**
 * Job for updating the launch environment.
 *
 * This job adds or updates an environment variable in process environment that will be used
 * anywhere a process is launched, e.g. DBus-activation environment, KLauncher environment, etc.
 *
 * @code
 * UpdateLaunchEnvJob("XCURSOR_THEME", "NewTheme");
 * @endcode
 *
 * @since 5.19
 */
class UpdateLaunchEnvJob : public KJob
{
    Q_OBJECT

public:
    explicit UpdateLaunchEnvJob(const QString &varName, const QString &value);
    explicit UpdateLaunchEnvJob(const QProcessEnvironment &environment);
    ~UpdateLaunchEnvJob() override;

    void start() override;

private:
    class Private;
    Private *const d;
};
