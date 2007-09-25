/*****************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2007 Rivo Laks <rivolaks@hot.ee>

You can Freely distribute this program under the GNU General Public
License. See the file "COPYING" for the exact licensing terms.
******************************************************************/

#ifndef KWIN_DESKTOPGRID_CONFIG_H
#define KWIN_DESKTOPGRID_CONFIG_H

#define KDE3_SUPPORT
#include <kcmodule.h>
#undef KDE3_SUPPORT

class QCheckBox;

namespace KWin
{

class DesktopGridEffectConfig : public KCModule
    {
    Q_OBJECT
    public:
        explicit DesktopGridEffectConfig(QWidget* parent = 0, const QVariantList& args = QVariantList());
        ~DesktopGridEffectConfig();

    public slots:
        virtual void save();
        virtual void load();
        virtual void defaults();

    private:
        QCheckBox* mSlide;
    };

} // namespace

#endif
