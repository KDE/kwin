/*
  This file is part of the KDE project.

  Copyright (C) 2005 Sandro Giessl <sandro@giessl.com>

  Permission is hereby granted, free of charge, to any person obtaining a
  copy of this software and associated documentation files (the "Software"),
  to deal in the Software without restriction, including without limitation
  the rights to use, copy, modify, merge, publish, distribute, sublicense,
  and/or sell copies of the Software, and to permit persons to whom the
  Software is furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in
  all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
  THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
  FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
  DEALINGS IN THE SOFTWARE.
 */

#ifndef KCOMMONDECORATION_H
#define KCOMMONDECORATION_H

#include <qbutton.h>
#include <qvaluevector.h>

#include "kdecoration.h"

class KDecorationBridge;
class KDecorationFactory;

enum ButtonType {
    HelpButton=0,
    MaxButton,
    MinButton,
    CloseButton,
    MenuButton,
    OnAllDesktopsButton,
    AboveButton,
    BelowButton,
    ShadeButton,
    NumButtons
};

class KCommonDecorationButton;

class KCommonDecorationButtonPrivate;
class KCommonDecorationPrivate;

/**
 * This class eases development of decorations by implementing parts of KDecoration
 * which are error prone and common for most decorations.
 * It takes care of the window layout, button/action handling, and window mask creation.
 */
class KWIN_EXPORT KCommonDecoration : public KDecoration
{
    Q_OBJECT

    public:
        KCommonDecoration(KDecorationBridge* bridge, KDecorationFactory* factory);
        virtual ~KCommonDecoration();

        /**
         * Used to calculate the decoration layout. The basic layout looks like this:
         *
         * Window:
         *  _______________________________________________________________
         * |           LM_TitleEdgeTop                                     |
         * |_______________________________________________________________|
         * | LM_TitleEdgeLeft |   [title]              | LM_TitleEdgeRight |
         * |__________________|________________________|___________________|
         * |           LM_TitleEdgeBottom                                  |
         * |_______________________________________________________________|
         * | |                                                           | |
         * | |                                                           | |
         * | |                                                           | |
         * |LM_BorderLeft                                    LM_BorderRight|
         * |_|___________________________________________________________|_|
         * |           LM_BorderBottom                                     |
         * |_______________________________________________________________|
         *
         * Title:
         *  ___________________________________________________________________________________
         * | LM_ButtonMarginTop             |                |              LM_ButtonMarginTop |
         * |________________________________|                |_________________________________|
         * | [Buttons] | LM_TitleBorderLeft | LM_TitleHeight | LM_TitleBorderRight | [Buttons] |
         * |___________|____________________|________________|_____________________|___________|
         *
         * Buttons:
         *  _____________________________________________________________________________________________
         * | button | spacing | button | spacing | explicit spacer | spacing | ...    | spacing | button |
         * |________|_________|________|_________|_________________|_________|________|_________|________|
         *
         * @see layoutMetric()
         */
        enum LayoutMetric
        {
            LM_BorderLeft,
            LM_BorderRight,
            LM_BorderBottom,
            LM_TitleHeight,
            LM_TitleBorderLeft,
            LM_TitleBorderRight,
            LM_TitleEdgeLeft,
            LM_TitleEdgeRight,
            LM_TitleEdgeTop,
            LM_TitleEdgeBottom,
            LM_ButtonWidth,
            LM_ButtonHeight,
            LM_ButtonSpacing,
            LM_ExplicitButtonSpacer,
            LM_ButtonMarginTop
        };

        enum DecorationBehaviour
        {
            DB_MenuClose, ///< Close window on double clicking the menu
            DB_WindowMask, ///< Set a mask on the window
            DB_ButtonHide  ///< Hide buttons when there is not enough space in the titlebar
        };

        enum WindowCorner
        {
            WC_TopLeft,
            WC_TopRight,
            WC_BottomLeft,
            WC_BottomRight
        };

        /**
         * The name of the decoration used in the decoration preview.
         */
        virtual QString visibleName() const = 0;
        /**
         * The default title button order on the left.
         * @see KDecoration::titleButtonsLeft()
         * @see KDecoration::titleButtonsRight()
         */
        virtual QString defaultButtonsLeft() const = 0;
        /**
         * The default title button order on the left.
         * @see KDecoration::titleButtonsLeft()
         * @see KDecoration::titleButtonsRight()
         */
        virtual QString defaultButtonsRight() const = 0;

        /**
         * This controls whether some specific behaviour should be enabled or not.
         * @see DecorationBehaviour
         */
        virtual bool decorationBehaviour(DecorationBehaviour behaviour) const;

        /**
         * This controls the layout of the decoration in various ways. It is
         * possible to have a different layout for different window states.
         * @param lm                 The layout element.
         * @param respectWindowState Whether window states should be taken into account or a "default" state should be assumed.
         * @param button             For LM_ButtonWidth and LM_ButtonHeight, the button.
         */
        virtual int layoutMetric(LayoutMetric lm, bool respectWindowState = true, const KCommonDecorationButton *button = 0) const;

        /**
         * Create a new title bar button. KCommonDecoration takes care of memory management.
         * @return a pointer to the button, or 0 if the button should not be created.
         */
        virtual KCommonDecorationButton *createButton(ButtonType type) = 0;

        /**
         * @return the mask for the specific window corner.
         */
        virtual QRegion cornerShape(WindowCorner corner);

        /**
         * This updates the window mask using the information provided by
         * cornerShape(). Edges which are aligned to screen corners are not
         * shaped for better usability (remember to paint these areas in paintEvent(), too).
         * You normally don't want/need to reimplement updateWindowShape().
         * @see cornerShape()
         */
        virtual void updateWindowShape();

        /**
         * Draw the window decoration.
         */
        virtual void paintEvent(QPaintEvent *e) = 0;

        /**
         * This is used to update the painting of the title bar after the caption has been changed.
         * Reimplement for a more efficient implementation (default calls update() on the whole decoration).
         */
        virtual void updateCaption();

        int buttonsLeftWidth() const;
        int buttonsRightWidth() const;

        /**
         * TODO: remove?
         */
        void updateLayout() const;
        /**
         * Makes sure all buttons are repainted.
         */
        void updateButtons() const;
        /**
         * Manually call reset() on each button.
         */
        void resetButtons() const;

        /**
         * Convenience method.
         * @returns true if the window type is NET::Toolbar, NET::Utility, or NET::Menu
         */
        bool isToolWindow() const;
        /**
         * Convenience method.
         * @returns the title rect.
         */
         QRect titleRect() const;

    public:
        /**
         * Handles widget and layout creation, call the base implementation when subclassing this member.
         */
        virtual void init();
        /**
         * Handles SettingButtons, call the base implementation when subclassing this member.
         */
        virtual void reset( unsigned long changed );
        virtual void borders( int& left, int& right, int& top, int& bottom ) const;
        virtual void show();
        virtual void resize(const QSize&);
        virtual QSize minimumSize() const;
        virtual void maximizeChange();
        virtual void desktopChange();
        virtual void shadeChange();
        virtual void iconChange();
        virtual void activeChange();
        virtual void captionChange();
    public slots:
        void keepAboveChange(bool above);
        void keepBelowChange(bool below);
        void slotMaximize();
        void slotShade();
        void slotKeepAbove();
        void slotKeepBelow();
        void menuButtonPressed();
        void menuButtonReleased();
    public:
        virtual Position mousePosition(const QPoint &point) const;

        virtual bool eventFilter( QObject* o, QEvent* e );
        virtual void resizeEvent(QResizeEvent *e);
        virtual void mouseDoubleClickEvent(QMouseEvent *e);

    private:
        void resetLayout();

        void moveWidget(int x, int y, QWidget *w) const;
        void resizeWidget(int w, int h, QWidget *w) const;

        typedef QValueVector <KCommonDecorationButton*> ButtonContainer; ///< If the entry is 0, it's a spacer.
        int buttonContainerWidth(const ButtonContainer &btnContainer, bool countHidden = false) const;
        void addButtons(ButtonContainer &btnContainer, const QString& buttons, bool isLeft);
        void calcHiddenButtons();

        KCommonDecorationButton *m_button[NumButtons];

        ButtonContainer m_buttonsLeft;
        ButtonContainer m_buttonsRight;

        QWidget *m_previewWidget;

        int minBtnHideWidth;

        bool closing; // for menu doubleclick closing...

        KCommonDecorationPrivate *d;
};

/**
 * Title bar buttons of KCommonDecoration need to inherit this class.
 */
class KWIN_EXPORT KCommonDecorationButton : public QButton
{
    friend class KCommonDecoration;

    public:
        KCommonDecorationButton(ButtonType type, KCommonDecoration *parent, const char *name);
        virtual ~KCommonDecorationButton();

        /**
         * These flags specify what has changed, e.g. the reason for a reset().
         */
        enum
        {
            ManualReset     = 1 << 0, ///< The button might want to do a full reset for some reason...
            SizeChange      = 1 << 1, ///< The button size changed @see setSize()
            ToggleChange    = 1 << 2, ///< The button toggle state has changed @see setToggleButton()
            StateChange     = 1 << 3, ///< The button has been set pressed or not... @see setOn()
            IconChange      = 1 << 4, ///< The window icon has been changed
            DecorationReset = 1 << 5  ///< E.g. when decoration colors have changed
        };
        /**
         * Initialize the button after size change etc.
         */
        virtual void reset(unsigned long changed) = 0;
        /**
         * @returns the KCommonDecoration the button belongs to.
         */
        KCommonDecoration *decoration() const;
        /**
         * @returns the button type.
         * @see ButtonType
         */
        ButtonType type() const;

        /**
         * Whether the button is left of the titlebar or not.
         */
        bool isLeft() const;

        /**
         * Set which mouse buttons the button should honor. Used e.g. to prevent accidental right mouse clicks.
         */
        void setRealizeButtons(int btns);
        /**
         * Set the button size.
         */
        void setSize(const QSize &s);
        /**
         * Set/update the button's tool tip
         */
        void setTipText(const QString &tip);
        /**
         * The mouse button that has been clicked last time.
         */
        ButtonState lastMousePress() const { return m_lastMouse; }

        QSize sizeHint() const;

    protected:
        void setToggleButton(bool toggle);
        void setOn(bool on);
        void setLeft(bool left);
        void mousePressEvent(QMouseEvent *e);
        void mouseReleaseEvent(QMouseEvent *e);

    private:
        KCommonDecoration *m_decoration;
        ButtonType m_type;
        int m_realizeButtons;
        QSize m_size;
        ButtonState m_lastMouse;

        bool m_isLeft;

        KCommonDecorationButtonPrivate *d;
};

#endif // KCOMMONDECORATION_H
