/*****************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2007 Rivo Laks <rivolaks@hot.ee>

You can Freely distribute this program under the GNU General Public
License. See the file "COPYING" for the exact licensing terms.
******************************************************************/

#ifndef KWIN_SHADOW_CONFIG_H
#define KWIN_SHADOW_CONFIG_H

#define KDE3_SUPPORT
#include <kcmodule.h>
#undef KDE3_SUPPORT

class QSpinBox;
class QCheckBox;

namespace KWin
{

class ShadowEffectConfig : public KCModule
    {
    Q_OBJECT
    public:
        explicit ShadowEffectConfig(QWidget* parent = 0, const QVariantList& args = QVariantList());
        ~ShadowEffectConfig();

        virtual void save();
        virtual void load();
        virtual void defaults();

    private:
        QSpinBox* mShadowXOffset;
        QSpinBox* mShadowYOffset;
        QSpinBox* mShadowOpacity;
        QSpinBox* mShadowFuzzyness;
        QSpinBox* mShadowSize;
        QCheckBox* mIntensifyActiveShadow;
    };

} // namespace

#endif
