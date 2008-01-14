/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2007 Rivo Laks <rivolaks@hot.ee>

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

#ifndef KWIN_MAKETRANSPARENT_CONFIG_H
#define KWIN_MAKETRANSPARENT_CONFIG_H

#define KDE3_SUPPORT
#include <kcmodule.h>
#undef KDE3_SUPPORT

class QSpinBox;

namespace KWin
{

class MakeTransparentEffectConfig : public KCModule
    {
    Q_OBJECT
    public:
        explicit MakeTransparentEffectConfig(QWidget* parent = 0, const QVariantList& args = QVariantList());

        virtual void save();
        virtual void load();
        virtual void defaults();

    private:
        QSpinBox* mDecoration;
        QSpinBox* mMoveResize;
        QSpinBox* mDialogs;
        QSpinBox* mInactive;
    };

} // namespace

#endif
