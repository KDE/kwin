/*
    Library for Aurorae window decoration themes.
    Copyright (C) 2009, 2010, 2012 Martin Gräßlin <mgraesslin@kde.org>

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
    Q_PROPERTY(int borderLeft READ leftBorder NOTIFY themeChanged)
    Q_PROPERTY(int borderRight READ rightBorder NOTIFY themeChanged)
    Q_PROPERTY(int borderTop READ topBorder NOTIFY themeChanged)
    Q_PROPERTY(int borderBottom READ bottomBorder NOTIFY themeChanged)
    Q_PROPERTY(int borderLeftMaximized READ leftBorderMaximized NOTIFY themeChanged)
    Q_PROPERTY(int borderRightMaximized READ rightBorderMaximized NOTIFY themeChanged)
    Q_PROPERTY(int borderTopMaximized READ topBorderMaximized NOTIFY themeChanged)
    Q_PROPERTY(int borderBottomMaximized READ bottomBorderMaximized NOTIFY themeChanged)
    Q_PROPERTY(int paddingLeft READ paddingLeft NOTIFY themeChanged)
    Q_PROPERTY(int paddingRight READ paddingRight NOTIFY themeChanged)
    Q_PROPERTY(int paddingTop READ paddingTop NOTIFY themeChanged)
    Q_PROPERTY(int paddingBottom READ paddingBottom NOTIFY themeChanged)
    Q_PROPERTY(QString themeName READ themeName NOTIFY themeChanged)
    Q_PROPERTY(int buttonHeight READ buttonHeight NOTIFY themeChanged)
    Q_PROPERTY(int buttonWidth READ buttonWidth NOTIFY themeChanged)
    Q_PROPERTY(int buttonWidthMinimize READ buttonWidthMinimize NOTIFY themeChanged)
    Q_PROPERTY(int buttonWidthMaximizeRestore READ buttonWidthMaximizeRestore NOTIFY themeChanged)
    Q_PROPERTY(int buttonWidthClose READ buttonWidthClose NOTIFY themeChanged)
    Q_PROPERTY(int buttonWidthAllDesktops READ buttonWidthAllDesktops NOTIFY themeChanged)
    Q_PROPERTY(int buttonWidthKeepAbove READ buttonWidthKeepAbove NOTIFY themeChanged)
    Q_PROPERTY(int buttonWidthKeepBelow READ buttonWidthKeepBelow NOTIFY themeChanged)
    Q_PROPERTY(int buttonWidthShade READ buttonWidthShade NOTIFY themeChanged)
    Q_PROPERTY(int buttonWidthHelp READ buttonWidthHelp NOTIFY themeChanged)
    Q_PROPERTY(int buttonWidthMenu READ buttonWidthMenu NOTIFY themeChanged)
    Q_PROPERTY(int buttonSpacing READ buttonSpacing NOTIFY themeChanged)
    Q_PROPERTY(int buttonMarginTop READ buttonMarginTop NOTIFY themeChanged)
    Q_PROPERTY(int explicitButtonSpacer READ explicitButtonSpacer NOTIFY themeChanged)
    Q_PROPERTY(qreal buttonSizeFactor READ buttonSizeFactor NOTIFY buttonSizesChanged)
    Q_PROPERTY(int animationTime READ animationTime NOTIFY themeChanged)
    Q_PROPERTY(int titleEdgeLeft READ titleEdgeLeft NOTIFY themeChanged)
    Q_PROPERTY(int titleEdgeRight READ titleEdgeRight NOTIFY themeChanged)
    Q_PROPERTY(int titleEdgeTop READ titleEdgeTop NOTIFY themeChanged)
    Q_PROPERTY(int titleEdgeLeftMaximized READ titleEdgeLeftMaximized NOTIFY themeChanged)
    Q_PROPERTY(int titleEdgeRightMaximized READ titleEdgeRightMaximized NOTIFY themeChanged)
    Q_PROPERTY(int titleEdgeTopMaximized READ titleEdgeTopMaximized NOTIFY themeChanged)
    Q_PROPERTY(int titleBorderRight READ titleBorderRight NOTIFY themeChanged)
    Q_PROPERTY(int titleBorderLeft READ titleBorderLeft NOTIFY themeChanged)
    Q_PROPERTY(QString decorationPath READ decorationPath NOTIFY themeChanged)
    Q_PROPERTY(QString minimizeButtonPath READ minimizeButtonPath NOTIFY themeChanged)
    Q_PROPERTY(QString maximizeButtonPath READ maximizeButtonPath NOTIFY themeChanged)
    Q_PROPERTY(QString restoreButtonPath READ restoreButtonPath NOTIFY themeChanged)
    Q_PROPERTY(QString closeButtonPath READ closeButtonPath NOTIFY themeChanged)
    Q_PROPERTY(QString allDesktopsButtonPath READ allDesktopsButtonPath NOTIFY themeChanged)
    Q_PROPERTY(QString keepAboveButtonPath READ keepAboveButtonPath NOTIFY themeChanged)
    Q_PROPERTY(QString keepBelowButtonPath READ keepBelowButtonPath NOTIFY themeChanged)
    Q_PROPERTY(QString shadeButtonPath READ shadeButtonPath NOTIFY themeChanged)
    Q_PROPERTY(QString helpButtonPath READ helpButtonPath NOTIFY themeChanged)
    Q_PROPERTY(QColor activeTextColor READ activeTextColor NOTIFY themeChanged)
    Q_PROPERTY(QColor inactiveTextColor READ inactiveTextColor NOTIFY themeChanged)
    Q_PROPERTY(Qt::Alignment horizontalAlignment READ alignment NOTIFY themeChanged)
    Q_PROPERTY(Qt::Alignment verticalAlignment READ verticalAlignment NOTIFY themeChanged)
public:
    AuroraeTheme(QObject* parent = 0);
    virtual ~AuroraeTheme();
    // TODO: KSharedConfigPtr
    void loadTheme(const QString &name, const KConfig &config);
    bool isValid() const;
    void readThemeConfig(const KConfig& config);
    const QString &themeName() const;
    int leftBorder() const;
    int rightBorder() const;
    int topBorder() const;
    int bottomBorder() const;
    int leftBorderMaximized() const;
    int rightBorderMaximized() const;
    int topBorderMaximized() const;
    int bottomBorderMaximized() const;
    int paddingLeft() const;
    int paddingRight() const;
    int paddingTop() const;
    int paddingBottom() const;
    int buttonWidth() const;
    int buttonWidthMinimize() const;
    int buttonWidthMaximizeRestore() const;
    int buttonWidthClose() const;
    int buttonWidthAllDesktops() const;
    int buttonWidthKeepAbove() const;
    int buttonWidthKeepBelow() const;
    int buttonWidthShade() const;
    int buttonWidthHelp() const;
    int buttonWidthMenu() const;
    int buttonHeight() const;
    int buttonSpacing() const;
    int buttonMarginTop() const;
    int explicitButtonSpacer() const;
    int animationTime() const;
    int titleEdgeLeft() const;
    int titleEdgeRight() const;
    int titleEdgeTop() const;
    int titleEdgeLeftMaximized() const;
    int titleEdgeRightMaximized() const;
    int titleEdgeTopMaximized() const;
    int titleBorderLeft() const;
    int titleBorderRight() const;
    QString decorationPath() const;
    QString minimizeButtonPath() const;
    QString maximizeButtonPath() const;
    QString restoreButtonPath() const;
    QString closeButtonPath() const;
    QString allDesktopsButtonPath() const;
    QString keepAboveButtonPath() const;
    QString keepBelowButtonPath() const;
    QString shadeButtonPath() const;
    QString helpButtonPath() const;
    QColor activeTextColor() const;
    QColor inactiveTextColor() const;
    Qt::Alignment alignment() const;
    Qt::Alignment verticalAlignment() const;
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

    void setTabDragMimeType(const QString &mime);
    const QString &tabDragMimeType() const;

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
