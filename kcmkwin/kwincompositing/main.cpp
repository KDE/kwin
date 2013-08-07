/**************************************************************************
 * KWin - the KDE window manager                                          *
 * This file is part of the KDE project.                                  *
 *                                                                        *
 * Copyright (C) 2013 Antonis Tsiapaliokas <kok3rs@gmail.com>             *
 *                                                                        *
 * This program is free software; you can redistribute it and/or modify   *
 * it under the terms of the GNU General Public License as published by   *
 * the Free Software Foundation; either version 2 of the License, or      *
 * (at your option) any later version.                                    *
 *                                                                        *
 * This program is distributed in the hope that it will be useful,        *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of         *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the          *
 * GNU General Public License for more details.                           *
 *                                                                        *
 * You should have received a copy of the GNU General Public License      *
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.  *
 **************************************************************************/



#include "model.h"
#include <QApplication>

#include <KAboutData>
#include <kcmdlineargs.h>

int main(int argc, char *argv[])
{
    KAboutData aboutData("kwincompositing", 0, i18n("KWinCompositing"),
                         "1.0", i18n("Plasma Add-Ons Creator"),
                         KAboutData::License_GPL,
                         i18n("Copyright 2009-2013 Plasma Development Team"),
                         "", "", "kwin@kde.org");

    aboutData.addAuthor(i18n("Antonis Tsiapaliokas"),
                        i18n("Author and maintainer"),
                        "kok3rs@gmail.com");

    KCmdLineArgs::init(argc, argv,
                       "kwincompositing", "",
                       ki18n("KWinCompositing"),
                       "0.1");

    QApplication app(argc, argv);

    KWin::Compositing::EffectView *view = new KWin::Compositing::EffectView();
    view->show();

    return app.exec();
}
