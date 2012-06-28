/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

 Copyright (C) 2010 Martin Gräßlin <kde@martin-graesslin.com>

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

#ifndef KWIN_STARTUPFEEDBACK_H
#define KWIN_STARTUPFEEDBACK_H
#include <QObject>
#include <kwineffects.h>
#include <KDE/KStartupInfo>

class KSelectionOwner;
namespace KWin
{
class GLTexture;

class StartupFeedbackEffect
    : public Effect
{
    Q_OBJECT
public:
    StartupFeedbackEffect();
    virtual ~StartupFeedbackEffect();

    virtual void reconfigure(ReconfigureFlags flags);
    virtual void prePaintScreen(ScreenPrePaintData& data, int time);
    virtual void paintScreen(int mask, QRegion region, ScreenPaintData& data);
    virtual void postPaintScreen();
    virtual bool isActive() const;

    static bool supported();

private Q_SLOTS:
    void gotNewStartup(const KStartupInfoId& id, const KStartupInfoData& data);
    void gotRemoveStartup(const KStartupInfoId& id, const KStartupInfoData& data);
    void gotStartupChange(const KStartupInfoId& id, const KStartupInfoData& data);
    void slotMouseChanged(const QPoint& pos, const QPoint& oldpos, Qt::MouseButtons buttons, Qt::MouseButtons oldbuttons, Qt::KeyboardModifiers modifiers, Qt::KeyboardModifiers oldmodifiers);

private:
    enum FeedbackType {
        NoFeedback,
        BouncingFeedback,
        BlinkingFeedback,
        PassiveFeedback
    };
    void start(const QString& icon);
    void stop();
    QImage scalePixmap(const QPixmap& pm, const QSize& size) const;
    void prepareTextures(const QPixmap& pix);
    QRect feedbackRect() const;

    KStartupInfo* m_startupInfo;
    KSelectionOwner* m_selection;
    KStartupInfoId m_currentStartup;
    QMap< KStartupInfoId, QString > m_startups; // QString == pixmap
    bool m_active;
    int m_frame;
    int m_progress;
    GLTexture* m_bouncingTextures[5];
    GLTexture* m_texture; // for passive and blinking
    FeedbackType m_type;
    QRect m_currentGeometry, m_dirtyRect;
    GLShader *m_blinkingShader;
};
} // namespace

#endif
