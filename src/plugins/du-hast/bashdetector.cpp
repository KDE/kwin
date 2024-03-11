#include "bashdetector.h"

using namespace std::chrono_literals;

namespace KWin
{

BashDetector::BashDetector()
{
}

BashDetector::~BashDetector()
{
}

bool BashDetector::update(KeyEvent *event)
{
    // Prune the old entries in the history.
    auto it = m_history.begin();
    for (; it != m_history.end(); ++it) {
        if (event->timestamp() - it->timestamp < 5s) {
            break;
        }
    }
    if (it != m_history.begin()) {
        m_history.erase(m_history.begin(), it);
    }

    m_history.emplace_back(HistoryItem{
        .keyCode = event->nativeScanCode(),
        .timestamp = event->timestamp(),
    });

    std::map<quint32, size_t> entries;
    for (const HistoryItem &item : m_history) {
        entries[item.keyCode]++;
    }

    const int minHits = 5;
    const int minKeys = 4;

    int hitKeyCount = 0;
    for (const auto &[key, hitCount] : entries) {
        if (hitCount >= minHits) {
            hitKeyCount++;
            if (hitKeyCount >= minKeys) {
                m_history.clear();
                return true;
            }
        }
    }

    return false;
}

} // namespace KWin
