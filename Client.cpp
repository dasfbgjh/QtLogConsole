#include "Client.h"

Client *Client::instace = nullptr;

Client::Client( QString address, qint64 port, QString name ) {
    this->address = address;
    this->port = port;
    this->name = name;
}

void Client::connectServer( QStringList argvs ) {
    QString address;
    qint64 port = -1;
    QString name;
    bool isOk = false;
    foreach ( QString argv, argvs ) {
        bool addressOk = false;
        bool portOk = false;
        bool nameOk = false;
        foreach ( QString items, argv.split( "&" ) ) {
            QStringList map = items.split( "=" );
            if ( map.size() != 2 )
                break;
            if ( map.at( 0 ) == "address" ) {
                address = map.at( 1 );
                addressOk = true;
            } else if ( map.at( 0 ) == "port" ) {
                port = map.at( 1 ).toLongLong( &portOk );
            } else if ( map.at( 0 ) == "name" ) {
                name = map.at( 1 );
                nameOk = true;
            }
        }
        isOk = addressOk && portOk && nameOk;
        if ( isOk )
            break;
    }

    if ( isOk ) {
        if ( instace != nullptr ) {
            if ( instace->isRunning() )
                return;
            delete instace;
        }
        instace = new Client( address, port, name );
        instace->start();
    }
}

void Client::close() {
    if ( instace != nullptr ) {
        instace->quit();
        instace->wait( 50 );
        delete instace;
        instace = nullptr;
    }
}

bool Client::sendMessagge( QtMsgType type, const QMessageLogContext &context, const QString &msg ) {
    if ( instace == nullptr )
        return false;
    QString msgType;
    switch ( type ) {
    case QtDebugMsg:
        msgType = QString( "Debug:" );
        break;
    case QtInfoMsg:
        msgType = QString( "Info:" );
        break;
    case QtWarningMsg:
        msgType = QString( "Warning:" );
        break;
    case QtCriticalMsg:
        msgType = QString( "Error:" );
        break;
    case QtFatalMsg:
        msgType = QString( "Fatal:" );
        break;
    default:
        msgType = QString( " :" );
        break;
    }
    QMutexLocker locker( &instace->mutex );
    instace->messageList << QString( "%1 %2 %3 %4" )
                                .arg( QTime::currentTime().toString( "hh:mm:ss" ), msgType, msg, context.function );
    return true;
}

void Client::run() {
    RunTask task( *this );
    exec();
}

/*************************************************************/
Client::RunTask::RunTask( Client &thread )
    : thread( thread ) {
    tcpSocket = new QTcpSocket( this );

    delay = new QTimer( this );
    delay->setSingleShot( true );
    connect( delay, &QTimer::timeout, this, &RunTask::dealDelay );
    dealDelay();
}

void Client::RunTask::dealDelay() {
    if ( tcpSocket->state() == QAbstractSocket::ConnectedState ) {
        QString message;
        if ( true ) {
            QMutexLocker locker( &thread.mutex );
            if ( !thread.messageList.isEmpty() )
                message = thread.messageList.takeAt( 0 );
        }
        if ( !message.isEmpty() ) {
            tcpSocket->write( message.toUtf8() );
            tcpSocket->waitForBytesWritten();
        }
        delay->start( 100 );
    } else {
        tcpSocket->connectToHost( thread.address, thread.port );
        tcpSocket->waitForConnected();
        if ( tcpSocket->state() == QAbstractSocket::ConnectedState ) {
            tcpSocket->write( QString( "name=" + thread.name ).toUtf8() );
            tcpSocket->waitForBytesWritten();
            delay->start( 100 );
        } else
            delay->start( 2000 );
    }
}
