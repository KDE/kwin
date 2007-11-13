/*****************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2007 Christian Nitschkowski <christian.nitschkowski@kdemail.net>

You can Freely distribute this program under the GNU General Public
License. See the file "COPYING" for the exact licensing terms.
******************************************************************/

#ifndef KWIN_MOUSEMARK_CONFIG_H
#define KWIN_MOUSEMARK_CONFIG_H

#define KDE3_SUPPORT
#include <kcmodule.h>
#undef KDE3_SUPPORT

#include "ui_mousemark_config.h"

class KActionCollection;

namespace KWin
{

class MouseMarkEffectConfigForm : public QWidget, public Ui::MouseMarkEffectConfigForm
{
    Q_OBJECT
    public:
        explicit MouseMarkEffectConfigForm(QWidget* parent);
};

class MouseMarkEffectConfig : public KCModule
    {
    Q_OBJECT
    public:
      explicit MouseMarkEffectConfig(QWidget* parent = 0, const QVariantList& args = QVariantList());

        virtual void save();
        virtual void load();
        virtual void defaults();

    private:
        MouseMarkEffectConfigForm* m_ui;
        KActionCollection* m_actionCollection;
    };

} // namespace

#endif
