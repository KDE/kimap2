/*
    Copyright (c) 2009 Kevin Ottens <ervin@kde.org>

    Copyright (c) 2010 Klarälvdalens Datakonsult AB, a KDAB Group company <info@kdab.com>
    Author: Kevin Ottens <kevin@kdab.com>

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

#include "session.h"
#include "session_p.h"

#include <QtCore/QDebug>
#include <QtCore/QTimer>

#include "kimap_debug.h"

#include "job.h"
#include "loginjob.h"
#include "message_p.h"
#include "sessionlogger_p.h"
#include "rfccodecs.h"
#include "imapstreamparser.h"

Q_DECLARE_METATYPE(QSsl::SslProtocol)
Q_DECLARE_METATYPE(QSslSocket::SslMode)
static const int _kimap_sslVersionId = qRegisterMetaType<QSsl::SslProtocol>();

using namespace KIMAP2;

Session::Session(const QString &hostName, quint16 port, QObject *parent)
    : QObject(parent), d(new SessionPrivate(this))
{
    if (!qEnvironmentVariableIsEmpty("KIMAP2_LOGFILE")) {
        d->logger = QSharedPointer<SessionLogger>::create();
    }

    d->isSocketConnected = false;
    d->state = Disconnected;
    d->jobRunning = false;
    d->hostName =hostName;
    d->port = port;

    d->socket = QSharedPointer<QSslSocket>::create();
    d->stream = QSharedPointer<ImapStreamParser>::create(d->socket.data());

    connect(d->socket.data(), &QIODevice::readyRead,
            d, &SessionPrivate::readMessage, Qt::QueuedConnection);

    // Delay the call to socketDisconnected so that it finishes disconnecting before we call reconnect()
    connect(d->socket.data(), &QSslSocket::disconnected,
            d, &SessionPrivate::socketDisconnected, Qt::QueuedConnection);
    connect(d->socket.data(), &QSslSocket::connected,
            d, &SessionPrivate::socketConnected);
    connect(d->socket.data(), static_cast<void (QSslSocket::*)(const QList<QSslError>&)>(&QSslSocket::sslErrors),
            d, &SessionPrivate::handleSslErrors);
    connect(d->socket.data(), static_cast<void (QSslSocket::*)(QAbstractSocket::SocketError)>(&QSslSocket::error),
            d, &SessionPrivate::socketError);

    connect(d->socket.data(), &QIODevice::bytesWritten,
            d, &SessionPrivate::socketActivity);
    connect(d->socket.data(), &QSslSocket::encryptedBytesWritten,
            d, &SessionPrivate::socketActivity);
    connect(d->socket.data(), &QIODevice::readyRead,
            d, &SessionPrivate::socketActivity);

    d->socketTimer.setSingleShot(true);
    connect(&d->socketTimer, &QTimer::timeout,
            d, &SessionPrivate::onSocketTimeout);

    d->startSocketTimer();

    QMetaObject::invokeMethod(d, "reconnect", Qt::QueuedConnection);
}

Session::~Session()
{
    //Make sure all jobs know we're done
    d->socketDisconnected();
    delete d;
}

QString Session::hostName() const
{
    return d->hostName;
}

quint16 Session::port() const
{
    return d->port;
}

Session::State Session::state() const
{
    return d->state;
}

QString Session::userName() const
{
    return d->userName;
}

QByteArray Session::serverGreeting() const
{
    return d->greeting;
}

int Session::jobQueueSize() const
{
    return d->queue.size() + (d->jobRunning ? 1 : 0);
}

void Session::close()
{
    d->closeSocket();
}

void Session::ignoreErrors(const QList<QSslError> &errors)
{
    d->socket->ignoreSslErrors(errors);
}

void Session::setTimeout(int timeout)
{
    d->setSocketTimeout(timeout * 1000);
}

int Session::timeout() const
{
    return d->socketTimeout() / 1000;
}

QString Session::selectedMailBox() const
{
    return QString::fromUtf8(d->currentMailBox);
}


SessionPrivate::SessionPrivate(Session *session)
    : QObject(session),
      q(session),
      state(Session::Disconnected),
      logger(Q_NULLPTR),
      currentJob(Q_NULLPTR),
      tagCount(0),
      socketTimerInterval(30000)   // By default timeouts on 30s
{
}

SessionPrivate::~SessionPrivate()
{
}

void SessionPrivate::handleSslErrors(const QList<QSslError> &errors)
{
    emit q->sslErrors(errors);
}

void SessionPrivate::addJob(Job *job)
{
    queue.append(job);
    emit q->jobQueueSizeChanged(q->jobQueueSize());

    QObject::connect(job, &KJob::result, this, &SessionPrivate::jobDone);
    QObject::connect(job, &QObject::destroyed, this, &SessionPrivate::jobDestroyed);

    if (state != Session::Disconnected) {
        startNext();
    }
}

void SessionPrivate::startNext()
{
    QMetaObject::invokeMethod(this, "doStartNext");
}

void SessionPrivate::doStartNext()
{
    if (queue.isEmpty() || jobRunning || !isSocketConnected) {
        return;
    }

    restartSocketTimer();
    jobRunning = true;

    currentJob = queue.dequeue();
    currentJob->doStart();
}

void SessionPrivate::jobDone(KJob *job)
{
    Q_UNUSED(job);
    Q_ASSERT(job == currentJob);

    stopSocketTimer();

    jobRunning = false;
    currentJob = Q_NULLPTR;
    emit q->jobQueueSizeChanged(q->jobQueueSize());
    startNext();
}

void SessionPrivate::jobDestroyed(QObject *job)
{
    queue.removeAll(static_cast<KIMAP2::Job *>(job));
    if (currentJob == job) {
        currentJob = Q_NULLPTR;
    }
}

void SessionPrivate::responseReceived(const Message &response)
{
    if (logger && (state == Session::Authenticated || state == Session::Selected)) {
        logger->dataReceived(response.toString());
    }

    QByteArray tag;
    QByteArray code;

    if (response.content.size() >= 1) {
        tag = response.content[0].toString();
    }

    if (response.content.size() >= 2) {
        code = response.content[1].toString();
    }

    // BYE may arrive as part of a LOGOUT sequence or before the server closes the connection after an error.
    // In any case we should wait until the server closes the connection, so we don't have to do anything.
    if (code == "BYE") {
        Message simplified = response;
        if (simplified.content.size() >= 2) {
            simplified.content.removeFirst(); // Strip the tag
            simplified.content.removeFirst(); // Strip the code
        }
        qCDebug(KIMAP2_LOG) << "Received BYE: " << simplified.toString();
        return;
    }

    switch (state) {
    case Session::Disconnected:
        if (socketTimer.isActive()) {
            stopSocketTimer();
        }
        if (code == "OK") {
            setState(Session::NotAuthenticated);

            Message simplified = response;
            simplified.content.removeFirst(); // Strip the tag
            simplified.content.removeFirst(); // Strip the code
            greeting = simplified.toString().trimmed(); // Save the server greeting

            startNext();
        } else if (code == "PREAUTH") {
            setState(Session::Authenticated);

            Message simplified = response;
            simplified.content.removeFirst(); // Strip the tag
            simplified.content.removeFirst(); // Strip the code
            greeting = simplified.toString().trimmed(); // Save the server greeting

            startNext();
        } else {
            closeSocket();
        }
        return;
    case Session::NotAuthenticated:
        if (code == "OK" && tag == authTag) {
            setState(Session::Authenticated);
        }
        break;
    case Session::Authenticated:
        if (code == "OK" && tag == selectTag) {
            setState(Session::Selected);
            currentMailBox = upcomingMailBox;
        }
        break;
    case Session::Selected:
        if ((code == "OK" && tag == closeTag) ||
                (code != "OK" && tag == selectTag)) {
            setState(Session::Authenticated);
            currentMailBox = QByteArray();
        } else if (code == "OK" && tag == selectTag) {
            currentMailBox = upcomingMailBox;
        }
        break;
    }

    if (tag == authTag) {
        authTag.clear();
    }
    if (tag == selectTag) {
        selectTag.clear();
    }
    if (tag == closeTag) {
        closeTag.clear();
    }

    // If a job is running forward it the response
    if (currentJob != Q_NULLPTR) {
        restartSocketTimer();
        currentJob->handleResponse(response);
    } else {
        qCWarning(KIMAP2_LOG) << "A message was received from the server with no job to handle it:"
                             << response.toString()
                             << '(' + response.toString().toHex() + ')';
    }
}

void SessionPrivate::setState(Session::State s)
{
    if (s != state) {
        Session::State oldState = state;
        state = s;
        emit q->stateChanged(state, oldState);
    }
}

QByteArray SessionPrivate::sendCommand(const QByteArray &command, const QByteArray &args)
{
    QByteArray tag = 'A' + QByteArray::number(++tagCount).rightJustified(6, '0');

    QByteArray payload = tag + ' ' + command;
    if (!args.isEmpty()) {
        payload += ' ' + args;
    }

    sendData(payload);

    if (command == "LOGIN" || command == "AUTHENTICATE") {
        authTag = tag;
    } else if (command == "SELECT" || command == "EXAMINE") {
        selectTag = tag;
        upcomingMailBox = args;
        upcomingMailBox.remove(0, 1);
        upcomingMailBox = upcomingMailBox.left(upcomingMailBox.indexOf('\"'));
        upcomingMailBox = KIMAP2::decodeImapFolderName(upcomingMailBox);
    } else if (command == "CLOSE") {
        closeTag = tag;
    }
    return tag;
}

void SessionPrivate::sendData(const QByteArray &data)
{
    restartSocketTimer();

    if (logger && (state == Session::Authenticated || state == Session::Selected)) {
        logger->dataSent(data);
    }

    dataQueue.enqueue(data + "\r\n");
    QMetaObject::invokeMethod(this, "writeDataQueue");
}

void SessionPrivate::socketConnected()
{
    qWarning() << "Socket connected";
    stopSocketTimer();
    isSocketConnected = true;

    bool willUseSsl = false;
    if (!queue.isEmpty()) {
        KIMAP2::LoginJob *login = qobject_cast<KIMAP2::LoginJob *>(queue.first());
        if (login) {
            qWarning() << "got a login job";
            willUseSsl = login->encryptionMode() != QSsl::UnknownProtocol;
            userName = login->userName();
        }
    }

    if (state == Session::Disconnected && willUseSsl) {
        startNext();
    } else {
        startSocketTimer();
    }
}

void SessionPrivate::socketDisconnected()
{
    qWarning() << "Disconnected";
    if (!isSocketConnected) {
        return;
    }
    if (socketTimer.isActive()) {
        stopSocketTimer();
    }

    if (logger && (state == Session::Authenticated || state == Session::Selected)) {
        logger->disconnectionOccured();
    }

    if (state != Session::Disconnected) {
        setState(Session::Disconnected);
    } else {
        emit q->connectionFailed();
    }

    isSocketConnected = false;

    clearJobQueue();
}

void SessionPrivate::socketActivity()
{
    restartSocketTimer();
}

void SessionPrivate::socketError(QAbstractSocket::SocketError error)
{
    if (socketTimer.isActive()) {
        stopSocketTimer();
    }

    if (currentJob) {
        currentJob->setSocketError(error);
    } else if (!queue.isEmpty()) {
        currentJob = queue.takeFirst();
        currentJob->setSocketError(error);
    }

    if (isSocketConnected) {
        closeSocket();
    } else {
        emit q->connectionFailed();
        clearJobQueue();
    }
}

void SessionPrivate::clearJobQueue()
{
    if (currentJob) {
        currentJob->connectionLost();
    } else if (!queue.isEmpty()) {
        currentJob = queue.takeFirst();
        currentJob->connectionLost();
    }

    QQueue<Job *> queueCopy = queue; // copy because jobDestroyed calls removeAll
    qDeleteAll(queueCopy);
    queue.clear();
    emit q->jobQueueSizeChanged(0);
}

void SessionPrivate::startSsl(const QSsl::SslProtocol &version)
{
    Q_ASSERT(socket);
    if (!socket) {
        return;
    }

    connect(socket.data(), &QSslSocket::encrypted, this, &SessionPrivate::sslConnected);
    socket->startClientEncryption();
}

void SessionPrivate::sslConnected()
{
    emit encryptionNegotiationResult(true);
}

void SessionPrivate::setSocketTimeout(int ms)
{
    bool timerActive = socketTimer.isActive();

    if (timerActive) {
        stopSocketTimer();
    }

    socketTimerInterval = ms;

    if (timerActive) {
        startSocketTimer();
    }
}

int SessionPrivate::socketTimeout() const
{
    return socketTimerInterval;
}

void SessionPrivate::startSocketTimer()
{
    // qWarning() << "Starting socket timer: " << socketTimerInterval;
    if (socketTimerInterval < 0) {
        return;
    }
    Q_ASSERT(!socketTimer.isActive());

    socketTimer.start(socketTimerInterval);
}

void SessionPrivate::stopSocketTimer()
{
    if (socketTimerInterval < 0) {
        return;
    }

    socketTimer.stop();
}

void SessionPrivate::restartSocketTimer()
{
    if (socketTimer.isActive()) {
        stopSocketTimer();
    }
    startSocketTimer();
}

void SessionPrivate::onSocketTimeout()
{
    qWarning() << "Socket timeout!";
    socket->abort();
    socketDisconnected();
}

void SessionPrivate::writeDataQueue()
{
    if (!socket) {
        return;
    }

    while (!dataQueue.isEmpty()) {
        socket->write(dataQueue.dequeue());
    }
}

void SessionPrivate::readMessage()
{
    if (!stream || stream->availableDataSize() == 0) {
        return;
    }

    Message message;
    QList<Message::Part> *payload = &message.content;

    try {
        while (!stream->atCommandEnd()) {
            if (stream->hasString()) {
                QByteArray string = stream->readString();
                if (string == "NIL") {
                    *payload << Message::Part(QList<QByteArray>());
                } else {
                    *payload << Message::Part(string);
                }
            } else if (stream->hasList()) {
                *payload << Message::Part(stream->readParenthesizedList());
            } else if (stream->hasResponseCode()) {
                payload = &message.responseCode;
            } else if (stream->atResponseCodeEnd()) {
                payload = &message.content;
            } else if (stream->hasLiteral()) {
                QByteArray literal;
                while (!stream->atLiteralEnd()) {
                    literal += stream->readLiteralPart();
                }
                *payload << Message::Part(literal);
            } else {
                // Oops! Something really bad happened, we won't be able to recover
                // so close the socket immediately
                qWarning("Inconsistent state, probably due to some packet loss");
                doCloseSocket();
                return;
            }
        }

        responseReceived(message);

    } catch (KIMAP2::ImapParserException e) {
        qCWarning(KIMAP2_LOG) << "The stream parser raised an exception:" << e.what();
    }

    if (stream->availableDataSize() > 1) {
        QMetaObject::invokeMethod(this, "readMessage", Qt::QueuedConnection);
    }

}

void SessionPrivate::closeSocket()
{
    QMetaObject::invokeMethod(this, "doCloseSocket", Qt::QueuedConnection);
}

void SessionPrivate::doCloseSocket()
{
    if (!socket) {
        return;
    }
    qCDebug(KIMAP2_LOG) << "close";
    socket->close();
}

void SessionPrivate::reconnect()
{
    if (socket == Q_NULLPTR) { // threadQuit already called
        return;
    }
    if (socket->state() != QSslSocket::ConnectedState &&
        socket->state() != QSslSocket::ConnectingState) {
        socket->connectToHost(hostName, port);
    }
}

#include "moc_session.cpp"
#include "moc_session_p.cpp"
