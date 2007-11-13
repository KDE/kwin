/*****************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2007 Christian Nitschkowski <christian.nitschkowski@kdemail.net>

You can Freely distribute this program under the GNU General Public
License. See the file "COPYING" for the exact licensing terms.
******************************************************************/

#ifndef KWIN_DIMINACTIVE_CONFIG_H
#define KWIN_DIMINACTIVE_CONFIG_H

#define KDE3_SUPPORT
#include <kcmodule.h>
#undef KDE3_SUPPORT

#include "ui_diminactive_config.h"

namespace KWin
{

class DimInactiveEffectConfigForm : public QWidget, public Ui::DimInactiveEffectConfigForm
{
    Q_OBJECT
    public:
        explicit DimInactiveEffectConfigForm(QWidget* parent);
};

class DimInactiveEffectConfig : public KCModule
    {
    Q_OBJECT
    public:
        explicit DimInactiveEffectConfig(QWidget* parent = 0, const QVariantList& args = QVariantList());

        virtual void save();
        virtual void load();
        virtual void defaults();

    private:
        DimInactiveEffectConfigForm* m_ui;
    };

} // namespace

#endif
