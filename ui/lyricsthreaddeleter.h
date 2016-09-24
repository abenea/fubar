#include <QObject>

class QThread;

class LyricsThreadDeleter : public QObject {
    Q_OBJECT
public:
    LyricsThreadDeleter(QThread *thread);

public slots:
    void cleanup();

private:
    QThread *thread_;
};
