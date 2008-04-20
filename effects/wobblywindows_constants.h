/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

 Copyright (C) 2008 Cédric Borgese <cedric.borgese@gmail.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/

#include <QtGlobal>

static const qreal STIFFNESS = 0.06;
static const qreal DRAG = 0.92;
static const qreal MOVEFACTOR = 0.1;

static const int XTESSELATION = 20;
static const int YTESSELATION = 20;

static const qreal MINVELOCITY = 0.0;
static const qreal MAXVELOCITY = 1000.0;
static const qreal STOPVELOCITY = 3.0;
static const qreal MINACCELERATION = 0.0;
static const qreal MAXACCELERATION = 1000.0;
static const qreal STOPACCELERATION = 5.0;

static const char* VELOCITYFILTER = "FourRingLinearMean";
static const char* ACCELERATIONFILTER = "FourRingLinearMean";
