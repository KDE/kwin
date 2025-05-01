/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "windowthumbnailitem.h"

namespace KWin
{

class Output;
class VirtualDesktop;

/*!
 * \qmltype DesktopBackground
 * \inqmlmodule org.kde.kwin
 *
 * The DesktopBackgroundItem type is a convenience helper that represents the desktop
 * background on the specified screen, virtual desktop, and activity.
 */
class DesktopBackgroundItem : public WindowThumbnailItem
{
    Q_OBJECT
    /*!
     * \qmlproperty string DesktopBackground::outputName
     *
     * This property specifies the output name of the desktop wallpaper. Either the output or the
     * outputName property must be set; otherwise no desktop background will be displayed.
     */
    Q_PROPERTY(QString outputName READ outputName WRITE setOutputName NOTIFY outputChanged)

    /*!
     * \qmlproperty Output DesktopBackground::output
     *
     * This property specifies the output of the desktop wallpaper. Either the output or the outputName
     * property must be set; otherwise no desktop background will be displayed.
     */
    Q_PROPERTY(KWin::Output *output READ output WRITE setOutput NOTIFY outputChanged)

    /*!
     * \qmlproperty string DesktopBackground::activity
     *
     * This property specifies the activity of the desktop wallpaper. If it's not explicitly set
     * to any value, the first desktop background on the specified output will be used.
     */
    Q_PROPERTY(QString activity READ activity WRITE setActivity NOTIFY activityChanged)

    /*!
     * \qmlproperty VirtualDesktop DesktopBackground::desktop
     *
     * This property specifies the virtual desktop of the desktop wallpaper. If it's not explicitly
     * set to any value, the first desktop background on the specified output will be used.
     */
    Q_PROPERTY(KWin::VirtualDesktop *desktop READ desktop WRITE setDesktop NOTIFY desktopChanged)

public:
    explicit DesktopBackgroundItem(QQuickItem *parent = nullptr);

    void componentComplete() override;

    QString outputName() const;
    void setOutputName(const QString &name);

    Output *output() const;
    void setOutput(Output *output);

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

    Output *m_output = nullptr;
    VirtualDesktop *m_desktop = nullptr;
    QString m_activity;
};

} // namespace KWin
