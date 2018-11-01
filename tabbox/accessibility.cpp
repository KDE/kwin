
#include "tabbox/accessibility.h"
#include "main.h"

#include <qaccessible.h>

using namespace KWin;

TabBoxAccessible::TabBoxAccessible(TabBox::TabBox *parent) : QAccessibleObject(parent) {
    auto *appInterface = QAccessible::queryAccessibleInterface(KWin::kwinApp());
    KWinAccessibleApplication *appAccessible = static_cast<KWinAccessibleApplication*>(appInterface);
    appAccessible->addChildAccessible(this);
    qDebug() << Q_FUNC_INFO << "Created tab box accessible with object:" << parent;
}

QAccessibleInterface *TabBoxAccessible::parent() const {
    return QAccessible::queryAccessibleInterface(KWin::kwinApp());
}

QAccessibleInterface *TabBoxAccessible::child(int index) const
{
    Q_UNUSED(index); // FIXME
    return nullptr;
}

int TabBoxAccessible::childCount() const
{
    return 0;
}

int TabBoxAccessible::indexOfChild(const QAccessibleInterface *) const
{
    return -1;
}

QString TabBoxAccessible::text(QAccessible::Text t) const
{
    if (t == QAccessible::Name) {
        return "Window Switcher Window";
    }
    return "KWIN THING"; // FIXME
}

QAccessible::Role TabBoxAccessible::role() const
{
    return QAccessible::Window;
}

QAccessible::State TabBoxAccessible::state() const
{
    QAccessible::State s;
    s.focused = true;
    return s;
}

KWinAccessibleApplication::KWinAccessibleApplication(Application *app)
{
    Q_UNUSED(app);  // FIXME remove parameter?
}

int KWinAccessibleApplication::childCount() const {
    int count = QAccessibleApplication::childCount();
    if (m_child) {
        ++count;
    }
    return count;
}

QAccessibleInterface *KWinAccessibleApplication::child(int index) const {
//    qDebug() << Q_FUNC_INFO << index << "CC" << QAccessibleApplication::childCount();
    if (index < QAccessibleApplication::childCount()) {
        return QAccessibleApplication::child(index);
    } else if (index == QAccessibleApplication::childCount()) {
        qDebug() << "Getting VIRTUAL CHILD";
        return m_child;
    }
//    qDebug() << "Returning nullptr as child";
    return nullptr;
}

int KWinAccessibleApplication::indexOfChild(const QAccessibleInterface *child) const {
    if (child == m_child) {
        return childCount() - 1;
    }
    return QAccessibleApplication::indexOfChild(child);
}
