/*
    Copyright (c) 2009 Kevin Ottens <ervin@kde.org>
    Copyright (c) 2017 Christian Mollekopf <mollekopf@kolabsys.com>

    This library is free software; you can redistribute it and/or modify it
    under the terms of the GNU Library General Public License as published by
    the Free Software Foundation; either version 2 of the License, or (at your
    option) any later version.

    This library is distributed in the hope that it will be useful, but WITHOUT
    ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
    FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Library General Public
    License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to the
    Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
    02110-1301, USA.
*/

#ifndef KIMAP2_SESSION_P_H
#define KIMAP2_SESSION_P_H

#include "session.h"

#include <QtNetwork/QSslSocket>

#include <QtCore/QObject>
#include <QtCore/QQueue>
#include <QtCore/QString>
#include <QtCore/QTimer>
#include <QtCore/QTime>

class KJob;

namespace KIMAP2
{

class Job;
struct Message;
class SessionLogger;
class ImapStreamParser;

class KIMAP2_EXPORT SessionPrivate : public QObject
{
    Q_OBJECT

    friend class Session;

public:
    explicit SessionPrivate(Session *session);
    virtual ~SessionPrivate();

    void addJob(Job *job);
    QByteArray sendCommand(const QByteArray &command, const QByteArray &args = QByteArray());
    void startSsl(QSsl::SslProtocol version);
    void sendData(const QByteArray &data);

    void setSocketTimeout(int ms);
    int socketTimeout() const;

Q_SIGNALS:
    void encryptionNegotiationResult(bool);

private Q_SLOTS:
    void onSocketTimeout();
    void onSocketProgressTimeout();

    void doStartNext();
    void jobDone(KJob *);
    void jobDestroyed(QObject *);

    void socketConnected();
    void socketDisconnected();
    void socketError(QAbstractSocket::SocketError);
    void socketActivity();
    void handleSslErrors(const QList<QSslError> &errors);

    void closeSocket();
    void readMessage();
    void writeDataQueue();
    void sslConnected();

private:
    void responseReceived(const KIMAP2::Message &);
    void startNext();
    void clearJobQueue();
    void setState(Session::State state);

    void startSocketTimer();
    void stopSocketTimer();
    void restartSocketTimer();
    QString getStateName() const;

    Session *const q;

    bool isSocketConnected;
    Session::State state;
    bool hostLookupInProgress;

    QScopedPointer<SessionLogger> logger;

    bool jobRunning;
    Job *currentJob;
    QQueue<Job *> queue;

    QByteArray authTag;
    QByteArray selectTag;
    QByteArray closeTag;

    QString userName;
    QByteArray greeting;
    QByteArray currentMailBox;
    QByteArray upcomingMailBox;
    quint16 tagCount;

    int socketTimerInterval;
    QTimer socketTimer;
    int socketProgressInterval;
    QTimer socketProgressTimer;

    QString hostName;
    quint16 port;

    QScopedPointer<QSslSocket> socket;
    QScopedPointer<ImapStreamParser> stream;

    QQueue<QByteArray> dataQueue;

    QTime time;
    qint64 accumulatedWaitTime;
    qint64 accumulatedProcessingTime;
    bool trackTime;
    bool dumpTraffic;
};

}

#endif
