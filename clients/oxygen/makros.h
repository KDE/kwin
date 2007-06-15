#ifndef OXYGEN_DEFS_H
#define OXYGEN_DEFS_H

#define _IsNotHtmlWidget(w) ( w->objectName() != "__khtml" )
#define _IsHtmlWidget(w) ( w->objectName() == "__khtml" )
#define _IsViewportChild(w) w->parent() &&\
( w->parent()->objectName() == "qt_viewport" || \
  w->parent()->objectName() == "qt_clipped_viewport" )

#define _HighContrastColor(c) (qGray(c.rgb()) < 128 ) ? Qt::white : Qt::black

#define _BLOCKEVENTS_(obj) obj->installEventFilter(eventKiller)
#define _UNBLOCKEVENTS_(obj) obj->removeEventFilter(eventKiller)

#define _IsTabStack(w) ( w->objectName() == "qt_tabwidget_stackedwidget" )

#define SAVE_PEN QPen saved_pen = painter->pen();
#define RESTORE_PEN painter->setPen(saved_pen);
#define SAVE_BRUSH QBrush saved_brush = painter->brush();
#define RESTORE_BRUSH painter->setBrush(saved_brush);
#define SAVE_ANTIALIAS bool hadAntiAlias = painter->renderHints() & QPainter::Antialiasing;
#define RESTORE_ANTIALIAS painter->setRenderHint(QPainter::Antialiasing, hadAntiAlias);

#define RECT option->rect
#define PAL option->palette
#define COLOR(_TYPE_) option->palette.color(QPalette::_TYPE_)
#define CONF_COLOR(_TYPE_) option->palette.color(config._TYPE_)

#endif //OXYGEN_DEFS_H
