/*
    SPDX-FileCopyrightText: 2004 Lubos Lunak <l.lunak@kde.org>
    SPDX-FileCopyrightText: 2020 Ismael Asensio <isma.af@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef KWIN_RULESDIALOG_H
#define KWIN_RULESDIALOG_H

#include "rulesmodel.h"
#include "../../rules.h"

#include <QDialog>

namespace KWin
{

class Rules;

class RulesDialog : public QDialog
{
    Q_OBJECT

public:
    explicit RulesDialog(QWidget* parent = nullptr, const char* name = nullptr);

    Rules* edit(Rules* r, const QVariantMap& info, bool show_hints);

protected:
    void accept() override;

private:
    RulesModel* m_rulesModel;
    QWidget *m_quickWidget;
    Rules* m_rules;
};

} // namespace

#endif // KWIN_RULESDIALOG_H
