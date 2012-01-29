/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2007 Rivo Laks <rivolaks@hot.ee>
Copyright (C) 2008 Lucas Murray <lmurray@undefinedfire.com>

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

#ifndef KWIN_INVERT_H
#define KWIN_INVERT_H

#include <kwineffects.h>

namespace KWin
{

class GLShader;

/**
 * Inverts desktop's colors
 **/
class InvertEffect
    : public Effect
{
    Q_OBJECT
public:
    InvertEffect();
    ~InvertEffect();

    virtual void drawWindow(EffectWindow* w, int mask, QRegion region, WindowPaintData& data);
    virtual void prePaintScreen(ScreenPrePaintData &data, int time);
    virtual void prePaintWindow(EffectWindow *w, WindowPrePaintData &data, int time);
    virtual void paintEffectFrame(KWin::EffectFrame* frame, QRegion region, double opacity, double frameOpacity);
    virtual bool isActive() const;

    static bool supported();

public slots:
    void toggle();
    void toggleWindow();
    void slotWindowClosed(KWin::EffectWindow *w);

protected:
    bool loadData();

private:
    bool m_inited;
    bool m_valid;
    GLShader* m_shader;
    bool m_allWindows;
    QList<EffectWindow*> m_windows;
};

} // namespace

#endif
