#pragma once

#include <QHash>
#include <QString>
#include <QTemporaryFile>

// intention is to keep this generic enough to move to frameworks under a
// new namespace
// no-op without systemd internally?

namespace KWinSystemService
{

Q_DECL_EXPORT void markReady();

class Q_DECL_EXPORT FDStore
{
public:
    void store(const QString &id, int fd);
    int storedFd(const QString &id) const;
    QStringList ids() const;
    void remove(const QString &id);

    QVariant metaData(const QString &id) const;
    void storeMetadata(const QString &id, const QVariant &metaData);

    static FDStore *instance(); // Dave, make all above methods static, private static pimpl?
private:
    FDStore();
    QHash<QString, QVariant> m_metaData;
    QHash<QString, int> m_store;
    int m_pid;
    QFile m_metaDataFile;
};

// other things to put in this namespace
}
