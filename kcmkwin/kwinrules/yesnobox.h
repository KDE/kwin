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


#ifndef YESNOBOX_H
#define YESNOBOX_H

#include <QHBoxLayout>
#include <QRadioButton>

#include <klocale.h>

class YesNoBox : public QWidget {
    Q_OBJECT
public:
    YesNoBox( QWidget *parent ) : QWidget(parent)
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
    bool isChecked() { return yes->isChecked(); }
public slots:
    void setChecked(bool b) { yes->setChecked(b); }
    void toggle() { yes->toggle(); }

signals:
    void clicked(bool checked = false);
    void toggled(bool checked);
private slots:
    void noClicked(bool checked) { emit clicked(!checked); }
private:
    QRadioButton *yes, *no;
};

#endif // YESNOBOX_H
