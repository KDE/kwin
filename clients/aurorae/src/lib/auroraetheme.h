/*
    Library for Aurorae window decoration themes.
    Copyright (C) 2009, 2010 Martin Gräßlin <kde@martin-graesslin.com>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

*/

#ifndef AURORAETHEME_H
#define AURORAETHEME_H

// #include "libaurorae_export.h"

#include <QtCore/QObject>
#include <QtGui/QGraphicsItem>
#include <kdecoration.h>

namespace Plasma {
class FrameSvg;
}
class KConfig;

namespace Aurorae {
class AuroraeThemePrivate;
class AuroraeTab;
class AuroraeScene;
class AuroraeButton;
class AuroraeMaximizeButton;
class AuroraeSpacer;
class ThemeConfig;

enum AuroraeButtonType {
    MinimizeButton = QGraphicsItem::UserType+1,
    MaximizeButton,
    RestoreButton,
    CloseButton,
    AllDesktopsButton,
    KeepAboveButton,
    KeepBelowButton,
    ShadeButton,
    HelpButton,
    MenuButton
};

enum DecorationPosition {
    DecorationTop = 0,
    DecorationLeft,
    DecorationRight,
    DecorationBottom
};

class /*LIBAURORAE_EXPORT*/ AuroraeTheme : public QObject
{
    Q_OBJECT
public:
    AuroraeTheme(QObject* parent = 0);
    virtual ~AuroraeTheme();
    // TODO: KSharedConfigPtr
    void loadTheme(const QString &name, const KConfig &config);
    bool isValid() const;
    void readThemeConfig(const KConfig& config);
    const QString &themeName() const;
    /**
    * Sets the borders according to maximized state.
    * Borders are global to all windows.
    */
    void borders(int &left, int &top, int &right, int &bottom, bool maximized) const;
    /**
    * Sets the padding according.
    * Padding is global to all windows.
    */
    void padding(int &left, int &top, int &right, int &bottom) const;
    /**
    * Sets the title edges according to maximized state.
    * Title edges are global to all windows.
    */
    void titleEdges(int &left, int &top, int &right, int &bottom, bool maximized) const;
    /**
    * Sets the button margins.
    * Button margins are global to all windows. There is no button margin bottom.
    */
    void buttonMargins(int& left, int& top, int& right) const;
    void setCompositingActive(bool active);
    bool isCompositingActive() const;

    /**
    * @returns The FrameSvg containing the decoration.
    */
    Plasma::FrameSvg *decoration() const;
    /**
    * @returns The FrameSvg for the specified button or NULL if there is no such button.
    */
    Plasma::FrameSvg *button(AuroraeButtonType button) const;
    /**
    * @returns true if the theme contains a FrameSvg for specified button.
    */
    bool hasButton(AuroraeButtonType button) const;
    QString defaultButtonsLeft() const;
    QString defaultButtonsRight() const;
    void setBorderSize(KDecorationDefines::BorderSize size);
    bool isShowTooltips() const;
    /**
    * Sets the size of the buttons.
    * The available sizes are identical to border sizes, therefore BorderSize is used.
    * @param size The buttons size
    */
    void setButtonSize(KDecorationDefines::BorderSize size);
    qreal buttonSizeFactor() const;

    DecorationPosition decorationPosition() const;

    // TODO: move to namespace
    static QLatin1String mapButtonToName(AuroraeButtonType type);
    static char mapButtonToChar(AuroraeButtonType type);

public Q_SLOTS:
    void setShowTooltips(bool show);

Q_SIGNALS:
    void themeChanged();
    void showTooltipsChanged(bool show);
    void buttonSizesChanged();

private:
    const ThemeConfig &themeConfig() const;

    AuroraeThemePrivate* const d;

    friend class Aurorae::AuroraeButton;
    friend class Aurorae::AuroraeMaximizeButton;
    friend class Aurorae::AuroraeSpacer;
    friend class Aurorae::AuroraeScene;
    friend class Aurorae::AuroraeTab;
};

} // namespace

#endif // AURORAETHEME_H
