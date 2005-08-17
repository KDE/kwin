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

#include <qapplication.h>
#include <qcursor.h>
#include <qdatetime.h>
#include <qlabel.h>
#include <qtooltip.h>
#include <qwidget.h>

#include <kdebug.h>

#include <kapplication.h>
#include <kdecorationfactory.h>
#include <klocale.h>
#include <QDesktopWidget>

#include "kcommondecoration.h"
#include "kcommondecoration.moc"

KCommonDecoration::KCommonDecoration(KDecorationBridge* bridge, KDecorationFactory* factory)
    : KDecoration (bridge, factory),
        m_previewWidget(0),
        btnHideMinWidth(200),
        btnHideLastWidth(0),
        closing(false)
{
    // sizeof(...) is calculated at compile time
    memset(m_button, 0, sizeof(KCommonDecorationButton *) * NumButtons);
}

KCommonDecoration::~KCommonDecoration()
{
    for (int n=0; n<NumButtons; n++) {
        if (m_button[n]) delete m_button[n];
    }
    delete m_previewWidget;
}

bool KCommonDecoration::decorationBehaviour(DecorationBehaviour behaviour) const
{
    switch (behaviour) {
        case DB_MenuClose:
            return false;

        case DB_WindowMask:
            return false;

        case DB_ButtonHide:
            return true;
    }

    return false;
}

int KCommonDecoration::layoutMetric(LayoutMetric lm, bool, const KCommonDecorationButton *) const
{
    switch (lm) {
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
            return 5;

        default:
            return 0;
    }
}

void KCommonDecoration::init()
{
    createMainWidget();

    // for flicker-free redraws
    widget()->setBackgroundMode(Qt::NoBackground);

    widget()->installEventFilter( this );

    resetLayout();

    connect(this, SIGNAL(keepAboveChanged(bool) ), SLOT(keepAboveChange(bool) ) );
    connect(this, SIGNAL(keepBelowChanged(bool) ), SLOT(keepBelowChange(bool) ) );

    updateCaption();
}

void KCommonDecoration::reset( unsigned long changed )
{
    if (changed & SettingButtons) {
        resetLayout();
        widget()->update();
    }
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

void KCommonDecoration::borders( int& left, int& right, int& top, int& bottom ) const
{
    left = layoutMetric(LM_BorderLeft);
    right = layoutMetric(LM_BorderRight);
    bottom = layoutMetric(LM_BorderBottom);
    top = layoutMetric(LM_TitleHeight) +
            layoutMetric(LM_TitleEdgeTop) +
            layoutMetric(LM_TitleEdgeBottom);

    updateLayout(); // TODO!! don't call everytime we are in ::borders
}

void KCommonDecoration::updateLayout() const
{
    QRect r = widget()->rect();
    int r_x, r_y, r_x2, r_y2;
    r.coords(&r_x, &r_y, &r_x2, &r_y2);

    // layout preview widget
    if (m_previewWidget) {
        const int borderLeft = layoutMetric(LM_BorderLeft);
        const int borderRight = layoutMetric(LM_BorderRight);
        const int borderBottom = layoutMetric(LM_BorderBottom);
        const int titleHeight = layoutMetric(LM_TitleHeight);
        const int titleEdgeTop = layoutMetric(LM_TitleEdgeTop);
        const int titleEdgeBottom = layoutMetric(LM_TitleEdgeBottom);

        int left = r_x+borderLeft;
        int top = r_y+titleEdgeTop+titleHeight+titleEdgeBottom;
        int width = r_x2-borderRight-left+1;
        int height = r_y2-borderBottom-top+1;
        m_previewWidget->setGeometry(left, top, width, height);
        moveWidget(left,top, m_previewWidget);
        resizeWidget(width, height, m_previewWidget);
    }

    // resize buttons...
    for (int n=0; n<NumButtons; n++) {
        if (m_button[n]) {
            QSize newSize = QSize(layoutMetric(LM_ButtonWidth, true, m_button[n]),
                                  layoutMetric(LM_ButtonHeight, true, m_button[n]) );
            if (newSize != m_button[n]->size() )
                m_button[n]->setSize(newSize);
        }
    }

    // layout buttons
    int y = r_y + layoutMetric(LM_TitleEdgeTop) + layoutMetric(LM_ButtonMarginTop);
    if (m_buttonsLeft.count() > 0) {
        const int buttonSpacing = layoutMetric(LM_ButtonSpacing);
        int x = r_x + layoutMetric(LM_TitleEdgeLeft);
        for (ButtonContainer::const_iterator it = m_buttonsLeft.begin(); it != m_buttonsLeft.end(); ++it) {
            bool elementLayouted = false;
            if (*it) {
                if (!(*it)->isHidden() ) {
                    moveWidget(x,y, *it);
                    x += layoutMetric(LM_ButtonWidth, true, qobject_cast<KCommonDecorationButton*>(*it) );
                    elementLayouted = true;
                }
            } else {
                x+= layoutMetric(LM_ExplicitButtonSpacer);
                elementLayouted = true;
            }
            if (elementLayouted && it != m_buttonsLeft.end() )
                x += buttonSpacing;
        }
    }

    if (m_buttonsRight.count() > 0) {
        const int titleEdgeRightLeft = r_x2-layoutMetric(LM_TitleEdgeRight)+1;

        const int buttonSpacing = layoutMetric(LM_ButtonSpacing);
        int x = titleEdgeRightLeft - buttonContainerWidth(m_buttonsRight);
        for (ButtonContainer::const_iterator it = m_buttonsRight.begin(); it != m_buttonsRight.end(); ++it) {
            bool elementLayouted = false;
            if (*it) {
                if (!(*it)->isHidden() ) {
                    moveWidget(x,y, *it);
                    x += layoutMetric(LM_ButtonWidth, true, qobject_cast<KCommonDecorationButton*>(*it) );;
                    elementLayouted = true;
                }
            } else {
                x += layoutMetric(LM_ExplicitButtonSpacer);
                elementLayouted = true;
            }
            if (elementLayouted && it != m_buttonsRight.end() )
                x += buttonSpacing;
        }
    }
}

void KCommonDecoration::updateButtons() const
{
    for (int n=0; n<NumButtons; n++)
        if (m_button[n]) m_button[n]->update();
}

void KCommonDecoration::resetButtons() const
{
    for (int n=0; n<NumButtons; n++)
        if (m_button[n]) m_button[n]->reset(KCommonDecorationButton::ManualReset);
}

void KCommonDecoration::resetLayout()
{
    for (int n=0; n<NumButtons; n++) {
        if (m_button[n]) {
            delete m_button[n];
            m_button[n] = 0;
        }
    }
    m_buttonsLeft.clear();
    m_buttonsRight.clear();

    delete m_previewWidget;
    m_previewWidget = 0;

    // shown instead of the window contents in decoration previews
    if(isPreview() ) {
        m_previewWidget = new QLabel(i18n("<center><b>%1 preview</b></center>").arg(visibleName() ), widget());
        m_previewWidget->show();
    }

    addButtons(m_buttonsLeft,
               options()->customButtonPositions() ? options()->titleButtonsLeft() : defaultButtonsLeft(),
               true);
    addButtons(m_buttonsRight,
               options()->customButtonPositions() ? options()->titleButtonsRight() : defaultButtonsRight(),
               false);

    updateLayout();

    const int minTitleBarWidth = 35;
    btnHideMinWidth = buttonContainerWidth(m_buttonsLeft,true) + buttonContainerWidth(m_buttonsRight,true) +
            layoutMetric(LM_TitleEdgeLeft,false) + layoutMetric(LM_TitleEdgeRight,false) +
            layoutMetric(LM_TitleBorderLeft,false) + layoutMetric(LM_TitleBorderRight,false) +
            minTitleBarWidth;
    btnHideLastWidth = 0;
}

int KCommonDecoration::buttonsLeftWidth() const
{
    return buttonContainerWidth(m_buttonsLeft);
}

int KCommonDecoration::buttonsRightWidth() const
{
    return buttonContainerWidth(m_buttonsRight);
}

int KCommonDecoration::buttonContainerWidth(const ButtonContainer &btnContainer, bool countHidden) const
{
    int explicitSpacer = layoutMetric(LM_ExplicitButtonSpacer);

    int shownElementsCount = 0;

    int w = 0;
    for (ButtonContainer::const_iterator it = btnContainer.begin(); it != btnContainer.end(); ++it) {
        if (*it) {
            if (countHidden || !(*it)->isHidden() ) {
                w += (*it)->width();
                ++shownElementsCount;
            }
        } else {
            w += explicitSpacer;
            ++shownElementsCount;
        }
    }
    w += layoutMetric(LM_ButtonSpacing)*(shownElementsCount-1);

    return w;
}

void KCommonDecoration::addButtons(ButtonContainer &btnContainer, const QString& s, bool isLeft)
{
    if (s.length() > 0) {
        for (int n=0; n < s.length(); n++) {
            KCommonDecorationButton *btn = 0;
            switch (s[n].toAscii()) {
              case 'M': // Menu button
                  if (!m_button[MenuButton]){
                      btn = createButton(MenuButton);
                      if (!btn) break;
                      btn->setTipText(i18n("Menu") );
                      btn->setRealizeButtons(Qt::LeftButton|Qt::RightButton);
                      connect(btn, SIGNAL(pressed()), SLOT(menuButtonPressed()));
                      connect(btn, SIGNAL(released()), this, SLOT(menuButtonReleased()));

                      m_button[MenuButton] = btn;
                  }
                  break;
              case 'S': // OnAllDesktops button
                  if (!m_button[OnAllDesktopsButton]){
                      btn = createButton(OnAllDesktopsButton);
                      if (!btn) break;
                      const bool oad = isOnAllDesktops();
                      btn->setTipText(oad?i18n("Not on all desktops"):i18n("On all desktops") );
                      btn->setToggleButton(true);
                      btn->setOn( oad );
                      connect(btn, SIGNAL(clicked()), SLOT(toggleOnAllDesktops()));

                      m_button[OnAllDesktopsButton] = btn;
                  }
                  break;
              case 'H': // Help button
                  if ((!m_button[HelpButton]) && providesContextHelp()){
                      btn = createButton(HelpButton);
                      if (!btn) break;
                      btn->setTipText(i18n("Help") );
                      connect(btn, SIGNAL(clicked()), SLOT(showContextHelp()));

                      m_button[HelpButton] = btn;
                  }
                  break;
              case 'I': // Minimize button
                  if ((!m_button[MinButton]) && isMinimizable()){
                      btn = createButton(MinButton);
                      if (!btn) break;
                      btn->setTipText(i18n("Minimize") );
                      connect(btn, SIGNAL(clicked()), SLOT(minimize()));

                      m_button[MinButton] = btn;
                  }
                  break;
              case 'A': // Maximize button
                  if ((!m_button[MaxButton]) && isMaximizable()){
                      btn = createButton(MaxButton);
                      if (!btn) break;
                      btn->setRealizeButtons(Qt::LeftButton|Qt::MidButton|Qt::RightButton);
                      const bool max = maximizeMode()==MaximizeFull;
                      btn->setTipText(max?i18n("Restore"):i18n("Maximize") );
                      btn->setToggleButton(true);
                      btn->setOn( max );
                      connect(btn, SIGNAL(clicked()), SLOT(slotMaximize()));

                      m_button[MaxButton] = btn;
                  }
                  break;
              case 'X': // Close button
                  if ((!m_button[CloseButton]) && isCloseable()){
                      btn = createButton(CloseButton);
                      if (!btn) break;
                      btn->setTipText(i18n("Close") );
                      connect(btn, SIGNAL(clicked()), SLOT(closeWindow()));

                      m_button[CloseButton] = btn;
                  }
                  break;
              case 'F': // AboveButton button
                  if (!m_button[AboveButton]){
                      btn = createButton(AboveButton);
                      if (!btn) break;
                      bool above = keepAbove();
                      btn->setTipText(above?i18n("Do not keep above others"):i18n("Keep above others") );
                      btn->setToggleButton(true);
                      btn->setOn( above );
                      connect(btn, SIGNAL(clicked()), SLOT(slotKeepAbove()));

                      m_button[AboveButton] = btn;
                  }
                  break;
              case 'B': // BelowButton button
                  if (!m_button[BelowButton]){
                      btn = createButton(BelowButton);
                      if (!btn) break;
                      bool below = keepBelow();
                      btn->setTipText(below?i18n("Do not keep below others"):i18n("Keep below others") );
                      btn->setToggleButton(true);
                      btn->setOn( below );
                      connect(btn, SIGNAL(clicked()), SLOT(slotKeepBelow()));

                      m_button[BelowButton] = btn;
                  }
                  break;
              case 'L': // Shade button
                  if ((!m_button[ShadeButton]) && isShadeable()){
                      btn = createButton(ShadeButton);
                      if (!btn) break;
                      bool shaded = isSetShade();
                      btn->setTipText(shaded?i18n("Unshade"):i18n("Shade") );
                      btn->setToggleButton(true);
                      btn->setOn( shaded );
                      connect(btn, SIGNAL(clicked()), SLOT(slotShade()));

                      m_button[ShadeButton] = btn;
                  }
                  break;
              case '_': // Spacer item
                  btnContainer.append(0);
            }


            if (btn) {
                btn->setLeft(isLeft);
                btn->setSize(QSize(layoutMetric(LM_ButtonWidth, true, btn),layoutMetric(LM_ButtonHeight, true, btn)) );
                btn->show();
                btnContainer.append(btn);
            }

        }
    }
}

void KCommonDecoration::calcHiddenButtons()
{
    if (width() == btnHideLastWidth)
        return;

    btnHideLastWidth = width();

    //Hide buttons in the following order:
    KCommonDecorationButton* btnArray[] = { m_button[HelpButton], m_button[ShadeButton], m_button[BelowButton],
        m_button[AboveButton], m_button[OnAllDesktopsButton], m_button[MaxButton],
        m_button[MinButton], m_button[MenuButton], m_button[CloseButton] };
    const int buttonsCount = sizeof( btnArray ) / sizeof( btnArray[ 0 ] );

    int current_width = width();
    int count = 0;

    // Hide buttons
    while (current_width < btnHideMinWidth && count < buttonsCount)
    {
        if (btnArray[count] ) {
            current_width += btnArray[count]->width();
            if (btnArray[count]->isVisible() )
                btnArray[count]->hide();
        }
        count++;
    }
    // Show the rest of the buttons...
    for(int i = count; i < buttonsCount; i++)
    {
        if (btnArray[i] ) {

            if (! btnArray[i]->isHidden() )
                break; // all buttons shown...

            btnArray[i]->show();
        }
    }
}

void KCommonDecoration::show()
{
    if (decorationBehaviour(DB_ButtonHide) )
        calcHiddenButtons();
    widget()->show();
}

void KCommonDecoration::resize( const QSize& s )
{
    widget()->resize( s );
}

QSize KCommonDecoration::minimumSize() const
{
    const int minWidth = QMAX(layoutMetric(LM_TitleEdgeLeft), layoutMetric(LM_BorderLeft))
            +QMAX(layoutMetric(LM_TitleEdgeRight), layoutMetric(LM_BorderRight))
            +layoutMetric(LM_TitleBorderLeft)+layoutMetric(LM_TitleBorderRight);
    return QSize(minWidth,
                 layoutMetric(LM_TitleEdgeTop)+layoutMetric(LM_TitleHeight)
                         +layoutMetric(LM_TitleEdgeBottom)
                         +layoutMetric(LM_BorderBottom) );
}

void KCommonDecoration::maximizeChange()
{
    if( m_button[MaxButton] ) {
        m_button[MaxButton]->setOn( maximizeMode()==MaximizeFull);
        m_button[MaxButton]->setTipText( (maximizeMode()!=MaximizeFull) ?
                i18n("Maximize")
            : i18n("Restore"));
        m_button[MaxButton]->reset(KCommonDecorationButton::StateChange);
    }
}

void KCommonDecoration::desktopChange()
{
    if ( m_button[OnAllDesktopsButton] ) {
        m_button[OnAllDesktopsButton]->setOn( isOnAllDesktops() );
        m_button[OnAllDesktopsButton]->setTipText( isOnAllDesktops() ?
                i18n("Not on all desktops")
            : i18n("On all desktops"));
        m_button[OnAllDesktopsButton]->reset(KCommonDecorationButton::StateChange);
    }
}

void KCommonDecoration::shadeChange()
{
    if ( m_button[ShadeButton] ) {
        bool shaded = isSetShade();
        m_button[ShadeButton]->setOn( shaded );
        m_button[ShadeButton]->setTipText( shaded ?
                i18n("Unshade")
            : i18n("Shade"));
        m_button[ShadeButton]->reset(KCommonDecorationButton::StateChange);
    }
}

void KCommonDecoration::iconChange()
{
    if (m_button[MenuButton])
    {
        m_button[MenuButton]->update();
        m_button[MenuButton]->reset(KCommonDecorationButton::IconChange);
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
    if (m_button[AboveButton])
    {
        m_button[AboveButton]->setOn(above);
        m_button[AboveButton]->setTipText( above?i18n("Do not keep above others"):i18n("Keep above others") );
        m_button[AboveButton]->reset(KCommonDecorationButton::StateChange);
    }

    if (m_button[BelowButton] && m_button[BelowButton]->isOn())
    {
        m_button[BelowButton]->setOn(false);
        m_button[BelowButton]->setTipText( i18n("Keep below others") );
        m_button[BelowButton]->reset(KCommonDecorationButton::StateChange);
    }
}

void KCommonDecoration::keepBelowChange(bool below)
{
    if (m_button[BelowButton])
    {
        m_button[BelowButton]->setOn(below);
        m_button[BelowButton]->setTipText( below?i18n("Do not keep below others"):i18n("Keep below others") );
        m_button[BelowButton]->reset(KCommonDecorationButton::StateChange);
    }

    if (m_button[AboveButton] && m_button[AboveButton]->isOn())
    {
        m_button[AboveButton]->setOn(false);
        m_button[AboveButton]->setTipText( i18n("Keep above others") );
        m_button[AboveButton]->reset(KCommonDecorationButton::StateChange);
    }
}

void KCommonDecoration::slotMaximize()
{
    if (m_button[MaxButton])
    {
        maximize(m_button[MaxButton]->lastMousePress() );
        updateWindowShape();
    }
}

void KCommonDecoration::slotShade()
{
    setShade( !isSetShade() );
}

void KCommonDecoration::slotKeepAbove()
{
    setKeepAbove(!keepAbove() );
}

void KCommonDecoration::slotKeepBelow()
{
    setKeepBelow(!keepBelow() );
}

void KCommonDecoration::menuButtonPressed()
{
    static QTime* t = NULL;
    static KCommonDecoration* lastClient = NULL;
    if (t == NULL)
        t = new QTime;
    bool dbl = (lastClient==this && t->elapsed() <= QApplication::doubleClickInterval());
    lastClient = this;
    t->start();
    if (!dbl || !decorationBehaviour(DB_MenuClose) ) {
        QRect menuRect = m_button[MenuButton]->rect();
        QPoint menutop = m_button[MenuButton]->mapToGlobal(menuRect.topLeft());
        QPoint menubottom = m_button[MenuButton]->mapToGlobal(menuRect.bottomRight())+QPoint(0,2);
        KDecorationFactory* f = factory();
        showWindowMenu(QRect(menutop, menubottom));
        if( !f->exists( this )) // 'this' was deleted
            return;
        m_button[MenuButton]->setDown(false);
    }
    else
        closing = true;
}

void KCommonDecoration::menuButtonReleased()
{
    if(closing)
        closeWindow();
}

void KCommonDecoration::resizeEvent(QResizeEvent */*e*/)
{
    if (decorationBehaviour(DB_ButtonHide) )
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

    if (x!=oldX || y!=oldY)
        widget->move(x,y);
}

void KCommonDecoration::resizeWidget(int w, int h, QWidget *widget) const
{
    QSize s = widget->size();
    int oldW = s.width();
    int oldH = s.height();

    if (w!=oldW || h!=oldH)
        widget->resize(w,h);
}

void KCommonDecoration::mouseDoubleClickEvent(QMouseEvent *e)
{
    if( e->button() != Qt::LeftButton )
        return;

    int tb = layoutMetric(LM_TitleEdgeTop)+layoutMetric(LM_TitleHeight)+layoutMetric(LM_TitleEdgeBottom);
    // when shaded, react on double clicks everywhere to make it easier to unshade. otherwise
    // react only on double clicks in the title bar region...
    if (isSetShade() || e->pos().y() <= tb )
        titlebarDblClickOperation();
}

void KCommonDecoration::wheelEvent(QWheelEvent *e)
{
    int tb = layoutMetric(LM_TitleEdgeTop)+layoutMetric(LM_TitleHeight)+layoutMetric(LM_TitleEdgeBottom);
    if (isSetShade() || e->pos().y() <= tb )
        titlebarMouseWheelOperation( e->delta());
}

KCommonDecoration::Position KCommonDecoration::mousePosition(const QPoint &point) const
{
    const int corner = 18+3*layoutMetric(LM_BorderBottom, false)/2;
    Position pos = PositionCenter;

    QRect r = widget()->rect();
    int r_x, r_y, r_x2, r_y2;
    r.coords(&r_x, &r_y, &r_x2, &r_y2);
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

    const int borderBottomTop = r_y2-borderBottom+1;
    const int borderLeftRight = r_x+borderLeft-1;
//     const int borderRightLeft = r_x2-borderRight+1;
    const int titleEdgeLeftRight = r_x+titleEdgeLeft-1;
    const int titleEdgeRightLeft = r_x2-titleEdgeRight+1;
    const int titleEdgeBottomBottom = r_y+titleEdgeTop+titleHeight+titleEdgeBottom-1;
    const int titleEdgeTopBottom = r_y+titleEdgeTop-1;

    if (p_y <= titleEdgeTopBottom) {
        if (p_x <= r_x+corner)
            pos = PositionTopLeft;
        else if (p_x >= r_x2-corner)
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
        if      (p_y < r_y2-corner) {
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
    } else if(p_y >= borderBottomTop) {
        if (p_x <= r_x+corner)
            pos = PositionBottomLeft;
        else if (p_x >= r_x2-corner)
            pos = PositionBottomRight;
        else
            pos = PositionBottom;
    }

    return pos;
}

void KCommonDecoration::updateWindowShape()
{
    // don't mask the widget...
    if (!decorationBehaviour(DB_WindowMask) )
        return;

    int w = widget()->width();
    int h = widget()->height();

    bool tl=true,tr=true,bl=true,br=true; // is there a transparent rounded corner in top-left? etc

    QDesktopWidget *desktop=KApplication::desktop();
    // no transparent rounded corners if this window corner lines up with a screen corner
    for(int screen=0; screen < desktop->numScreens(); ++screen)
    {
        QRect fullscreen(desktop->screenGeometry(screen));
        QRect window = geometry();

        if(window.topLeft()    == fullscreen.topLeft() ) tl = false;
        if(window.topRight()   == fullscreen.topRight() ) tr = false;
        if(window.bottomLeft() == fullscreen.bottomLeft() ) bl = false;
        if(window.bottomRight()== fullscreen.bottomRight() ) br = false;
    }

    QRegion mask(0, 0, w, h);

    // Remove top-left corner.
    if(tl)
    {
        mask -= cornerShape(WC_TopLeft);
    }
    // Remove top-right corner.
    if(tr)
    {
        mask -= cornerShape(WC_TopRight);
    }
        // Remove top-left corner.
    if(bl)
    {
        mask -= cornerShape(WC_BottomLeft);
    }
    // Remove top-right corner.
    if(br)
    {
        mask -= cornerShape(WC_BottomRight);
    }

    setMask( mask );
}

bool KCommonDecoration::eventFilter( QObject* o, QEvent* e )
{
    if( o != widget())
        return false;
    switch( e->type())
    {
        case QEvent::Resize:
            resizeEvent(static_cast<QResizeEvent*>(e) );
            return true;
        case QEvent::Paint:
            paintEvent( static_cast< QPaintEvent* >( e ));
            return true;
        case QEvent::MouseButtonDblClick:
            mouseDoubleClickEvent( static_cast< QMouseEvent* >( e ));
            return true;
        case QEvent::MouseButtonPress:
            processMousePressEvent( static_cast< QMouseEvent* >( e ));
            return true;
        case QEvent::Wheel:
            wheelEvent( static_cast< QWheelEvent* >( e ));
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
    NET::WindowType type = windowType( SUPPORTED_WINDOW_TYPES_MASK );
    return ((type==NET::Toolbar)||(type==NET::Utility)||(type==NET::Menu));
}

QRect KCommonDecoration::titleRect() const
{
    int r_x, r_y, r_x2, r_y2;
    widget()->rect().coords(&r_x, &r_y, &r_x2, &r_y2);
    const int titleEdgeLeft = layoutMetric(LM_TitleEdgeLeft);
    const int titleEdgeTop = layoutMetric(LM_TitleEdgeTop);
    const int titleEdgeRight = layoutMetric(LM_TitleEdgeRight);
    const int titleEdgeBottom = layoutMetric(LM_TitleEdgeBottom);
    const int titleBorderLeft = layoutMetric(LM_TitleBorderLeft);
    const int titleBorderRight = layoutMetric(LM_TitleBorderRight);
    const int ttlHeight = layoutMetric(LM_TitleHeight);
    const int titleEdgeBottomBottom = r_y+titleEdgeTop+ttlHeight+titleEdgeBottom-1;
    return QRect(r_x+titleEdgeLeft+buttonsLeftWidth()+titleBorderLeft, r_y+titleEdgeTop,
              r_x2-titleEdgeRight-buttonsRightWidth()-titleBorderRight-(r_x+titleEdgeLeft+buttonsLeftWidth()+titleBorderLeft),
              titleEdgeBottomBottom-(r_y+titleEdgeTop) );
}


KCommonDecorationButton::KCommonDecorationButton(ButtonType type, KCommonDecoration *parent, const char *name)
    : QAbstractButton(parent->widget(), name),
        m_decoration(parent),
        m_type(type),
        m_realizeButtons(Qt::LeftButton),
        m_lastMouse(Qt::NoButton),
        m_isLeft(true)
{
    setCursor(Qt::ArrowCursor);
}

KCommonDecorationButton::~KCommonDecorationButton()
{
}

KCommonDecoration *KCommonDecorationButton::decoration() const
{
    return m_decoration;
}

ButtonType KCommonDecorationButton::type() const
{
    return m_type;
}

bool KCommonDecorationButton::isLeft() const
{
    return m_isLeft;
}

void KCommonDecorationButton::setLeft(bool left)
{
    m_isLeft = left;
}

void KCommonDecorationButton::setRealizeButtons(int btns)
{
    m_realizeButtons = btns;
}

void KCommonDecorationButton::setSize(const QSize &s)
{
    if (!m_size.isValid() || s != size() ) {
        m_size = s;

        setFixedSize(m_size);
        reset(SizeChange);
    }
}

QSize KCommonDecorationButton::sizeHint() const
{
    return m_size;
}

void KCommonDecorationButton::setTipText(const QString &tip) {
    QToolTip::remove(this );
    QToolTip::add(this, tip );
}

void KCommonDecorationButton::setToggleButton(bool toggle)
{
    QAbstractButton::setToggleButton(toggle);
    reset(ToggleChange);
}

void KCommonDecorationButton::setOn(bool on)
{
    if (on != isOn() ) {
        QAbstractButton::setOn(on);
        reset(StateChange);
    }
}

void KCommonDecorationButton::mousePressEvent(QMouseEvent* e)
{
    m_lastMouse = e->button();
    // pass on event after changing button to LeftButton
    QMouseEvent me(e->type(), e->pos(), e->globalPos(),
                   (e->button()&m_realizeButtons)?Qt::LeftButton : Qt::NoButton, e->state());

    QAbstractButton::mousePressEvent(&me);
}

void KCommonDecorationButton::mouseReleaseEvent(QMouseEvent* e)
{
    m_lastMouse = e->button();
    // pass on event after changing button to LeftButton
    QMouseEvent me(e->type(), e->pos(), e->globalPos(),
                   (e->button()&m_realizeButtons)?Qt::LeftButton : Qt::NoButton, e->state());

    QAbstractButton::mouseReleaseEvent(&me);
}
