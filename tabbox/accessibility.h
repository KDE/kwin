#include <QtGui/QAccessible>

#include <QAccessibleWidget>

// ### FIXME

#include "tabbox/tabbox.h"
// #include "main.h"

#ifndef KWIN_TABBOX_ACCESSIBILITY_H
#define KWIN_TABBOX_ACCESSIBILITY_H

// namespace and stuff (why?)
namespace KWin {


class TabBoxAccessible : public QAccessibleObject
{
public:
    TabBoxAccessible(KWin::TabBox::TabBox *parent);

    QAccessibleInterface* parent() const override;

//    bool isValid() const override;
//    QObject *object() const override;
//    QWindow *window() const;

    // relations
//    QVector<QPair<QAccessibleInterface*, QAccessible::Relation> > relations(QAccessible::Relation match = QAccessible::AllRelations) const;
//    QAccessibleInterface *focusChild() const;

//    QAccessibleInterface *childAt(int x, int y) const override;

    // navigation, hierarchy
    QAccessibleInterface *child(int index) const override;
    int childCount() const override;
    int indexOfChild(const QAccessibleInterface *) const override;

    // properties and state
    QString text(QAccessible::Text) const override;
//    QRect rect() const override;
    QAccessible::Role role() const override;
    QAccessible::State state() const override;
};

class Application;

class KWinAccessibleApplication : public QAccessibleApplication
{
public:
    KWinAccessibleApplication(Application *app);

    int childCount() const override;

    QAccessibleInterface *child(int index) const override;

    int indexOfChild(const QAccessibleInterface *child) const override;

    void addChildAccessible(QAccessibleInterface *child) {
        qDebug() << Q_FUNC_INFO << "child" << child;
        m_child = child;  // FIXME: lifetime!!!
    }

private:
    Application *m_KWinApplication = nullptr;
    QAccessibleInterface *m_child = nullptr;
};

}


#endif
