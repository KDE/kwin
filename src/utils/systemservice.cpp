#include "systemservice.h"
#include <systemd/sd-daemon.h>

#include <QDebug>
#include <QTemporaryFile>
#include <QVariant>

using namespace KWinSystemService;

void KWinSystemService::markReady()
{
    sd_notify(1, "READY=1");
}

FDStore::FDStore()
{
    m_pid = getpid();

    char **names = nullptr;
    sd_listen_fds_with_names(false, &names);
    if (names) {
        for (int i = 0; names[i]; i++) {
            m_store.insert(names[i], SD_LISTEN_FDS_START + i);
            free(names[i]);
        }
        // Dave, do they need fnct, O_CLO_EXEC?
        free(names);
    }

    qDebug() << m_store;

    if (int metaDataFd = m_store.take("_metadata")) {
        qDebug() << "reading old metadata";
        // read the meta data
        m_metaDataFile.open(metaDataFd, QFile::ReadWrite, QFileDevice::AutoCloseHandle);
        m_metaDataFile.reset(); // explicitly seek to the start before reading
        QDataStream stream(&m_metaDataFile);
        stream >> m_metaData;
    } else {
        QTemporaryFile tmp;
        tmp.open();
        m_metaDataFile.open(dup(tmp.handle()), QFile::ReadWrite, QFileDevice::AutoCloseHandle);

        int fd = m_metaDataFile.handle();
        int rc = sd_pid_notifyf_with_fds(m_pid, false, &fd, 1, "FDSTORE=1\nFDNAME=_metadata");
        if (rc < 1) {
            qDebug() << "failed to set metadata file as persistent";
        }
    }
}

void FDStore::store(const QString &id, int fd)
{
    Q_ASSERT(!id.contains(QLatin1Char('\n')));
    Q_ASSERT(!id.isEmpty());

    // systemd can handle multiple under the same name. Should we?
    if (m_store.contains(id)) {
        Q_ASSERT(false);
        return;
    }
    m_store[id] = fd;
    int rc = sd_pid_notifyf_with_fds(m_pid, false, &fd, 1, "FDSTORE=1\nFDNAME=%s", id.toLatin1().constData());
    qDebug() << rc;
}

QStringList FDStore::ids() const
{
    return m_store.keys(); // will be useful for SecurityContexts
}

int FDStore::storedFd(const QString &id) const
{
    return m_store.value(id, -1);
}

void FDStore::remove(const QString &id)
{
    m_store.remove(id);
    sd_pid_notifyf(m_pid, true, "FDSTOREREMOVE=1\nFDNAME=%s", id.toLatin1().constData());
}

QVariant FDStore::metaData(const QString &id) const
{
    return m_metaData.value(id);
}

void FDStore::storeMetadata(const QString &id, const QVariant &metaData)
{
    m_metaData[id] = metaData;

    m_metaDataFile.reset();
    QDataStream stream(&m_metaDataFile);
    stream << m_metaData;
    m_metaDataFile.flush();
}

KWinSystemService::FDStore *KWinSystemService::FDStore::instance()
{
    static FDStore s_instance;
    return &s_instance;
}
