/*
 *
 * Copyright (c) 2005 Sandro Giessl <sandro@giessl.com>
 * Copyright (c) 2005 Luciano Montanaro <mikelima@cirulla.net>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <qtimer.h>

#include <kdebug.h>
#include <kconfig.h>
#include <kdecoration_plugins_p.h>
#include <kdecorationfactory.h>

#include <time.h>
#include <sys/timeb.h>
#include <iostream>


#include <kaboutdata.h>
#include <kapplication.h>
#include <kcmdlineargs.h>

#include "preview.h"
#include "main.h"

static KCmdLineOptions options[] =
{
	{ "+decoration", "Decoration library to use, such as kwin3_plastik.", 0 },
	{ "+tests", "Which test should be executed ('all', 'repaint', 'caption', 'resize')", 0 },
	{ "+repetitions", "Number of test repetitions.", 0 },
	{ 0, 0, 0 }
};

DecoBenchApplication::DecoBenchApplication(const QString &library, Tests tests, int count) :
		m_tests(tests),
		m_count(count)
{
	KConfig kwinConfig("kwinrc");
	kwinConfig.setGroup("Style");

	plugins = new KDecorationPreviewPlugins( &kwinConfig );
	preview = new KDecorationPreview( 0 );

	if (plugins->loadPlugin(library) && preview->recreateDecoration(plugins) )
		kdDebug() << "decoration " << library << " created..." << endl;

	preview->show();
}

DecoBenchApplication::~DecoBenchApplication()
{
	delete preview;
	delete plugins;
}

void DecoBenchApplication::executeTest()
{
	clock_t stime = clock();
	timeb astart, aend;
	ftime(&astart);

	if (m_tests == AllTests || m_tests == RepaintTest)
		preview->performRepaintTest(m_count);
	if (m_tests == AllTests || m_tests == CaptionTest)
		preview->performCaptionTest(m_count);
	if (m_tests == AllTests || m_tests == ResizeTest)
		preview->performResizeTest(m_count);

	clock_t etime = clock();
	ftime(&aend);

	long long time_diff = (aend.time - astart.time)*1000+aend.millitm - astart.millitm;
	kdDebug() << "Total:" << (float(time_diff)/1000) << endl;
	quit();
}

int main(int argc, char** argv)
{
	QString style = "keramik";
	// KApplication app(argc, argv);
	KAboutData about("decobenchmark", "DecoBenchmark", "0.1", "kwin decoration performance tester...", KAboutData::License_LGPL, "(C) 2005 Sandro Giessl");
	KCmdLineArgs::init(argc, argv, &about);
	KCmdLineArgs::addCmdLineOptions( options );

	KCmdLineArgs *args = KCmdLineArgs::parsedArgs();

	if (args->count() != 3)
		KCmdLineArgs::usage("Wrong number of arguments!");

	QString library = QString(args->arg(0) );
	QString t = QString(args->arg(1) );
	int count = QString(args->arg(2) ).toInt();

	Tests test;
	if (t == "all")
		test = AllTests;
	else if (t == "repaint")
		test = RepaintTest;
	else if (t == "caption")
		test = CaptionTest;
	else if (t == "resize")
		test = ResizeTest;
	else
		KCmdLineArgs::usage("Specify a valid test!");

	DecoBenchApplication app(library, test, count);

	QTimer::singleShot(0, &app, SLOT(executeTest()));
	app.exec();
}
#include "main.moc"

// kate: space-indent off; tab-width 4;
