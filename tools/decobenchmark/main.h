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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Cambridge, MA 02110-1301, USA.
 */

#ifndef BENCH_MAIN_H
#define BENCH_MAIN_H

enum Tests {
	AllTests,
	RepaintTest,
	CaptionTest,
	ResizeTest,
	RecreationTest
};

class DecoBenchApplication : public KApplication
{
	Q_OBJECT
public:
	DecoBenchApplication(const QString &library, Tests tests, int count);
	~DecoBenchApplication();

public slots:
	void executeTest();

private:
	KDecorationPreview *preview;
	KDecorationPlugins *plugins;
	Tests m_tests;
	int m_count;
};

#endif // BENCH_MAIN_H

// kate: space-indent off; tab-width 4;
