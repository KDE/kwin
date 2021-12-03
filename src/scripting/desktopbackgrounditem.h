/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "thumbnailitem.h"

namespace KWin
{

class AbstractOutput;
class VirtualDesktop;

/**
 * The DesktopBackgroundItem type is a convenience helper that represents the desktop
 * background on the specified screen, virtual desktop, and activity.
 */
class DesktopBackgroundItem : public WindowThumbnailItem
{
    Q_OBJECT
    Q_PROPERTY(QString outputName READ outputName WRITE setOutputName NOTIFY outputChanged)
    Q_PROPERTY(KWin::AbstractOutput *output READ output WRITE setOutput NOTIFY outputChanged)
    Q_PROPERTY(QString activity READ activity WRITE setActivity NOTIFY activityChanged)
    Q_PROPERTY(KWin::VirtualDesktop *desktop READ desktop WRITE setDesktop NOTIFY desktopChanged)

public:
    explicit DesktopBackgroundItem(QQuickItem *parent = nullptr);

    void componentComplete() override;

    QString outputName() const;
    void setOutputName(const QString &name);

    AbstractOutput *output() const;
    void setOutput(AbstractOutput *output);

    VirtualDesktop *desktop() const;
    void setDesktop(VirtualDesktop *desktop);

    QString activity() const;
    void setActivity(const QString &activity);

Q_SIGNALS:
    void outputChanged();
    void desktopChanged();
    void activityChanged();

private:
    void updateWindow();

    AbstractOutput *m_output = nullptr;
    VirtualDesktop *m_desktop = nullptr;
    QString m_activity;
};

} // namespace KWin
