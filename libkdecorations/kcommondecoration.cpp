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

#include "kcommondecoration.h"
#include "kcommondecoration_p.h"

#include <QApplication>
#include <QBasicTimer>
#include <QCursor>
#include <QDateTime>
#include <QLabel>
#include <QPointer>

#include <QWidget>

#include "kdecorationfactory.h"
#include <KDE/KLocalizedString>

#include "kcommondecoration.moc"

/** @addtogroup kdecoration */
/** @{ */

class KCommonDecorationPrivate
{
public:
    KCommonDecorationPrivate(KCommonDecoration *deco, KDecorationBridge *bridge, KDecorationFactory *factory);
    ~KCommonDecorationPrivate();
    KCommonDecoration *q;
    KCommonDecorationWrapper *wrapper; //    delete wrapper; - do not do this, this object is actually owned and deleted by the wrapper
    KCommonDecorationButton *button[NumButtons];
    KCommonDecoration::ButtonContainer buttonsLeft;
    KCommonDecoration::ButtonContainer buttonsRight;
    QPointer<QWidget> previewWidget;
    int btnHideMinWidth;
    int btnHideLastWidth;
    bool closing; // for menu doubleclick closing...
};

KCommonDecorationPrivate::KCommonDecorationPrivate(KCommonDecoration* deco, KDecorationBridge* bridge, KDecorationFactory* factory)
    : q(deco)
    , wrapper(new KCommonDecorationWrapper(deco, bridge, factory))
    , previewWidget()
    , btnHideMinWidth(200)
    , btnHideLastWidth(0)
    , closing(false)
{
    // sizeof(...) is calculated at compile time
    memset(button, 0, sizeof(KCommonDecorationButton *) * NumButtons);
}

KCommonDecorationPrivate::~KCommonDecorationPrivate()
{
    for (int n = 0; n < NumButtons; n++) {
        if (button[n]) delete button[n];
    }
}

KCommonDecoration::KCommonDecoration(KDecorationBridge* bridge, KDecorationFactory* factory)
    :   d(new KCommonDecorationPrivate(this, bridge, factory))

{
    connect(d->wrapper, SIGNAL(keepAboveChanged(bool)), this, SIGNAL(keepAboveChanged(bool)));
    connect(d->wrapper, SIGNAL(keepBelowChanged(bool)), this, SIGNAL(keepBelowChanged(bool)));
    connect(d->wrapper, &KDecoration::decorationButtonsChanged,
            this, &KCommonDecoration::decorationButtonsChanged);
    connect(d->wrapper, &KDecoration::decorationButtonsChanged,
            this, &KCommonDecoration::buttonsChanged);
    connect(d->wrapper, &KDecoration::activeChanged, this, &KCommonDecoration::activeChange);
    connect(d->wrapper, &KDecoration::captionChanged, this, &KCommonDecoration::captionChange);
    connect(d->wrapper, &KDecoration::desktopChanged, this, &KCommonDecoration::desktopChange);
    connect(d->wrapper, &KDecoration::shadeChanged, this, &KCommonDecoration::shadeChange);
    connect(d->wrapper, &KDecoration::iconChanged, this, &KCommonDecoration::iconChange);
    connect(d->wrapper, &KDecoration::maximizeChanged, this, &KCommonDecoration::maximizeChange);
}

KCommonDecoration::~KCommonDecoration()
{
}

bool KCommonDecoration::decorationBehaviour(DecorationBehaviour behaviour) const
{
    switch(behaviour) {
    case DB_MenuClose:
        return false;

    case DB_WindowMask:
        return false;

    case DB_ButtonHide:
        return true;
    }

    return false;
}

int KCommonDecoration::layoutMetric(LayoutMetric lm, bool respectWindowState, const KCommonDecorationButton *button) const
{
    switch(lm) {
    case LM_BorderLeft:
    case LM_BorderRight:
    case LM_BorderBottom:
    case LM_TitleEdgeTop:
    case LM_TitleEdgeBottom:
    case LM_TitleEdgeLeft:
    case LM_TitleEdgeRight:
    case LM_TitleBorderLeft:
    case LM_TitleBorderRight:
        return 5;


    case LM_ButtonWidth:
    case LM_ButtonHeight:
    case LM_TitleHeight:
        return 20;

    case LM_ButtonSpacing:
        return 5;

    case LM_ButtonMarginTop:
        return 0;

    case LM_ExplicitButtonSpacer:
        return layoutMetric(LM_ButtonWidth, respectWindowState, button) / 2; // half button width by default

    default:
        return 0;
    }
}

void KCommonDecoration::init()
{
    createMainWidget();

    // for flicker-free redraws
    widget()->setAttribute(Qt::WA_NoSystemBackground);

    widget()->installEventFilter(this);

    resetLayout();

    connect(this, SIGNAL(keepAboveChanged(bool)), SLOT(keepAboveChange(bool)));
    connect(this, SIGNAL(keepBelowChanged(bool)), SLOT(keepBelowChange(bool)));

    updateCaption();
}

void KCommonDecoration::buttonsChanged()
{
    resetLayout();
    widget()->update();
}

QRegion KCommonDecoration::cornerShape(WindowCorner)
{
    return QRegion();
}

void KCommonDecoration::updateCaption()
{
    // This should be reimplemented in decorations for better efficiency
    widget()->update();
}

void KCommonDecoration::borders(int& left, int& right, int& top, int& bottom) const
{
    left = layoutMetric(LM_BorderLeft);
    right = layoutMetric(LM_BorderRight);
    bottom = layoutMetric(LM_BorderBottom);
    top = layoutMetric(LM_TitleHeight) +
          layoutMetric(LM_TitleEdgeTop) +
          layoutMetric(LM_TitleEdgeBottom);

    updateLayout(); // TODO!! don't call every time we are in ::borders
}

void KCommonDecoration::updateLayout() const
{
    const int paddingLeft   = layoutMetric(LM_OuterPaddingLeft);
    const int paddingTop    = layoutMetric(LM_OuterPaddingTop);
    const int paddingRight  = layoutMetric(LM_OuterPaddingRight);
    const int paddingBottom = layoutMetric(LM_OuterPaddingBottom);

    QRect r = widget()->rect().adjusted(paddingLeft, paddingTop, -paddingRight, -paddingBottom);
    int r_x, r_y, r_x2, r_y2;
    r.getCoords(&r_x, &r_y, &r_x2, &r_y2);

    // layout preview widget
    if (d->previewWidget) {
        const int borderLeft = layoutMetric(LM_BorderLeft);
        const int borderRight = layoutMetric(LM_BorderRight);
        const int borderBottom = layoutMetric(LM_BorderBottom);
        const int titleHeight = layoutMetric(LM_TitleHeight);
        const int titleEdgeTop = layoutMetric(LM_TitleEdgeTop);
        const int titleEdgeBottom = layoutMetric(LM_TitleEdgeBottom);

        int left = r_x + borderLeft;
        int top = r_y + titleEdgeTop + titleHeight + titleEdgeBottom;
        int width = r_x2 - borderRight - left + 1;
        int height = r_y2 - borderBottom - top + 1;
        d->previewWidget->setGeometry(left, top, width, height);
        moveWidget(left, top, d->previewWidget.data());
        resizeWidget(width, height, d->previewWidget.data());
    }

    // resize buttons...
    for (int n = 0; n < NumButtons; n++) {
        if (d->button[n]) {
            QSize newSize = QSize(layoutMetric(LM_ButtonWidth, true, d->button[n]),
                                  layoutMetric(LM_ButtonHeight, true, d->button[n]));
            if (newSize != d->button[n]->size())
                d->button[n]->setSize(newSize);
        }
    }

    // layout buttons
    int y = r_y + layoutMetric(LM_TitleEdgeTop) + layoutMetric(LM_ButtonMarginTop);
    if (d->buttonsLeft.count() > 0) {
        const int buttonSpacing = layoutMetric(LM_ButtonSpacing);
        int x = r_x + layoutMetric(LM_TitleEdgeLeft);
        for (ButtonContainer::const_iterator it = d->buttonsLeft.constBegin(); it != d->buttonsLeft.constEnd(); ++it) {
            bool elementLayouted = false;
            if (*it) {
                if (!(*it)->isHidden()) {
                    moveWidget(x, y, *it);
                    x += layoutMetric(LM_ButtonWidth, true, qobject_cast<KCommonDecorationButton*>(*it));
                    elementLayouted = true;
                }
            } else {
                x += layoutMetric(LM_ExplicitButtonSpacer);
                elementLayouted = true;
            }
            if (elementLayouted && it != d->buttonsLeft.end())
                x += buttonSpacing;
        }
    }

    if (d->buttonsRight.count() > 0) {
        const int titleEdgeRightLeft = r_x2 - layoutMetric(LM_TitleEdgeRight) + 1;

        const int buttonSpacing = layoutMetric(LM_ButtonSpacing);
        int x = titleEdgeRightLeft - buttonContainerWidth(d->buttonsRight);
        for (ButtonContainer::const_iterator it = d->buttonsRight.constBegin(); it != d->buttonsRight.constEnd(); ++it) {
            bool elementLayouted = false;
            if (*it) {
                if (!(*it)->isHidden()) {
                    moveWidget(x, y, *it);
                    x += layoutMetric(LM_ButtonWidth, true, qobject_cast<KCommonDecorationButton*>(*it));;
                    elementLayouted = true;
                }
            } else {
                x += layoutMetric(LM_ExplicitButtonSpacer);
                elementLayouted = true;
            }
            if (elementLayouted && it != d->buttonsRight.end())
                x += buttonSpacing;
        }
    }
}

void KCommonDecoration::updateButtons() const
{
    for (int n = 0; n < NumButtons; n++)
        if (d->button[n]) d->button[n]->update();
}

void KCommonDecoration::resetButtons() const
{
    for (int n = 0; n < NumButtons; n++)
        if (d->button[n]) d->button[n]->reset(KCommonDecorationButton::ManualReset);
}

void KCommonDecoration::resetLayout()
{
    for (int n = 0; n < NumButtons; n++) {
        if (d->button[n]) {
            delete d->button[n];
            d->button[n] = nullptr;
        }
    }
    d->buttonsLeft.clear();
    d->buttonsRight.clear();

    delete d->previewWidget.data();
    d->previewWidget.clear();

    // shown instead of the window contents in decoration previews
    if (isPreview()) {
        d->previewWidget = QPointer<QWidget>(new QLabel(i18n("<center><b>%1</b></center>", visibleName()), widget()));
        d->previewWidget->setAutoFillBackground(true);
        d->previewWidget->show();
    }

    addButtons(d->buttonsLeft,
               options()->customButtonPositions() ? options()->titleButtonsLeft() : KDecorationOptions::defaultTitleButtonsLeft(),
               true);
    addButtons(d->buttonsRight,
               options()->customButtonPositions() ? options()->titleButtonsRight() : KDecorationOptions::defaultTitleButtonsRight(),
               false);

    updateLayout();

    const int minTitleBarWidth = 35;
    d->btnHideMinWidth = buttonContainerWidth(d->buttonsLeft, true) + buttonContainerWidth(d->buttonsRight, true) +
                      layoutMetric(LM_TitleEdgeLeft, false) + layoutMetric(LM_TitleEdgeRight, false) +
                      layoutMetric(LM_TitleBorderLeft, false) + layoutMetric(LM_TitleBorderRight, false) +
                      minTitleBarWidth;
    d->btnHideLastWidth = 0;
}

void KCommonDecoration::objDestroyed(QObject *obj)
{
    // make sure button deletion is reflected in the m_button[] array.
    // this makes sure that m_button[]-entries are not destroyed
    // twice, first in their KCommonDecorationButton/QObject
    // destructor (button objects are parented with the decroation
    // widget in KCommonDecoration constructor), and then in
    // ~KCommonDecoration.  a QPointer<KCommonDecorationButton> would
    // have been the better approach, but changing the button array
    // would have been ABI incompatible & would have required creation
    // of kcommondecorationprivate instance.
    // the same applies to .
    for (int n = 0; n < NumButtons; n++) {
        if (d->button[n] == obj) {
            d->button[n] = nullptr;
            break;
        }
    }
}

QRegion KCommonDecoration::region(KDecorationDefines::Region)
{
    return QRegion();
}

int KCommonDecoration::buttonsLeftWidth() const
{
    return buttonContainerWidth(d->buttonsLeft);
}

int KCommonDecoration::buttonsRightWidth() const
{
    return buttonContainerWidth(d->buttonsRight);
}

int KCommonDecoration::buttonContainerWidth(const ButtonContainer &btnContainer, bool countHidden) const
{
    int explicitSpacer = layoutMetric(LM_ExplicitButtonSpacer);

    int shownElementsCount = 0;

    int w = 0;
    for (ButtonContainer::const_iterator it = btnContainer.begin(); it != btnContainer.end(); ++it) {
        if (*it) {
            if (countHidden || !(*it)->isHidden()) {
                w += (*it)->width();
                ++shownElementsCount;
            }
        } else {
            w += explicitSpacer;
            ++shownElementsCount;
        }
    }
    w += layoutMetric(LM_ButtonSpacing) * (shownElementsCount - 1);

    return w;
}

void KCommonDecoration::addButtons(ButtonContainer &btnContainer, const QList<DecorationButton>& buttons, bool isLeft)
{
    if (buttons.isEmpty()) {
        return;
    }
    for (auto button : buttons) {
        KCommonDecorationButton *btn = nullptr;
        switch(button) {
        case DecorationButtonMenu:
            if (!d->button[MenuButton]) {
                btn = createButton(MenuButton);
                if (!btn) break;
                btn->setTipText(i18nc("Button showing window actions menu", "Window Menu"));
                btn->setRealizeButtons(Qt::LeftButton | Qt::RightButton);
                connect(btn, SIGNAL(pressed()), SLOT(menuButtonPressed()));
                connect(btn, SIGNAL(released()), this, SLOT(menuButtonReleased()));

                // fix double deletion, see objDestroyed()
                connect(btn, SIGNAL(destroyed(QObject*)), this, SLOT(objDestroyed(QObject*)));

                d->button[MenuButton] = btn;
            }
            break;
        case DecorationButtonApplicationMenu:
            if (!d->button[AppMenuButton]) {
                btn = createButton(AppMenuButton);
                if (!btn) break;
                btn->setTipText(i18nc("Button showing application menu", "Application Menu"));
                btn->setRealizeButtons(Qt::LeftButton);
                connect(btn, SIGNAL(clicked()), SLOT(appMenuButtonPressed()), Qt::QueuedConnection);
                // Application want to show it menu
                connect(decoration(), SIGNAL(showRequest()), this, SLOT(appMenuButtonPressed()), Qt::UniqueConnection);
                // Wait for menu to become available before displaying any button
                connect(decoration(), SIGNAL(appMenuAvailable()), this, SLOT(slotAppMenuAvailable()), Qt::UniqueConnection);
                // On Kded module shutdown, hide application menu button
                connect(decoration(), SIGNAL(appMenuUnavailable()), this, SLOT(slotAppMenuUnavailable()), Qt::UniqueConnection);
                // Application menu button may need to be modified on this signal
                connect(decoration(), SIGNAL(menuHidden()), btn, SLOT(slotAppMenuHidden()), Qt::UniqueConnection);

                // fix double deletion, see objDestroyed()
                connect(btn, SIGNAL(destroyed(QObject*)), this, SLOT(objDestroyed(QObject*)));
                d->button[AppMenuButton] = btn;
            }
            break;
        case DecorationButtonOnAllDesktops:
            if (!d->button[OnAllDesktopsButton]) {
                btn = createButton(OnAllDesktopsButton);
                if (!btn) break;
                const bool oad = isOnAllDesktops();
                btn->setTipText(oad ? i18n("Not on all desktops") : i18n("On all desktops"));
                btn->setToggleButton(true);
                btn->setOn(oad);
                connect(btn, SIGNAL(clicked()), SLOT(toggleOnAllDesktops()));

                // fix double deletion, see objDestroyed()
                connect(btn, SIGNAL(destroyed(QObject*)), this, SLOT(objDestroyed(QObject*)));

                d->button[OnAllDesktopsButton] = btn;
            }
            break;
        case DecorationButtonQuickHelp:
            if ((!d->button[HelpButton]) && providesContextHelp()) {
                btn = createButton(HelpButton);
                if (!btn) break;
                btn->setTipText(i18n("Help"));
                connect(btn, SIGNAL(clicked()), SLOT(showContextHelp()));

                // fix double deletion, see objDestroyed()
                connect(btn, SIGNAL(destroyed(QObject*)), this, SLOT(objDestroyed(QObject*)));

                d->button[HelpButton] = btn;
            }
            break;
        case DecorationButtonMinimize:
            if ((!d->button[MinButton]) && isMinimizable()) {
                btn = createButton(MinButton);
                if (!btn) break;
                btn->setTipText(i18n("Minimize"));
                connect(btn, SIGNAL(clicked()), SLOT(minimize()));

                // fix double deletion, see objDestroyed()
                connect(btn, SIGNAL(destroyed(QObject*)), this, SLOT(objDestroyed(QObject*)));

                d->button[MinButton] = btn;
            }
            break;
        case DecorationButtonMaximizeRestore:
            if ((!d->button[MaxButton]) && isMaximizable()) {
                btn = createButton(MaxButton);
                if (!btn) break;
                btn->setRealizeButtons(Qt::LeftButton | Qt::MidButton | Qt::RightButton);
                const bool max = maximizeMode() == MaximizeFull;
                btn->setTipText(max ? i18n("Restore") : i18n("Maximize"));
                btn->setToggleButton(true);
                btn->setOn(max);
                connect(btn, SIGNAL(clicked()), SLOT(slotMaximize()));

                // fix double deletion, see objDestroyed()
                connect(btn, SIGNAL(destroyed(QObject*)), this, SLOT(objDestroyed(QObject*)));

                d->button[MaxButton] = btn;
            }
            break;
        case DecorationButtonClose:
            if ((!d->button[CloseButton]) && isCloseable()) {
                btn = createButton(CloseButton);
                if (!btn) break;
                btn->setTipText(i18n("Close"));
                connect(btn, SIGNAL(clicked()), SLOT(closeWindow()));

                // fix double deletion, see objDestroyed()
                connect(btn, SIGNAL(destroyed(QObject*)), this, SLOT(objDestroyed(QObject*)));

                d->button[CloseButton] = btn;
            }
            break;
        case DecorationButtonKeepAbove:
            if (!d->button[AboveButton]) {
                btn = createButton(AboveButton);
                if (!btn) break;
                bool above = keepAbove();
                btn->setTipText(above ? i18n("Do not keep above others") : i18n("Keep above others"));
                btn->setToggleButton(true);
                btn->setOn(above);
                connect(btn, SIGNAL(clicked()), SLOT(slotKeepAbove()));

                // fix double deletion, see objDestroyed()
                connect(btn, SIGNAL(destroyed(QObject*)), this, SLOT(objDestroyed(QObject*)));

                d->button[AboveButton] = btn;
            }
            break;
        case DecorationButtonKeepBelow:
            if (!d->button[BelowButton]) {
                btn = createButton(BelowButton);
                if (!btn) break;
                bool below = keepBelow();
                btn->setTipText(below ? i18n("Do not keep below others") : i18n("Keep below others"));
                btn->setToggleButton(true);
                btn->setOn(below);
                connect(btn, SIGNAL(clicked()), SLOT(slotKeepBelow()));

                // fix double deletion, see objDestroyed()
                connect(btn, SIGNAL(destroyed(QObject*)), this, SLOT(objDestroyed(QObject*)));

                d->button[BelowButton] = btn;
            }
            break;
        case DecorationButtonShade:
            if ((!d->button[ShadeButton]) && isShadeable()) {
                btn = createButton(ShadeButton);
                if (!btn) break;
                bool shaded = isSetShade();
                btn->setTipText(shaded ? i18n("Unshade") : i18n("Shade"));
                btn->setToggleButton(true);
                btn->setOn(shaded);
                connect(btn, SIGNAL(clicked()), SLOT(slotShade()));

                // fix double deletion, see objDestroyed()
                connect(btn, SIGNAL(destroyed(QObject*)), this, SLOT(objDestroyed(QObject*)));

                d->button[ShadeButton] = btn;
            }
            break;
        case DecorationButtonExplicitSpacer:
            btnContainer.append(0);
        default:
            break;
        }


        if (btn) {
            btn->setLeft(isLeft);
            btn->setSize(QSize(layoutMetric(LM_ButtonWidth, true, btn), layoutMetric(LM_ButtonHeight, true, btn)));
            // will be shown later on window registration
            if (btn->type() == AppMenuButton && !isPreview() && !d->wrapper->menuAvailable()) {
                btn->hide();
            } else {
                btn->show();
            }

            btnContainer.append(btn);
        }

    }
}

void KCommonDecoration::calcHiddenButtons()
{
    if (width() == d->btnHideLastWidth)
        return;

    d->btnHideLastWidth = width();

    //Hide buttons in the following order:
    KCommonDecorationButton* btnArray[] = { d->button[HelpButton], d->button[AppMenuButton], d->button[ShadeButton], d->button[BelowButton],
                                            d->button[AboveButton], d->button[OnAllDesktopsButton], d->button[MaxButton],
                                            d->button[MinButton], d->button[MenuButton], d->button[CloseButton]
                                          };
    const int buttonsCount = sizeof(btnArray) / sizeof(btnArray[ 0 ]);

    int current_width = width();
    int count = 0;

    // Hide buttons
    while (current_width < d->btnHideMinWidth && count < buttonsCount) {
        if (btnArray[count]) {
            current_width += btnArray[count]->width();
            if (btnArray[count]->isVisible())
                btnArray[count]->hide();
        }
        count++;
    }
    // Show the rest of the buttons...
    for (int i = count; i < buttonsCount; i++) {
        if (btnArray[i]) {

            if (! btnArray[i]->isHidden())
                break; // all buttons shown...

            if (btnArray[i]->type() != AppMenuButton || d->wrapper->menuAvailable())
                btnArray[i]->show();
        }
    }
}

void KCommonDecoration::show()
{
    if (decorationBehaviour(DB_ButtonHide))
        calcHiddenButtons();
    widget()->show();
}

void KCommonDecoration::resize(const QSize& s)
{
    widget()->resize(s);
}

QSize KCommonDecoration::minimumSize() const
{
    const int minWidth = qMax(layoutMetric(LM_TitleEdgeLeft), layoutMetric(LM_BorderLeft))
                         + qMax(layoutMetric(LM_TitleEdgeRight), layoutMetric(LM_BorderRight))
                         + layoutMetric(LM_TitleBorderLeft) + layoutMetric(LM_TitleBorderRight);
    return QSize(minWidth,
                 layoutMetric(LM_TitleEdgeTop) + layoutMetric(LM_TitleHeight)
                 + layoutMetric(LM_TitleEdgeBottom)
                 + layoutMetric(LM_BorderBottom));
}

void KCommonDecoration::maximizeChange()
{
    if (d->button[MaxButton]) {
        d->button[MaxButton]->setOn(maximizeMode() == MaximizeFull);
        d->button[MaxButton]->setTipText((maximizeMode() != MaximizeFull) ?
                                        i18n("Maximize")
                                        : i18n("Restore"));
        d->button[MaxButton]->reset(KCommonDecorationButton::StateChange);
    }
    updateWindowShape();
    widget()->update();
}

void KCommonDecoration::desktopChange()
{
    if (d->button[OnAllDesktopsButton]) {
        d->button[OnAllDesktopsButton]->setOn(isOnAllDesktops());
        d->button[OnAllDesktopsButton]->setTipText(isOnAllDesktops() ?
                i18n("Not on all desktops")
                : i18n("On all desktops"));
        d->button[OnAllDesktopsButton]->reset(KCommonDecorationButton::StateChange);
    }
}

void KCommonDecoration::shadeChange()
{
    if (d->button[ShadeButton]) {
        bool shaded = isSetShade();
        d->button[ShadeButton]->setOn(shaded);
        d->button[ShadeButton]->setTipText(shaded ?
                                          i18n("Unshade")
                                          : i18n("Shade"));
        d->button[ShadeButton]->reset(KCommonDecorationButton::StateChange);
    }
}

void KCommonDecoration::iconChange()
{
    if (d->button[MenuButton]) {
        d->button[MenuButton]->update();
        d->button[MenuButton]->reset(KCommonDecorationButton::IconChange);
    }
}

void KCommonDecoration::activeChange()
{
    updateButtons();
    widget()->update(); // do something similar to updateCaption here
}

void KCommonDecoration::captionChange()
{
    updateCaption();
}

void KCommonDecoration::keepAboveChange(bool above)
{
    if (d->button[AboveButton]) {
        d->button[AboveButton]->setOn(above);
        d->button[AboveButton]->setTipText(above ? i18n("Do not keep above others") : i18n("Keep above others"));
        d->button[AboveButton]->reset(KCommonDecorationButton::StateChange);
    }

    if (d->button[BelowButton] && d->button[BelowButton]->isChecked()) {
        d->button[BelowButton]->setOn(false);
        d->button[BelowButton]->setTipText(i18n("Keep below others"));
        d->button[BelowButton]->reset(KCommonDecorationButton::StateChange);
    }
}

void KCommonDecoration::keepBelowChange(bool below)
{
    if (d->button[BelowButton]) {
        d->button[BelowButton]->setOn(below);
        d->button[BelowButton]->setTipText(below ? i18n("Do not keep below others") : i18n("Keep below others"));
        d->button[BelowButton]->reset(KCommonDecorationButton::StateChange);
    }

    if (d->button[AboveButton] && d->button[AboveButton]->isChecked()) {
        d->button[AboveButton]->setOn(false);
        d->button[AboveButton]->setTipText(i18n("Keep above others"));
        d->button[AboveButton]->reset(KCommonDecorationButton::StateChange);
    }
}

void KCommonDecoration::slotMaximize()
{
    if (d->button[MaxButton]) {
        maximize(d->button[MaxButton]->lastMousePress());
    }
}

void KCommonDecoration::slotShade()
{
    setShade(!isSetShade());
}

void KCommonDecoration::slotKeepAbove()
{
    setKeepAbove(!keepAbove());
}

void KCommonDecoration::slotKeepBelow()
{
    setKeepBelow(!keepBelow());
}

static QBasicTimer* timer = nullptr;
void KCommonDecoration::menuButtonPressed()
{
    if (decorationBehaviour(DB_MenuClose)) {
        if (timer == nullptr) {
            timer = new QBasicTimer();
        }
        if (!timer->isActive()) {
            timer->start(150, this);
        }
        // double click behavior
        static QTime* t = nullptr;
        static KCommonDecoration* lastClient = nullptr;
        if (t == nullptr) {
            t = new QTime;
        }
        if (lastClient == this && t->elapsed() <= QApplication::doubleClickInterval()) {
            d->closing = true;
        } else {
            lastClient = this;
            t->start();
        }
    } else {
        KDecorationFactory* f = factory();
        doShowWindowMenu();
        if (!f->exists(decoration()))   // 'this' was deleted
            return;
        d->button[MenuButton]->setDown(false);
    }
}

void KCommonDecoration::menuButtonReleased()
{
    if (d->closing) {
        if (timer && timer->isActive()) {
            timer->stop();
        }
        closeWindow();
    }
}

void KCommonDecoration::timerEvent(QTimerEvent *event)
{
    if (timer && event->timerId() == timer->timerId()) {
        timer->stop();
        if (d->closing || !d->button[MenuButton]->isDown()) {
            return;
        }
        d->closing = false;
        doShowWindowMenu();
        return;
    }
    QObject::timerEvent(event);
}

void KCommonDecoration::doShowWindowMenu()
{
    QRect menuRect = d->button[MenuButton]->rect();
    QPoint menutop = d->button[MenuButton]->mapToGlobal(menuRect.topLeft());
    QPoint menubottom = d->button[MenuButton]->mapToGlobal(menuRect.bottomRight()) + QPoint(0, 2);
    showWindowMenu(QRect(menutop, menubottom));
}


void KCommonDecoration::appMenuButtonPressed()
{
    QRect menuRect = d->button[AppMenuButton]->rect();
    d->wrapper->showApplicationMenu(d->button[AppMenuButton]->mapToGlobal(menuRect.bottomLeft()));

    KDecorationFactory* f = factory();
    if (!f->exists(decoration()))   // 'this' was deleted
        return;
    d->button[AppMenuButton]->setDown(false);
}

void KCommonDecoration::slotAppMenuAvailable()
{
    if (d->button[AppMenuButton]) {
        d->button[AppMenuButton]->show();
        updateLayout();
    }
}

void KCommonDecoration::slotAppMenuUnavailable()
{
    if (d->button[AppMenuButton]) {
        d->button[AppMenuButton]->hide();
        updateLayout();
    }
}

void KCommonDecoration::resizeEvent(QResizeEvent */*e*/)
{
    if (decorationBehaviour(DB_ButtonHide))
        calcHiddenButtons();

    updateLayout();

    updateWindowShape();
    // FIXME: don't update() here! this would result in two paintEvent()s
    // because there is already "something" else triggering the repaint...
//     widget()->update();
}

void KCommonDecoration::moveWidget(int x, int y, QWidget *widget) const
{
    QPoint p = widget->pos();
    int oldX = p.y();
    int oldY = p.x();

    if (x != oldX || y != oldY)
        widget->move(x, y);
}

void KCommonDecoration::resizeWidget(int w, int h, QWidget *widget) const
{
    QSize s = widget->size();
    int oldW = s.width();
    int oldH = s.height();

    if (w != oldW || h != oldH)
        widget->resize(w, h);
}

void KCommonDecoration::mouseDoubleClickEvent(QMouseEvent *e)
{
    if (e->button() != Qt::LeftButton)
        return;

    int tb = layoutMetric(LM_TitleEdgeTop) + layoutMetric(LM_TitleHeight) +
             layoutMetric(LM_TitleEdgeBottom) + layoutMetric(LM_OuterPaddingTop);
    // when shaded, react on double clicks everywhere to make it easier to unshade. otherwise
    // react only on double clicks in the title bar region...
    if (isSetShade() || e->pos().y() <= tb)
        titlebarDblClickOperation();
}

void KCommonDecoration::wheelEvent(QWheelEvent *e)
{
    int tb = layoutMetric(LM_TitleEdgeTop) + layoutMetric(LM_TitleHeight) +
             layoutMetric(LM_TitleEdgeBottom) + layoutMetric(LM_OuterPaddingTop);
    if (isSetShade() || e->pos().y() <= tb)
        titlebarMouseWheelOperation(e->delta());
}

KCommonDecoration::Position KCommonDecoration::mousePosition(const QPoint &point) const
{
    const int corner = 18 + 3 * layoutMetric(LM_BorderBottom, false) / 2;
    Position pos = PositionCenter;

    const int paddingLeft   = layoutMetric(LM_OuterPaddingLeft);
    const int paddingTop    = layoutMetric(LM_OuterPaddingTop);
    const int paddingRight  = layoutMetric(LM_OuterPaddingRight);
    const int paddingBottom = layoutMetric(LM_OuterPaddingBottom);

    QRect r = widget()->rect().adjusted(paddingLeft, paddingTop, -paddingRight, -paddingBottom);
    int r_x, r_y, r_x2, r_y2;
    r.getCoords(&r_x, &r_y, &r_x2, &r_y2);
    int p_x = point.x();
    int p_y = point.y();
    const int borderLeft = layoutMetric(LM_BorderLeft);
//     const int borderRight = layoutMetric(LM_BorderRight);
    const int borderBottom = layoutMetric(LM_BorderBottom);
    const int titleHeight = layoutMetric(LM_TitleHeight);
    const int titleEdgeTop = layoutMetric(LM_TitleEdgeTop);
    const int titleEdgeBottom = layoutMetric(LM_TitleEdgeBottom);
    const int titleEdgeLeft = layoutMetric(LM_TitleEdgeLeft);
    const int titleEdgeRight = layoutMetric(LM_TitleEdgeRight);

    const int borderBottomTop = r_y2 - borderBottom + 1;
    const int borderLeftRight = r_x + borderLeft - 1;
//     const int borderRightLeft = r_x2-borderRight+1;
    const int titleEdgeLeftRight = r_x + titleEdgeLeft - 1;
    const int titleEdgeRightLeft = r_x2 - titleEdgeRight + 1;
    const int titleEdgeBottomBottom = r_y + titleEdgeTop + titleHeight + titleEdgeBottom - 1;
    const int titleEdgeTopBottom = r_y + titleEdgeTop - 1;

    if (p_y <= titleEdgeTopBottom) {
        if (p_x <= r_x + corner)
            pos = PositionTopLeft;
        else if (p_x >= r_x2 - corner)
            pos = PositionTopRight;
        else
            pos = PositionTop;
    } else if (p_y <= titleEdgeBottomBottom) {
        if (p_x <= titleEdgeLeftRight)
            pos = PositionTopLeft;
        else if (p_x >= titleEdgeRightLeft)
            pos = PositionTopRight;
        else
            pos = PositionCenter; // title bar
    } else if (p_y < borderBottomTop) {
        if (p_y < r_y2 - corner) {
            if (p_x <= borderLeftRight)
                pos = PositionLeft;
            else
                pos = PositionRight;
        } else {
            if (p_x <= borderLeftRight)
                pos = PositionBottomLeft;
            else
                pos = PositionBottomRight;
        }
    } else if (p_y >= borderBottomTop) {
        if (p_x <= r_x + corner)
            pos = PositionBottomLeft;
        else if (p_x >= r_x2 - corner)
            pos = PositionBottomRight;
        else
            pos = PositionBottom;
    }

    return pos;
}

void KCommonDecoration::updateWindowShape()
{
    // don't mask the widget...
    if (!decorationBehaviour(DB_WindowMask))
        return;

    int w = widget()->width();
    int h = widget()->height();

    QRegion mask(0, 0, w, h);

    // Remove corners (if subclass wants to.)
    mask -= cornerShape(WC_TopLeft);
    mask -= cornerShape(WC_TopRight);
    mask -= cornerShape(WC_BottomLeft);
    mask -= cornerShape(WC_BottomRight);

    setMask(mask);
}

bool KCommonDecoration::eventFilter(QObject* o, QEvent* e)
{
    if (o != widget())
        return false;
    switch(e->type()) {
    case QEvent::Resize:
        resizeEvent(static_cast<QResizeEvent*>(e));
        return true;
    case QEvent::Paint:
        paintEvent(static_cast< QPaintEvent* >(e));
        return true;
    case QEvent::MouseButtonDblClick:
        mouseDoubleClickEvent(static_cast< QMouseEvent* >(e));
        return true;
    case QEvent::MouseButtonPress:
        processMousePressEvent(static_cast< QMouseEvent* >(e));
        return true;
    case QEvent::Wheel:
        wheelEvent(static_cast< QWheelEvent* >(e));
        return true;
    default:
        return false;
    }
}

const int SUPPORTED_WINDOW_TYPES_MASK = NET::NormalMask | NET::DesktopMask | NET::DockMask
                                        | NET::ToolbarMask | NET::MenuMask | NET::DialogMask /*| NET::OverrideMask*/ | NET::TopMenuMask
                                        | NET::UtilityMask | NET::SplashMask;

bool KCommonDecoration::isToolWindow() const
{
    NET::WindowType type = windowType(SUPPORTED_WINDOW_TYPES_MASK);
    return ((type == NET::Toolbar) || (type == NET::Utility) || (type == NET::Menu));
}

QRect KCommonDecoration::titleRect() const
{
    const int pl = layoutMetric(LM_OuterPaddingLeft);
    const int pt = layoutMetric(LM_OuterPaddingTop);
    const int pr = layoutMetric(LM_OuterPaddingRight);
    const int pb = layoutMetric(LM_OuterPaddingBottom);

    int r_x, r_y, r_x2, r_y2;
    widget()->rect().adjusted(pl, pt, -pr, -pb).getCoords(&r_x, &r_y, &r_x2, &r_y2);

    const int titleEdgeLeft = layoutMetric(LM_TitleEdgeLeft);
    const int titleEdgeTop = layoutMetric(LM_TitleEdgeTop);
    const int titleEdgeRight = layoutMetric(LM_TitleEdgeRight);
    const int titleEdgeBottom = layoutMetric(LM_TitleEdgeBottom);
    const int titleBorderLeft = layoutMetric(LM_TitleBorderLeft);
    const int titleBorderRight = layoutMetric(LM_TitleBorderRight);
    const int ttlHeight = layoutMetric(LM_TitleHeight);
    const int titleEdgeBottomBottom = r_y + titleEdgeTop + ttlHeight + titleEdgeBottom - 1;
    return QRect(r_x + titleEdgeLeft + buttonsLeftWidth() + titleBorderLeft, r_y + titleEdgeTop,
                 r_x2 - titleEdgeRight - buttonsRightWidth() - titleBorderRight - (r_x + titleEdgeLeft + buttonsLeftWidth() + titleBorderLeft),
                 titleEdgeBottomBottom - (r_y + titleEdgeTop));
}

QRect KCommonDecoration::transparentRect() const
{
    return d->wrapper->transparentRect();
}

void KCommonDecoration::update()
{
    d->wrapper->update();
}

void KCommonDecoration::update(const QRect &rect)
{
    d->wrapper->update(rect);
}

void KCommonDecoration::update(const QRegion &region)
{
    d->wrapper->update(region);
}

QPalette KCommonDecoration::palette() const
{
    return d->wrapper->palette();
}

class KCommonDecorationButtonPrivate
{
public:
    KCommonDecorationButtonPrivate(ButtonType t, KCommonDecoration *deco);
    KCommonDecoration *decoration;
    ButtonType type;
    int realizeButtons;
    QSize size;
    Qt::MouseButtons lastMouse;
    bool isLeft;
};

KCommonDecorationButtonPrivate::KCommonDecorationButtonPrivate(ButtonType t, KCommonDecoration* deco)
    : decoration(deco)
    , type(t)
    , realizeButtons(Qt::LeftButton)
    , lastMouse(Qt::NoButton)
    , isLeft(true)
{
}

KCommonDecorationButton::KCommonDecorationButton(ButtonType type, KCommonDecoration *parent)
    : QAbstractButton(parent ? parent->widget() : nullptr),
      d(new KCommonDecorationButtonPrivate(type, parent))
{
    setCursor(Qt::ArrowCursor);
}

KCommonDecorationButton::~KCommonDecorationButton()
{
}

KCommonDecoration *KCommonDecorationButton::decoration() const
{
    return d->decoration;
}

ButtonType KCommonDecorationButton::type() const
{
    return d->type;
}

bool KCommonDecorationButton::isLeft() const
{
    return d->isLeft;
}

void KCommonDecorationButton::setLeft(bool left)
{
    d->isLeft = left;
}

void KCommonDecorationButton::setRealizeButtons(int btns)
{
    d->realizeButtons = btns;
}

void KCommonDecorationButton::setSize(const QSize &s)
{
    if (!d->size.isValid() || s != size()) {
        d->size = s;

        setFixedSize(d->size);
        reset(SizeChange);
    }
}

QSize KCommonDecorationButton::sizeHint() const
{
    return d->size;
}

void KCommonDecorationButton::setTipText(const QString &tip)
{
    this->setToolTip(tip);
}

void KCommonDecorationButton::setToggleButton(bool toggle)
{
    QAbstractButton::setCheckable(toggle);
    reset(ToggleChange);
}

void KCommonDecorationButton::setOn(bool on)
{
    if (on != isChecked()) {
        QAbstractButton::setChecked(on);
        reset(StateChange);
    }
}

void KCommonDecorationButton::mousePressEvent(QMouseEvent* e)
{
    d->lastMouse = e->button();
    // pass on event after changing button to LeftButton
    QMouseEvent me(e->type(), e->pos(), (e->button()&d->realizeButtons) ? Qt::LeftButton : Qt::NoButton, e->buttons(), e->modifiers());

    QAbstractButton::mousePressEvent(&me);
}

void KCommonDecorationButton::mouseReleaseEvent(QMouseEvent* e)
{
    d->lastMouse = e->button();
    // pass on event after changing button to LeftButton
    QMouseEvent me(e->type(), e->pos(), (e->button()&d->realizeButtons) ? Qt::LeftButton : Qt::NoButton, e->buttons(), e->modifiers());

    QAbstractButton::mouseReleaseEvent(&me);
}



// *** wrap everything from KDecoration *** //
bool KCommonDecoration::drawbound(const QRect&, bool)
{
    return false;
}
bool KCommonDecoration::windowDocked(Position)
{
    return false;
}
const KDecorationOptions* KCommonDecoration::options()
{
    return KDecoration::options();
}
bool KCommonDecoration::isActive() const
{
    return d->wrapper->isActive();
}
bool KCommonDecoration::isCloseable() const
{
    return d->wrapper->isCloseable();
}
bool KCommonDecoration::isMaximizable() const
{
    return d->wrapper->isMaximizable();
}
KCommonDecoration::MaximizeMode KCommonDecoration::maximizeMode() const
{
    return d->wrapper->maximizeMode();
}
bool KCommonDecoration::isMinimizable() const
{
    return d->wrapper->isMinimizable();
}
bool KCommonDecoration::providesContextHelp() const
{
    return d->wrapper->providesContextHelp();
}
int KCommonDecoration::desktop() const
{
    return d->wrapper->desktop();
}
bool KCommonDecoration::isOnAllDesktops() const
{
    return d->wrapper->isOnAllDesktops();
}
bool KCommonDecoration::isModal() const
{
    return d->wrapper->isModal();
}
bool KCommonDecoration::isShadeable() const
{
    return d->wrapper->isShadeable();
}
bool KCommonDecoration::isShade() const
{
    return d->wrapper->isShade();
}
bool KCommonDecoration::isSetShade() const
{
    return d->wrapper->isSetShade();
}
bool KCommonDecoration::keepAbove() const
{
    return d->wrapper->keepAbove();
}
bool KCommonDecoration::keepBelow() const
{
    return d->wrapper->keepBelow();
}
bool KCommonDecoration::isMovable() const
{
    return d->wrapper->isMovable();
}
bool KCommonDecoration::isResizable() const
{
    return d->wrapper->isResizable();
}
NET::WindowType KCommonDecoration::windowType(unsigned long supported_types) const
{
    return d->wrapper->windowType(supported_types);
}
QIcon KCommonDecoration::icon() const
{
    return d->wrapper->icon();
}
QString KCommonDecoration::caption() const
{
    return d->wrapper->caption();
}
void KCommonDecoration::showWindowMenu(const QRect &pos)
{
    return d->wrapper->showWindowMenu(pos);
}
void KCommonDecoration::showWindowMenu(QPoint pos)
{
    return d->wrapper->showWindowMenu(pos);
}
void KCommonDecoration::performWindowOperation(WindowOperation op)
{
    return d->wrapper->performWindowOperation(op);
}
void KCommonDecoration::setMask(const QRegion& reg, int mode)
{
    return d->wrapper->setMask(reg, mode);
}
void KCommonDecoration::clearMask()
{
    return d->wrapper->clearMask();
}
bool KCommonDecoration::isPreview() const
{
    return d->wrapper->isPreview();
}
QRect KCommonDecoration::geometry() const
{
    return d->wrapper->geometry();
}
QRect KCommonDecoration::iconGeometry() const
{
    return d->wrapper->iconGeometry();
}
QRegion KCommonDecoration::unobscuredRegion(const QRegion& r) const
{
    return d->wrapper->unobscuredRegion(r);
}
WId KCommonDecoration::windowId() const
{
    return d->wrapper->windowId();
}
int KCommonDecoration::width() const
{
    return d->wrapper->width();
}
int KCommonDecoration::height() const
{
    return d->wrapper->height();
}
void KCommonDecoration::processMousePressEvent(QMouseEvent* e)
{
    return d->wrapper->processMousePressEvent(e);
}
void KCommonDecoration::setMainWidget(QWidget* w)
{
    return d->wrapper->setMainWidget(w);
}
void KCommonDecoration::createMainWidget(Qt::WindowFlags flags)
{
    return d->wrapper->createMainWidget(flags);
}
Qt::WindowFlags KCommonDecoration::initialWFlags() const
{
    return d->wrapper->initialWFlags();
}
QWidget* KCommonDecoration::widget()
{
    return d->wrapper->widget();
}
const QWidget* KCommonDecoration::widget() const
{
    return d->wrapper->widget();
}
KDecorationFactory* KCommonDecoration::factory() const
{
    return d->wrapper->factory();
}
void KCommonDecoration::grabXServer()
{
    return d->wrapper->grabXServer();
}
void KCommonDecoration::ungrabXServer()
{
    return d->wrapper->ungrabXServer();
}
void KCommonDecoration::closeWindow()
{
    return d->wrapper->closeWindow();
}
void KCommonDecoration::maximize(Qt::MouseButtons button)
{
    return d->wrapper->maximize(button);
}
void KCommonDecoration::maximize(MaximizeMode mode)
{
    return d->wrapper->maximize(mode);
}
void KCommonDecoration::minimize()
{
    return d->wrapper->minimize();
}
void KCommonDecoration::showContextHelp()
{
    return d->wrapper->showContextHelp();
}
void KCommonDecoration::setDesktop(int desktop)
{
    return d->wrapper->setDesktop(desktop);
}
void KCommonDecoration::toggleOnAllDesktops()
{
    return d->wrapper->toggleOnAllDesktops();
}
void KCommonDecoration::titlebarDblClickOperation()
{
    return d->wrapper->titlebarDblClickOperation();
}
void KCommonDecoration::titlebarMouseWheelOperation(int delta)
{
    return d->wrapper->titlebarMouseWheelOperation(delta);
}
void KCommonDecoration::setShade(bool set)
{
    return d->wrapper->setShade(set);
}
void KCommonDecoration::setKeepAbove(bool set)
{
    return d->wrapper->setKeepAbove(set);
}
void KCommonDecoration::setKeepBelow(bool set)
{
    return d->wrapper->setKeepBelow(set);
}
void KCommonDecoration::setAlphaEnabled(bool enabled)
{
    d->wrapper->wrapSetAlphaEnabled(enabled);
}
// *** end of wrapping of everything from KDecoration *** //

const KDecoration* KCommonDecoration::decoration() const
{
    return d->wrapper;
}
KDecoration* KCommonDecoration::decoration()
{
    return d->wrapper;
}

// All copied from kdecoration.cpp
bool KCommonDecoration::compositingActive() const
{
    return decoration()->compositingActive();
}

// Window tabbing

int KCommonDecoration::tabCount() const
{
    return decoration()->tabCount();
}

QString KCommonDecoration::caption(int idx) const
{
    return decoration()->caption(idx);
}

QIcon KCommonDecoration::icon(int idx) const
{
    return decoration()->icon(idx);
}

long KCommonDecoration::tabId(int idx) const
{
    return decoration()->tabId(idx);
}

long KCommonDecoration::currentTabId() const
{
    return decoration()->currentTabId();
}

void KCommonDecoration::setCurrentTab(long id)
{
    decoration()->setCurrentTab(id);
}

void KCommonDecoration::tab_A_before_B(long A, long B)
{
    decoration()->tab_A_before_B(A, B);
}

void KCommonDecoration::tab_A_behind_B(long A, long B)
{
    decoration()->tab_A_behind_B(A, B);
}

void KCommonDecoration::untab(long id, const QRect& newGeom)
{
    decoration()->untab(id, newGeom);
}

void KCommonDecoration::closeTab(long id)
{
    decoration()->closeTab(id);
}

void KCommonDecoration::closeTabGroup()
{
    decoration()->closeTabGroup();
}

void KCommonDecoration::showWindowMenu(const QPoint &pos, long id)
{
    decoration()->showWindowMenu(pos, id);
}

KDecoration::WindowOperation KCommonDecoration::buttonToWindowOperation(Qt::MouseButtons button)
{
    return decoration()->buttonToWindowOperation(button);
}

Qt::MouseButtons KCommonDecorationButton::lastMousePress() const
{
    return d->lastMouse;
}

// kate: space-indent on; indent-width 4; mixedindent off; indent-mode cstyle;

