#include <QObject>
#include <QRect>

class InternalInputContext : public QObject
{
    Q_OBJECT
public:
    InternalInputContext();
    ~InternalInputContext() override;

    // from QPA to kwin, update the member vars
    void reset();
    void commit();
    void updateState(Qt::InputMethodQueries queries, uint32_t flags);

    bool isInputPanelVisible() const;
    QRectF keyboardRect() const;

    QLocale locale() const;
    Qt::LayoutDirection inputDirection() const;

    // from kwin to -> QPA
    void setPreeditText(const QString &text, int cursorBegin, int cursorEnd);
    void commitText(const QString &text);
    void deleteSurroundingText(uint32_t beforeLength, uint32_t afterLength);

private:
    struct PreeditInfo
    {
        QString text;
        int cursorBegin = 0;
        int cursorEnd = 0;

        void clear()
        {
            text.clear();
            cursorBegin = 0;
            cursorEnd = 0;
        }
    };

    PreeditInfo m_pendingPreeditString;
    PreeditInfo m_currentPreeditString;
    QString m_pendingCommitString;
    uint m_pendingDeleteBeforeText = 0; // byte length
    uint m_pendingDeleteAfterText = 0; // byte length

    QString m_surroundingText;
    int m_cursor; // cursor position in QString
    int m_cursorPos; // cursor position in wayland index
    int m_anchorPos; // anchor position in wayland index
    uint32_t m_contentHint = 0;
    uint32_t m_contentPurpose = 0;
    QRect m_cursorRect;

    uint m_currentSerial = 0;

    bool m_condReselection = false;
};
