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

#include "modeltest.h"
#include "../model.h"
#include "effectmodeltest.h"

#include "effectmodel.h"

#include <QtTest>

EffectModelTest::EffectModelTest(QObject *parent)
    : QObject(parent) {

}

void EffectModelTest::testEffectModel() {
    auto effectModel = new KWin::EffectModel();

    new ModelTest(effectModel, this);
}

void EffectModelTest::testEffectFilterModel() {
    KWin::Compositing::EffectFilterModel *model = new KWin::Compositing::EffectFilterModel();

    new ModelTest(model, this);
}

QTEST_MAIN(EffectModelTest)
