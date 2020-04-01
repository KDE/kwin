/*
 * Copyright (c) 2011 Thomas Lübking <thomas.luebking@web.de>
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "yesnobox.h"

YesNoBox::YesNoBox( QWidget *parent )
    : QWidget(parent)
{
    QHBoxLayout *l = new QHBoxLayout(this);
    l->setContentsMargins(0, 0, 0, 0);
    l->addWidget(yes = new QRadioButton(i18n("Yes"), this));
    l->addWidget(no = new QRadioButton(i18n("No"), this));
    l->addStretch(100);
    no->setChecked(true);
    connect(yes, SIGNAL(clicked(bool)), this, SIGNAL(clicked(bool)));
    connect(yes, SIGNAL(toggled(bool)), this, SIGNAL(toggled(bool)));
    connect(no, SIGNAL(clicked(bool)), this, SLOT(noClicked(bool)));
}
