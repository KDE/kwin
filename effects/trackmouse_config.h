/*****************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2007 Rivo Laks <rivolaks@hot.ee>

You can Freely distribute this program under the GNU General Public
License. See the file "COPYING" for the exact licensing terms.
******************************************************************/

#ifndef KWIN_TRACKMOUSE_CONFIG_H
#define KWIN_TRACKMOUSE_CONFIG_H

#define KDE3_SUPPORT
#include <kcmodule.h>
#undef KDE3_SUPPORT

class KShortcutsEditor;

namespace KWin
{

class TrackMouseEffectConfig : public KCModule
    {
    Q_OBJECT
    public:
        explicit TrackMouseEffectConfig(QWidget* parent = 0, const QVariantList& args = QVariantList());
        ~TrackMouseEffectConfig();

    public slots:
        virtual void save();
        virtual void load();
        virtual void defaults();
    };

} // namespace

#endif
