#ifndef WIDGETSERVER_H
#define WIDGETSERVER_H

#include <QWidget>
#include <QFileDialog>
#include <QProcess>
#include <QTcpServer>
#include <QTcpSocket>
#include <QDateTime>
#include <QNetworkInterface>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QHostInfo>
#include <QStandardItemModel>
#include <QUuid>
#include <QMenu>
#include <QScrollBar>

QT_BEGIN_NAMESPACE
namespace Ui {
class WidgetServer;
}
QT_END_NAMESPACE

class WidgetServer : public QWidget {
    Q_OBJECT
public:
    WidgetServer( QWidget *parent = nullptr );
    ~WidgetServer();

private:
    void systemInfo();
    void openClient();
    void dealNewConnection();
    void send();
    QStringList ipv4();

    class TcpSocket;
    class Process;
    Ui::WidgetServer *ui;

    QTcpServer *server;
    QList<TcpSocket *> socketList;
    QStandardItemModel *clientModel;

    QMenu *contextMenu;
    QAction *actionSelectAll;
    QAction *actionCopy;

signals:
    void sendMessage( QString id, QString message );
};

/**********************************************/
class WidgetServer::Process : public QObject {
    Q_OBJECT
public:
    Process( const QString &path, const QStringList &argv, bool useSystemEnvironment, QWidget *parent = nullptr );
    void start();

private:
    QProcess *process;

signals:
    void newMessage( QString message );
    void finish( Process *obj );
};

/**********************************************/
class WidgetServer::TcpSocket : public QObject {
    Q_OBJECT
public:
    TcpSocket( QTcpSocket *socket, WidgetServer &parent );

private:
    void readMessage();
    void sendMessage( QString id, QString message );

    QTcpSocket *const tcpSocket;
    QString id;
    QString name;

signals:
    void disconnected( TcpSocket *obj, QString id );
    void newMessage( QString message );
    void newClient( QString id, QString name );
};
#endif // WIDGETSERVER_H
