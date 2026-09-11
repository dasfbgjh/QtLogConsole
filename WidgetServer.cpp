#include "WidgetServer.h"
#include "ui_WidgetServer.h"

WidgetServer::WidgetServer( QWidget *parent )
    : QWidget( parent ), ui( new Ui::WidgetServer ) {
    ui->setupUi( this );
    setWindowFlag( Qt::WindowStaysOnTopHint );
    setWindowTitle( "日志窗口" );

    clientModel = new QStandardItemModel( ui->clientList );
    ui->clientList->setModel( clientModel );

    server = new QTcpServer( this );
    connect( server, &QTcpServer::newConnection, this, &WidgetServer::dealNewConnection );
    // 断开客户端
    connect( ui->open, &QPushButton::clicked, this, &WidgetServer::openClient );
    // 清空日志
    connect( ui->clear, &QPushButton::clicked, ui->log, &QTextBrowser::clear );
    connect( ui->sysInfo, &QPushButton::clicked, this, &WidgetServer::systemInfo );
    connect( ui->send, &QPushButton::clicked, this, &WidgetServer::send );
    connect( ui->sendMessage, &QLineEdit::returnPressed, this, &WidgetServer::send );

    contextMenu = new QMenu( ui->log );
    actionSelectAll = contextMenu->addAction( "全选", this, [&]() {
        ui->log->selectAll();
    } );
    actionCopy = contextMenu->addAction( "复制", this, [&]() {
        ui->log->copy();
    } );
    connect( ui->log, &QTextBrowser::customContextMenuRequested, this, [&]( const QPoint & ) {
        contextMenu->exec( QCursor::pos() );
    } );

    server->listen();

    ui->address->addItems( ipv4() );
    ui->address->setCurrentText( "127.0.0.1" );

    ui->log->append( QString( "端口:%2\n" ).arg( server->serverPort() ) );
}

WidgetServer::~WidgetServer() {
    delete ui;
    server->close();
}

void WidgetServer::systemInfo() {
    QString sys = QString( "系统:%1\n版本:%2\nCPU架构:%3\n系统类型:%4\n构建:%5\n内核:%6 %7\n" ).arg( QSysInfo::prettyProductName(), QSysInfo::productVersion(), QSysInfo::currentCpuArchitecture(), QSysInfo::productType(), QSysInfo::buildAbi(), QSysInfo::kernelType(), QSysInfo::kernelVersion() );
    ui->log->append( sys );

    QNetworkAccessManager net;
    QString schemes;
    foreach ( QString str, net.supportedSchemes() )
        schemes.append( str + " " );
    ui->log->append( "支持协议:" + schemes );
    ui->log->append( QString( "ssl支持:%1" ).arg( QSslSocket::supportsSsl() ? "支持" : "不支持" ) );
    ui->log->append( QString( "ssl版本:%1" ).arg( QSslSocket::sslLibraryVersionString() ) );
    ui->log->append( QString( "ssl构建版本:%1\n" ).arg( QSslSocket::sslLibraryBuildVersionString() ) );

    ui->log->append( QString( "连接端口:%2\n" ).arg( server->serverPort() ) );
}

void WidgetServer::openClient() {
    if ( ui->clientName->text().isEmpty() )
        return;
    QString path = QFileDialog::getOpenFileName( this, "选择目标程序", QString(), "程序 (*)" );
    if ( path.isEmpty() )
        return;
    QString argv = QString( "address=%1&port=%2&name=%3" )
                       .arg( ui->address->currentText() )
                       .arg( server->serverPort() )
                       .arg( ui->clientName->text() );

    Process *p = new Process( path, { argv }, !ui->environment->isChecked() );
    connect( p, &Process::newMessage, this, [&]( QString message ) {
        ui->log->append( message );
        ui->log->textCursor().movePosition( QTextCursor::End );
        QScrollBar *scrollBar = ui->log->verticalScrollBar();
        scrollBar->scroll( scrollBar->maximum(), 0 );
    } );
    connect(
        p, &Process::finish, this, [&]( Process *obj ) {
            delete obj;
        },
        Qt::QueuedConnection );

    p->start();
    ui->clientName->clear();
}

void WidgetServer::dealNewConnection() {
    QTcpSocket *socket = server->nextPendingConnection();
    if ( socket == NULL )
        return;
    TcpSocket *tcpSocket = new TcpSocket( socket, *this );

    // 客户端断开
    connect(
        tcpSocket, &TcpSocket::disconnected, this, [&]( TcpSocket *obj, QString id ) {
            socketList.removeAll( obj );
            QList<QStandardItem *> items = clientModel->findItems( id, Qt::MatchFixedString, 1 );
            if ( !items.isEmpty() )
                clientModel->removeRow( items.at( 0 )->row() );
            delete obj;
        },
        Qt::QueuedConnection );
    // 客户端消息
    connect( tcpSocket, &TcpSocket::newMessage, this, [&]( QString message ) {
        ui->log->append( message );
        ui->log->textCursor().movePosition( QTextCursor::End );
        QScrollBar *scrollBar = ui->log->verticalScrollBar();
        scrollBar->scroll( scrollBar->maximum(), 0 );
    } );
    // 客户端连接
    connect( tcpSocket, &TcpSocket::newClient, this, [&]( QString id, QString name ) {
        QList<QStandardItem *> items = clientModel->findItems( id, Qt::MatchFixedString, 1 );
        if ( !items.isEmpty() )
            return;
        items << new QStandardItem( name ) << new QStandardItem( id );
        clientModel->appendRow( items );
    } );
    socketList << tcpSocket;
}

void WidgetServer::send() {
    if ( ui->sendMessage->text().isEmpty() )
        return;
    QModelIndex index = clientModel->index( ui->clientList->currentIndex(), 1 );
    QString id = clientModel->data( index ).toString();
    emit sendMessage( id, ui->sendMessage->text() );
    ui->sendMessage->clear();
}

QStringList WidgetServer::ipv4() {
    QStringList address;
    foreach ( QNetworkInterface net, QNetworkInterface::allInterfaces() ) {
        bool ok = net.flags().testFlag( QNetworkInterface::IsUp ) &&      // 激活
                  net.flags().testFlag( QNetworkInterface::IsRunning ) && // 运行
                  !net.flags().testFlag( QNetworkInterface::IsLoopBack ); // 不是回环地址
        if ( !ok )
            continue;
        ok = net.humanReadableName().contains( "WLAN", Qt::CaseInsensitive ) ||
             net.humanReadableName().contains( "以太网" ) ||
             net.type() == QNetworkInterface::Wifi ||
             net.type() == QNetworkInterface::Ethernet;
        if ( !ok )
            continue;
        foreach ( QHostAddress hostAddress, net.allAddresses() ) {
            if ( hostAddress.protocol() == QAbstractSocket::IPv4Protocol )
                address << hostAddress.toString();
        }
        break;
    }
    return address;
}

/**********************************************/
WidgetServer::Process::Process( const QString &path, const QStringList &argv, bool useSystemEnvironment, QWidget *parent )
    : QObject{ parent } {
    process = new QProcess( this );
    process->setProgram( path );
    process->setArguments( argv );
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert( "LD_LIBRARY_PATH", env.value( "LD_LIBRARY_PATH" ) + ":" + QFileInfo( path ).absolutePath() );
    if ( !useSystemEnvironment ) {
        env.remove( "LD_LIBRARY_PATH" );
        env.remove( "PATH" );
    }
    process->setProcessEnvironment( env );

    connect( process, &QProcess::readyReadStandardOutput, this, [&]() {
        emit newMessage( "processMessage:" + process->readAllStandardOutput() );
    } );
    connect( process, &QProcess::readyReadStandardError, this, [&]() {
        emit newMessage( "processError:" + process->readAllStandardError() );
    } );
    connect( process, &QProcess::stateChanged, this, [&]( QProcess::ProcessState newState ) {
        switch ( newState ) {
        case QProcess::NotRunning:
            emit newMessage( "processState:未运行" );
            break;
        case QProcess::Starting:
            emit newMessage( "processState:启动" );
            break;
        case QProcess::Running:
            emit newMessage( "processState:运行" );
            break;
        default:
            break;
        }
    } );
    connect(
        process, QOverload<int, QProcess::ExitStatus>::of( &QProcess::finished ), this,
        [&]( int, QProcess::ExitStatus ) {
            delete process;
            emit finish( this );
        },
        Qt::QueuedConnection );
}

void WidgetServer::Process::start() {
    process->start();
}

/**********************************************/
WidgetServer::TcpSocket::TcpSocket( QTcpSocket *socket, WidgetServer &parent )
    : tcpSocket( socket ) {
    connect( &parent, &WidgetServer::sendMessage, this, &TcpSocket::sendMessage );
    connect( socket, &QTcpSocket::readyRead, this, &TcpSocket::readMessage );
    connect( socket, &QTcpSocket::disconnected, this, [&]() {
        emit disconnected( this, id );
    } );
}

void WidgetServer::TcpSocket::readMessage() {
    QString data = tcpSocket->readAll();
    if ( id.isEmpty() ) {
        id = QUuid::createUuid().toString();
        if ( data.startsWith( "name=" ) ) {
            name = data.mid( 5 );
            emit newClient( id, name );
            name.append( ": " );
        } else {
            name = id;
            emit newClient( id, name );
            name.append( ": " );
            emit newMessage( name + data + "\n" );
        }
    } else {
        emit newMessage( name + data + "\n" );
    }
}

void WidgetServer::TcpSocket::sendMessage( QString id, QString message ) {
    if ( id != this->id )
        return;
    tcpSocket->write( message.toUtf8() );
}
