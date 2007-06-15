/***************************************************************************
 *   Copyright (C) 2006-2007 by Thomas L�bking                             *
 *   thomas.luebking@web.de                                                *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/

#include "oxrender.h"

#define ANIMATIONS (activeTabs + progressbars.count() + \
hoverWidgets.count() + complexHoverWidgets.count() + indexedHoverWidgets.count())

#define startTimer if (!timer->isActive()) timer->start(50)

bool TabAnimInfo::eventFilter( QObject* object, QEvent* event ) {
   if (event->type() != QEvent::Paint || !animStep)
      return false;
   QPainter p((QWidget*)object);
   p.drawPixmap(0,0, tabPix[2]);
   p.end();
   return true;
}

// QPixmap::grabWidget(.) currently fails on the background offset,
// so we use our own implementation
//TODO: fix scrollareas (the scrollbars aren't painted, so check for availability and usage...)
QPixmap grabWidget(QWidget * root) {
    if (!root)
        return QPixmap();
   
   QPixmap pix(root->size());
   QPoint zero(0,0);
   QPainter p(&pix);
   const QBrush bg = root->palette().brush(root->backgroundRole());
   if (bg.style() == Qt::TexturePattern)
      p.drawTiledPixmap(root->rect(), bg.texture(), root->mapTo(root->topLevelWidget(), zero));
   else
      p.fillRect(root->rect(), bg);
   p.end();
   
   QList <QWidget*> widgets = root->findChildren<QWidget*>();
   
   // resizing (in case)
   if (root->testAttribute(Qt::WA_PendingResizeEvent) ||
       !root->testAttribute(Qt::WA_WState_Created)) {
      QResizeEvent e(root->size(), QSize());
      QApplication::sendEvent(root, &e);
   }
   foreach (QWidget *w, widgets) {
      if (root->testAttribute(Qt::WA_PendingResizeEvent) ||
         !root->testAttribute(Qt::WA_WState_Created)) {
         QResizeEvent e(w->size(), QSize());
         QApplication::sendEvent(w, &e);
      }
   }
   
   // painting
   QPainter::setRedirected( root, &pix );
   QPaintEvent e = QPaintEvent(QRect(zero, root->size()));
   QCoreApplication::sendEvent(root, &e);
   QPainter::restoreRedirected(root);
   
   foreach (QWidget *w, widgets) {
      if (w->isVisibleTo(root)) {
         if (w->autoFillBackground()) {
            const QBrush bg = w->palette().brush(w->backgroundRole());
            QPainter p(&pix);
            QRect wrect = QRect(zero, w->size()).translated(w->mapTo(root, zero));
            if (bg.style() == Qt::TexturePattern)
               p.drawTiledPixmap(wrect, bg.texture(), w->mapTo(root->topLevelWidget(), zero));
            else
               p.fillRect(wrect, bg);
            p.end();
         }
         QPainter::setRedirected( w, &pix, -w->mapTo(root, zero) );
         e = QPaintEvent(QRect(zero, w->size()));
         QCoreApplication::sendEvent(w, &e);
         QPainter::restoreRedirected(w);
      }
   }
   
   return pix;
}

static QHash<QWidget*, uint> progressbars;
typedef QHash<QWidget*, HoverFadeInfo> HoverFades;
static HoverFades hoverWidgets;
typedef QHash<QWidget*, ComplexHoverFadeInfo> ComplexHoverFades;
static ComplexHoverFades complexHoverWidgets;
typedef QHash<QWidget*, IndexedFadeInfo> IndexedFades;
static IndexedFades indexedHoverWidgets;
static QHash<QTabWidget*, TabAnimInfo*> tabwidgets;
static int activeTabs = 0;

// --- ProgressBars --------------------
void OxygenStyle::updateProgressbars() {
   if (progressbars.isEmpty())
      return;
   //Update the registered progressbars.
   QHash<QWidget*, uint>::iterator iter;
   QProgressBar *pb;
   animationUpdate = true;
   for (iter = progressbars.begin(); iter != progressbars.end(); iter++) {
      if ( !qobject_cast<QProgressBar*>(iter.key()) )
         continue;
      pb = (QProgressBar*)(iter.key());
      if (pb->paintingActive() || !pb->isVisible() ||
          !(pb->value() > pb->minimum()) || !(pb->value() < pb->maximum()))
         continue;
//       int mod = (pb->orientation() == Qt::Horizontal) ?
//          (pb->height()-dpi.$8)*2 :
//          (pb->width()-dpi.$6)*2;
//       if (!mod)
//          continue;
//       ++iter.value();
//       if (mod)
//          iter.value() %= mod;
      if (iter.value() % 2) { // odd - fade out
         iter.value() -= 2;
         if (iter.value() < 4) // == 3
            ++iter.value(); // 4
         if ((iter.value()+1) % 4) // save some cycles...
            continue;
      }
      else { //fade in
         iter.value() += 2;
         if (iter.value() > 39) // == 40
            ++iter.value(); // 41
         if (iter.value() % 4) // save some cycles...
            continue;
      }
      pb->repaint(pb->rect().adjusted(dpi.$3,dpi.$4,-dpi.$3,-dpi.$5));
   }
   animationUpdate = false;
}

int OxygenStyle::progressStep(const QWidget *w) const {
   return progressbars.value(const_cast<QWidget*>(w),0);
}

void OxygenStyle::progressbarDestroyed(QObject* obj) {
   progressbars.remove(static_cast<QWidget*>(obj));
   if (!ANIMATIONS) timer->stop();
}

// --- TabWidgets --------------------
void OxygenStyle::tabChanged(int index) {
   if (config.tabTransition == Jump) return; // ugly nothing ;)
   QTabWidget* tw = (QTabWidget*)sender();
   QHash<QTabWidget*, TabAnimInfo*>::iterator i = tabwidgets.find(tw);
   if (i == tabwidgets.end()) // this tab isn't handled for some reason?
      return;
   
   TabAnimInfo* tai = i.value();
   
   QWidget *ctw = tw->widget(tai->lastTab);
   tai->lastTab = index;
   if (!ctw)
      return;
   tai->tabPix[0] = /*QPixmap::*/grabWidget(ctw);
   
   ctw = tw->currentWidget();
   if (!ctw) {
      tai->tabPix[0] = QPixmap();
      return;
   }
   tai->tabPix[1] = /*QPixmap::*/grabWidget(ctw);
   tai->tabPix[2] = tai->tabPix[0];
   
   tai->animStep = 6;
   // TAB Transitions, NOTE, I'm using a bit X11 code here as this prevents deep pixmap
   // copies that are sometimes not necessary
   // TODO make an - ifdef'd ? - qt version for other platforms
   switch (config.tabTransition) {
   case CrossFade:
      OXRender::blend(tai->tabPix[1], tai->tabPix[2], 0.1666);
      break;
   case SlideIn: {
      //TODO handle different bar positions
      Display *dpy = QX11Info::display();
      GC gc = XCreateGC( dpy, tai->tabPix[2].handle(), 0, 0 );
      XCopyArea( dpy, tai->tabPix[1].handle(), tai->tabPix[2].handle(), gc,
                  0, 6*tai->tabPix[1].height()/7,
                 tai->tabPix[1].width(), tai->tabPix[1].height()/7, 0, 0 );
      XFreeGC ( dpy , gc );
      break;
   }
   case SlideOut: {
      tai->tabPix[2] = tai->tabPix[1];
      //TODO handle different bar positions
      QPainter p(&tai->tabPix[2]);
      p.drawPixmap(0,0,tai->tabPix[0],0,tai->tabPix[0].height()/7,tai->tabPix[0].width(),6*tai->tabPix[0].height()/7);
      break;
   }
   case RollOut: {
      Display *dpy = QX11Info::display();
      GC gc = XCreateGC( dpy, tai->tabPix[2].handle(), 0, 0 );
      int h = tai->tabPix[1].height()/7;
      int y = (tai->tabPix[1].height()-h)/2;
      XCopyArea( dpy, tai->tabPix[1].handle(), tai->tabPix[2].handle(), gc,
                 0, y, tai->tabPix[1].width(), h, 0, y );
      XFreeGC ( dpy , gc );
      break;
   }
   case RollIn: {
      Display *dpy = QX11Info::display();
      GC gc = XCreateGC( dpy, tai->tabPix[2].handle(), 0, 0 );
      int h = tai->tabPix[1].height()/14;
      XCopyArea( dpy, tai->tabPix[1].handle(), tai->tabPix[2].handle(), gc,
                 0, 0, tai->tabPix[1].width(), h, 0, 0 );
      XCopyArea( dpy, tai->tabPix[1].handle(), tai->tabPix[2].handle(), gc,
                 0, tai->tabPix[1].height()-h, tai->tabPix[1].width(), h,
                 0, tai->tabPix[1].height()-h );
      XFreeGC ( dpy , gc );
      break;
   }
   case CloseVertically: {
      Display *dpy = QX11Info::display();
      GC gc = XCreateGC( dpy, tai->tabPix[2].handle(), 0, 0 );
      int h = tai->tabPix[1].height()/14;
      XCopyArea( dpy, tai->tabPix[1].handle(), tai->tabPix[2].handle(), gc,
                 0, tai->tabPix[1].height()/2-h, tai->tabPix[1].width(), h, 0, 0 );
      XCopyArea( dpy, tai->tabPix[1].handle(), tai->tabPix[2].handle(), gc,
                 0, tai->tabPix[1].height()/2, tai->tabPix[1].width(), h,
                 0, tai->tabPix[1].height()-h );
      XFreeGC ( dpy , gc );
      break;
   }
   case OpenVertically: {
      tai->tabPix[2] = tai->tabPix[1];
      QPainter p(&tai->tabPix[2]);
      int h = 6*tai->tabPix[0].height()/14;
      p.drawPixmap(0,0,tai->tabPix[0],0,tai->tabPix[0].height()/2-h,
                   tai->tabPix[0].width(),h);
      p.drawPixmap(0,tai->tabPix[0].height()-h,tai->tabPix[0],
                   0,tai->tabPix[0].height()/2,tai->tabPix[0].width(),h);
      break;
   }
   case CloseHorizontally: {
      Display *dpy = QX11Info::display();
      GC gc = XCreateGC( dpy, tai->tabPix[2].handle(), 0, 0 );
      int w = tai->tabPix[1].width()/14;
      XCopyArea( dpy, tai->tabPix[1].handle(), tai->tabPix[2].handle(), gc,
                 tai->tabPix[1].width()/2-w, 0, w, tai->tabPix[1].height(), 0, 0 );
      XCopyArea( dpy, tai->tabPix[1].handle(), tai->tabPix[2].handle(), gc,
                 tai->tabPix[1].width()/2, 0, w, tai->tabPix[1].height(),
                 tai->tabPix[1].width()-w, 0 );
      XFreeGC ( dpy , gc );
      break;
   }
   case OpenHorizontally: {
      tai->tabPix[2] = tai->tabPix[1];
      QPainter p(&tai->tabPix[2]);
      int w = 6*tai->tabPix[0].width()/14;
      p.drawPixmap(0,0,tai->tabPix[0],tai->tabPix[0].width()/2-w,0,
                   w,tai->tabPix[0].height());
      p.drawPixmap(tai->tabPix[0].width()-w,0,tai->tabPix[0],
                   tai->tabPix[0].width()/2,0,w,tai->tabPix[0].height());
      break;
   }
   case ScanlineBlend:
   default: {
      Display *dpy = QX11Info::display();
      GC gc = XCreateGC( dpy, tai->tabPix[2].handle(), 0, 0 );
      for (int i = 6; i < tai->tabPix[2].height(); i+=6)
         XCopyArea( dpy, tai->tabPix[1].handle(), tai->tabPix[2].handle(), gc,
                     0, i, tai->tabPix[1].width(), 1, 0, i );
      XFreeGC ( dpy , gc );
   }
   }
   ctw->parentWidget()->installEventFilter(tai);
   _BLOCKEVENTS_(ctw);
   QList<QWidget*> widgets = ctw->findChildren<QWidget*>();
   foreach(QWidget *widget, widgets) {
      _BLOCKEVENTS_(widget);
      if (widget->autoFillBackground()) {
         tai->autofillingWidgets.append(widget);
         widget->setAutoFillBackground(false);
      }
   }
   ctw->repaint();
   startTimer;
}

void OxygenStyle::updateTabAnimation() {
   if (tabwidgets.isEmpty())
      return;
   QHash<QTabWidget*, TabAnimInfo*>::iterator i;
   activeTabs = 0;
   TabAnimInfo* tai;
   QWidget *ctw = 0, *widget = 0; QList<QWidget*> widgets;
   int index;
   for (i = tabwidgets.begin(); i != tabwidgets.end(); i++) {
      tai = i.value();
      if (!tai->animStep)
         continue;
      ctw = i.key()->currentWidget();
      if (! --(tai->animStep)) { // zero, stop animation
         tai->tabPix[2] =
            tai->tabPix[1] =
            tai->tabPix[0] = QPixmap();
         ctw->parentWidget()->removeEventFilter(tai);
         _UNBLOCKEVENTS_(ctw);
         widgets = ctw->findChildren<QWidget*>();
//          ctw->repaint();
         foreach(widget, widgets) {
            index = tai->autofillingWidgets.indexOf(widget);
            if (index != -1) {
               tai->autofillingWidgets.removeAt(index);
               widget->setAutoFillBackground(true);
            }
            _UNBLOCKEVENTS_(widget);
            widget->update(); //if necessary
         }
         ctw->repaint(); //asap
         tai->autofillingWidgets.clear();
         continue;
      }
      ++activeTabs;
      switch (config.tabTransition) {
      case CrossFade:
         OXRender::blend(tai->tabPix[1], tai->tabPix[2], 1.1666-0.1666*tai->animStep);
         break;
      case SlideIn: {
         //TODO handle different bar positions
         Display *dpy = QX11Info::display();
         GC gc = XCreateGC( dpy, tai->tabPix[2].handle(), 0, 0 );
         XCopyArea( dpy, tai->tabPix[1].handle(), tai->tabPix[2].handle(), gc,
                     0, tai->animStep*tai->tabPix[1].height()/7,
                    tai->tabPix[1].width(), (7-tai->animStep)*tai->tabPix[1].height()/7,
                    0, 0 );
         XFreeGC ( dpy , gc );
         break;
      }
      case SlideOut: {
         tai->tabPix[2] = tai->tabPix[1];
         //TODO handle different bar positions
         QPainter p(&tai->tabPix[2]);
         p.drawPixmap(0,0,tai->tabPix[0],0,(7-tai->animStep)*tai->tabPix[0].height()/7,tai->tabPix[0].width(),tai->animStep*tai->tabPix[0].height()/7);
         break;
      }
      case RollOut: {
         Display *dpy = QX11Info::display();
         GC gc = XCreateGC( dpy, tai->tabPix[2].handle(), 0, 0 );
         int h = (7-tai->animStep)*tai->tabPix[1].height()/7;
         int y = (tai->tabPix[1].height()-h)/2;
         XCopyArea( dpy, tai->tabPix[1].handle(), tai->tabPix[2].handle(), gc,
                     0, y, tai->tabPix[1].width(), h, 0, y );
         XFreeGC ( dpy , gc );
         break;
      }
      case RollIn: {
         Display *dpy = QX11Info::display();
         GC gc = XCreateGC( dpy, tai->tabPix[2].handle(), 0, 0 );
         int h = (7-tai->animStep)*tai->tabPix[1].height()/14;
         XCopyArea( dpy, tai->tabPix[1].handle(), tai->tabPix[2].handle(), gc,
                  0, 0, tai->tabPix[1].width(), h, 0, 0 );
         XCopyArea( dpy, tai->tabPix[1].handle(), tai->tabPix[2].handle(), gc,
                  0, tai->tabPix[1].height()-h, tai->tabPix[1].width(), h,
                  0, tai->tabPix[1].height()-h );
         XFreeGC ( dpy , gc );
         break;
      }
      case CloseVertically: {
         Display *dpy = QX11Info::display();
         GC gc = XCreateGC( dpy, tai->tabPix[2].handle(), 0, 0 );
         int h = (7-tai->animStep)*tai->tabPix[1].height()/14;
         XCopyArea( dpy, tai->tabPix[1].handle(), tai->tabPix[2].handle(), gc,
                  0, tai->tabPix[1].height()/2-h, tai->tabPix[1].width(), h, 0, 0 );
         XCopyArea( dpy, tai->tabPix[1].handle(), tai->tabPix[2].handle(), gc,
                  0, tai->tabPix[1].height()/2, tai->tabPix[1].width(), h,
                  0, tai->tabPix[1].height()-h );
         XFreeGC ( dpy , gc );
         break;
      }
      case OpenVertically: {
         tai->tabPix[2] = tai->tabPix[1];
         QPainter p(&tai->tabPix[2]);
         int h = tai->animStep*tai->tabPix[0].height()/14;
         p.drawPixmap(0,0,tai->tabPix[0],0,tai->tabPix[0].height()/2-h,
                     tai->tabPix[0].width(),h);
         p.drawPixmap(0,tai->tabPix[0].height()-h,tai->tabPix[0],
                     0,tai->tabPix[0].height()/2,tai->tabPix[0].width(),h);
         break;
      }
      case CloseHorizontally: {
         Display *dpy = QX11Info::display();
         GC gc = XCreateGC( dpy, tai->tabPix[2].handle(), 0, 0 );
         int w = (7-tai->animStep)*tai->tabPix[1].width()/14;
         XCopyArea( dpy, tai->tabPix[1].handle(), tai->tabPix[2].handle(), gc,
                  tai->tabPix[1].width()/2-w, 0, w, tai->tabPix[1].height(), 0, 0 );
         XCopyArea( dpy, tai->tabPix[1].handle(), tai->tabPix[2].handle(), gc,
                  tai->tabPix[1].width()/2, 0, w, tai->tabPix[1].height(),
                  tai->tabPix[1].width()-w, 0 );
         XFreeGC ( dpy , gc );
         break;
      }
      case OpenHorizontally: {
         tai->tabPix[2] = tai->tabPix[1];
         QPainter p(&tai->tabPix[2]);
         int w = tai->animStep*tai->tabPix[0].width()/14;
         p.drawPixmap(0,0,tai->tabPix[0],tai->tabPix[0].width()/2-w,0,
                     w,tai->tabPix[0].height());
         p.drawPixmap(tai->tabPix[0].width()-w,0,tai->tabPix[0],
                     tai->tabPix[0].width()/2,0,w,tai->tabPix[0].height());
         break;
      }
      case ScanlineBlend:
      default: {
      Display *dpy = QX11Info::display();
      GC gc = XCreateGC( dpy, tai->tabPix[2].handle(), 0, 0 );
      for (int i = tai->animStep; i < tai->tabPix[2].height(); i+=6)
         XCopyArea( dpy, tai->tabPix[1].handle(), tai->tabPix[2].handle(), gc,
                     0, i, tai->tabPix[1].width(), 1, 0, i );
      XFreeGC ( dpy , gc );
      }
      }
      ctw->parentWidget()->repaint();
   }
   if (!ANIMATIONS) timer->stop();
}

void OxygenStyle::tabDestroyed(QObject* obj) {
   tabwidgets.remove(static_cast<QTabWidget*>(obj));
//    delete tai;
   if (!ANIMATIONS) timer->stop();
}


// -- Buttons etc. -------------------------------
void OxygenStyle::updateFades() {
   if (hoverWidgets.isEmpty())
      return;
   HoverFades::iterator it = hoverWidgets.begin();
   while (it != hoverWidgets.end()) {
      if (it.value().fadeIn) {
         it.value().step += 1;
         it.key()->update();
         if (it.value().step > 7)
            it = hoverWidgets.erase(it);
         else
            ++it;
      }
      else { // fade out
         it.value().step-=2;
         it.key()->update();
         if (it.value().step < 1)
            it = hoverWidgets.erase(it);
         else
            ++it;
      }
   }
   if (!ANIMATIONS) timer->stop();
}

void OxygenStyle::fadeIn(QWidget *widget) {
   HoverFades::iterator it = hoverWidgets.find(widget);
   if (it == hoverWidgets.end()) {
      it = hoverWidgets.insert(widget, HoverFadeInfo(1, true));
   }
   it.value().fadeIn = true;
   connect(widget, SIGNAL(destroyed(QObject*)), this, SLOT(fadeDestroyed(QObject*)));
   startTimer;
}

void OxygenStyle::fadeOut(QWidget *widget) {
   HoverFades::iterator it = hoverWidgets.find(widget);
   if (it == hoverWidgets.end()) {
      it = hoverWidgets.insert(widget, HoverFadeInfo(6, false));
   }
   it.value().fadeIn = false;
   connect(widget, SIGNAL(destroyed(QObject*)), this, SLOT(fadeDestroyed(QObject*)));
   startTimer;
}

void OxygenStyle::fadeDestroyed(QObject* obj) {
   hoverWidgets.remove(static_cast<QWidget*>(obj));
   if (!ANIMATIONS) timer->stop();
}

int OxygenStyle::hoverStep(const QWidget *widget) const {
   if (!widget)
      return 0;
   HoverFades::iterator it = hoverWidgets.find(const_cast<QWidget*>(widget));
   if (it != hoverWidgets.end())
      return it.value().step;
   return 0;
}

// -- Complex controls ----------------------

void OxygenStyle::updateComplexFades() {
   if (complexHoverWidgets.isEmpty())
      return;
   bool update;
   ComplexHoverFades::iterator it = complexHoverWidgets.begin();
   while (it != complexHoverWidgets.end()) {
      ComplexHoverFadeInfo &info = it.value();
      update = false;
      for (SubControl control = (SubControl)0x01;
           control <= (SubControl)0x80;
           control = (SubControl)(control<<1)) {
         if (info.fadingInControls & control) {
            update = true;
            info.steps[control] += 2;
            if (info.steps.value(control) > 4)
               info.fadingInControls &= ~control;
         }
         else if (info.fadingOutControls & control) {
            update = true;
            --info.steps[control];
            if (info.steps.value(control) < 1)
               info.fadingOutControls &= ~control;
         }
      }
      if (update)
         it.key()->update();
      if (info.activeSubControls == SC_None && // needed to detect changes!
          info.fadingOutControls == SC_None &&
          info.fadingInControls == SC_None)
         it = complexHoverWidgets.erase(it);
      else
         ++it;
   }
   if (!ANIMATIONS) timer->stop();
}

const ComplexHoverFadeInfo *OxygenStyle::complexHoverFadeInfo(const QWidget *widget,
   SubControls activeSubControls) const {
      QWidget *w = const_cast<QWidget*>(widget);
   ComplexHoverFades::iterator it = complexHoverWidgets.find(w);
   if (it == complexHoverWidgets.end()) {
      // we have no entry yet
      if (activeSubControls == SC_None)
         return 0; // no need here
      // ...but we'll need one
      it = complexHoverWidgets.insert(w, ComplexHoverFadeInfo());
      connect(widget, SIGNAL(destroyed(QObject*)),
               this, SLOT(complexFadeDestroyed(QObject*)));
      startTimer;
   }
   // we now have an entry - check for validity and update in case
   ComplexHoverFadeInfo *info = &it.value();
   if (info->activeSubControls != activeSubControls) { // sth. changed
      SubControls diff = info->activeSubControls ^ activeSubControls;
      SubControls newActive = diff & activeSubControls;
      SubControls newDead = diff & info->activeSubControls;
      info->fadingInControls &= ~newDead;
      info->fadingInControls |= newActive;
      info->fadingOutControls &= ~newActive;
      info->fadingOutControls |= newDead;
      info->activeSubControls = activeSubControls;
      for (SubControl control = (SubControl)0x01;
      control <= (SubControl)0x80;
      control = (SubControl)(control<<1)) {
         if (newActive & control)
            info->steps[control] = 1;
         else if (newDead & control) {
            info->steps[control] = 6;
         }
      }
   }
   return info;
}

void OxygenStyle::complexFadeDestroyed(QObject* obj) {
   complexHoverWidgets.remove(static_cast<QWidget*>(obj));
   if (!ANIMATIONS) timer->stop();
}

// -- Indexed items like menus, tabs ---------------------
void OxygenStyle::updateIndexedFades() {
   if (indexedHoverWidgets.isEmpty())
      return;
   IndexedFades::iterator it;
   QHash<long int, int>::iterator stepIt;
   it = indexedHoverWidgets.begin();
   while (it != indexedHoverWidgets.end()) {
      IndexedFadeInfo &info = it.value();
      if (info.fadingInIndices.isEmpty() && info.fadingOutIndices.isEmpty()) {
         ++it;
         continue;
      }
      
      stepIt = info.fadingInIndices.begin();
      while (stepIt != info.fadingInIndices.end()) {
         stepIt.value() += 2;
         if (stepIt.value() > 4)
            stepIt = info.fadingInIndices.erase(stepIt);
         else
            ++stepIt;
      }
      
      stepIt = info.fadingOutIndices.begin();
      while (stepIt != info.fadingOutIndices.end()) {
         --stepIt.value();
         if (stepIt.value() < 1)
            stepIt = info.fadingOutIndices.erase(stepIt);
         else
            ++stepIt;
      }
      
      it.key()->update();
      
      if (info.index == 0L && // nothing actually hovered
          info.fadingInIndices.isEmpty() && // no fade ins
          info.fadingOutIndices.isEmpty()) // no fade outs
         it = indexedHoverWidgets.erase(it);
      else
         ++it;
   }
   if (!ANIMATIONS) timer->stop();
}

const IndexedFadeInfo *OxygenStyle::indexedFadeInfo(const QWidget *widget,
   long int index) const {
   QWidget *w = const_cast<QWidget*>(widget);
   IndexedFades::iterator it = indexedHoverWidgets.find(w);
   if (it == indexedHoverWidgets.end()) {
      // we have no entry yet
      if (index == 0L)
         return 0L;
      // ... but we'll need one
      it = indexedHoverWidgets.insert(w, IndexedFadeInfo(0L));
      connect(widget, SIGNAL(destroyed(QObject*)),
               this, SLOT(indexedFadeDestroyed(QObject*)));
      startTimer;
   }
   // we now have an entry - check for validity and update in case
   IndexedFadeInfo *info = &it.value();
   if (info->index != index) { // sth. changed
      info->fadingInIndices[index] = 1;
      if (info->index)
         info->fadingOutIndices[info->index] = 6;
      info->index = index;
   }
   return info;
}

void OxygenStyle::indexedFadeDestroyed(QObject* obj) {
   indexedHoverWidgets.remove(static_cast<QWidget*>(obj));
   if (!ANIMATIONS) timer->stop();
}

int IndexedFadeInfo::step(long int index) {
   typedef QHash<long int, int> Index2Step;
   Index2Step::iterator stepIt;
   for (stepIt = fadingInIndices.begin(); stepIt != fadingInIndices.end(); stepIt++)
      if (stepIt.key() == index)
         return stepIt.value();
   for (stepIt = fadingOutIndices.begin(); stepIt != fadingOutIndices.end(); stepIt++)
      if (stepIt.key() == index)
         return stepIt.value();
   return 0;
}
