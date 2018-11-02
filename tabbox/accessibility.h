#include <QtGui/QAccessible>

#include <QAccessibleWidget>

// ### FIXME

#include "tabbox/tabbox.h"

#ifndef KWIN_TABBOX_ACCESSIBILITY_H
#define KWIN_TABBOX_ACCESSIBILITY_H

namespace KWin {

class AbstractClient;
class Application;

namespace TabBox {

class ClientAccessible : public QAccessibleObject
{
public:
    ClientAccessible(AbstractClient *client, const QAccessibleInterface *parentInterface);

    QAccessibleInterface* parent() const override;

    QAccessibleInterface *child(int) const override
    { return nullptr; }
    int childCount() const override { return 0; }
    int indexOfChild(const QAccessibleInterface *) const override
    { return -1; }

    QString text(QAccessible::Text t) const override;
    QAccessible::Role role() const override;
    QAccessible::State state() const override;

private:
    KWin::AbstractClient *client() const;
    QAccessibleInterface *m_parent;
};

class TabBoxAccessible : public QAccessibleObject
{
public:
    TabBoxAccessible(KWin::TabBox::TabBox *parent);

    QAccessibleInterface* parent() const override;

    QAccessibleInterface *focusChild() const;

    QAccessibleInterface *child(int index) const override;
    int childCount() const override;
    int indexOfChild(const QAccessibleInterface *) const override;

    QString text(QAccessible::Text) const override;
    QAccessible::Role role() const override;
    QAccessible::State state() const override;

    // The TabBox is shown/hidden - update accessibility
    void show() const;
    void hide() const;

private:
    KWin::TabBox::TabBox *tabbox() const;
};

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

} // namespace TabBox
} // namespace KWin

#endif
