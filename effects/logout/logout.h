/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2007 Lubos Lunak <l.lunak@kde.org>
Copyright (C) 2009 Martin Gräßlin <mgraesslin@kde.org>
Copyright (C) 2009, 2010 Lucas Murray <lmurray@undefinedfire.com>

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

#ifndef KWIN_LOGOUT_H
#define KWIN_LOGOUT_H

#include <kwineffects.h>
#include <xcbutils.h>


namespace KWin
{

class GLRenderTarget;
class GLTexture;

class WindowAttributes {
public:
    WindowAttributes(const WindowPaintData &data);
    void applyTo(WindowPaintData &data) const;
    qreal opacity = 1.0;
    qreal rotation = 0.0;
    QVector3D rotationAxis;
    QVector3D rotationOrigin;
    QVector3D scale = QVector3D(1,1,1);
    QVector3D translation;
};

class LogoutEffect
    : public Effect
{
    Q_OBJECT
    Q_PROPERTY(bool useBlur READ isUseBlur)
public:
    LogoutEffect();
    ~LogoutEffect();
    virtual void reconfigure(ReconfigureFlags);
    virtual void prePaintScreen(ScreenPrePaintData& data, int time);
    virtual void paintScreen(int mask, QRegion region, ScreenPaintData& data);
    virtual void postPaintScreen();
    virtual void paintWindow(EffectWindow* w, int mask, QRegion region, WindowPaintData& data);
    virtual bool isActive() const;

    int requestedEffectChainPosition() const override {
        return 85;
    }

    // for properties
    bool isUseBlur() const {
        return useBlur;
    }
public Q_SLOTS:
    void slotWindowAdded(KWin::EffectWindow* w);
    void slotWindowClosed(KWin::EffectWindow *w);
    void slotWindowDeleted(KWin::EffectWindow *w);
    void slotPropertyNotify(KWin::EffectWindow *w, long a);
private:
    bool isLogoutDialog(EffectWindow* w);
    double progress; // 0-1
    bool displayEffect;
    EffectWindow* logoutWindow;
    bool logoutWindowClosed;
    bool logoutWindowPassed;

    // Persistent effect
    Xcb::Atom logoutAtom;
    bool canDoPersistent;
    EffectWindowList ignoredWindows;

    void renderVignetting(const QMatrix4x4 &projection);
    void renderBlurTexture(const QMatrix4x4 &projection);
    int frameDelay;
    bool blurSupported, useBlur;
    GLTexture* blurTexture;
    GLRenderTarget* blurTarget;
    typedef QPair<EffectWindow*, WindowAttributes> WinDataPair;
    QList<WinDataPair> m_windows;
    GLShader *m_vignettingShader;
    GLShader *m_blurShader;
    QString m_shadersDir;
};

} // namespace

#endif
