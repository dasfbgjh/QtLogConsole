#ifndef CLIENT_H
#define CLIENT_H

#include <QApplication>
#include <QThread>
#include <QTcpSocket>
#include <QTimer>
#include <QTime>
#include <QMutex>

class Client : public QThread {
    Q_OBJECT
public:
    static void connectServer( QStringList argvs );
    static void close();
    static bool sendMessagge( QtMsgType type, const QMessageLogContext &context, const QString &msg );

protected:
    class RunTask;
    static Client *instace;

    Client( QString address, qint64 port, QString name );
    void run() override;

    QString address;
    qint64 port;
    QString name;

    QStringList messageList;
    QMutex mutex;
};

/*************************************************************/
class Client::RunTask : public QObject {
    Q_OBJECT
public:
    RunTask( Client &thread );

private:
    void dealDelay();
    Client &thread;
    QTcpSocket *tcpSocket;
    QTimer *delay;
};
#endif // CLIENT_H
