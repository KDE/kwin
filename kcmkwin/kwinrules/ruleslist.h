/*
 * Copyright (c) 2004 Lubos Lunak <l.lunak@kde.org>
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


#ifndef __RULESLIST_H__
#define __RULESLIST_H__

#include "../../rules.h"

#include "ui_ruleslist.h"

namespace KWin
{

class KCMRulesList
    : public QWidget, Ui_KCMRulesList
{
    Q_OBJECT
public:
    explicit KCMRulesList(QWidget* parent = nullptr);
    ~KCMRulesList() override;
    void load();
    void save();
    void defaults();
Q_SIGNALS:
    void changed(bool);
private Q_SLOTS:
    void newClicked();
    void modifyClicked();
    void deleteClicked();
    void moveupClicked();
    void movedownClicked();
    void exportClicked();
    void importClicked();
    void activeChanged();
private:
    QVector< Rules* > rules;
};

} // namespace

#endif
