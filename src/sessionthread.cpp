/*
    Copyright (c) 2009 Kevin Ottens <ervin@kde.org>

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

#include "sessionthread_p.h"

#include <QtCore/QDebug>

#include "kimap_debug.h"

#include "imapstreamparser.h"
#include "message_p.h"

using namespace KIMAP;


SessionThread::SessionThread(const QString &hostName, quint16 port)
    : QObject(), m_hostName(hostName), m_port(port),
      m_socket(Q_NULLPTR), m_stream(nullptr)
{
    m_socket = new SessionSocket;
    m_stream = new ImapStreamParser(m_socket);
    connect(m_socket, &QIODevice::readyRead,
            this, &SessionThread::readMessage, Qt::QueuedConnection);

    // Delay the call to slotSocketDisconnected so that it finishes disconnecting before we call reconnect()
    connect(m_socket, &QSslSocket::disconnected,
            this, &SessionThread::slotSocketDisconnected, Qt::QueuedConnection);
    connect(m_socket, &QSslSocket::connected,
            this, &SessionThread::socketConnected);
    connect(m_socket, static_cast<void (QSslSocket::*)(const QList<QSslError>&)>(&QSslSocket::sslErrors),
            this, &SessionThread::slotSocketSslError);
    connect(m_socket, static_cast<void (QSslSocket::*)(QAbstractSocket::SocketError)>(&QSslSocket::error),
            this, &SessionThread::slotSocketError);
    connect(m_socket, &QIODevice::bytesWritten,
            this, &SessionThread::socketActivity);
    connect(m_socket, &QSslSocket::encryptedBytesWritten,
            this, &SessionThread::socketActivity);
    connect(m_socket, &QIODevice::readyRead,
            this, &SessionThread::socketActivity);

    QMetaObject::invokeMethod(this, "reconnect", Qt::QueuedConnection);
}

SessionThread::~SessionThread()
{
    delete m_stream;
    m_stream = Q_NULLPTR;
    delete m_socket;
    m_socket = Q_NULLPTR;
}

void SessionThread::sendData(const QByteArray &payload)
{
    m_dataQueue.enqueue(payload);
    QMetaObject::invokeMethod(this, "writeDataQueue");
}

void SessionThread::writeDataQueue()
{
    if (!m_socket) {
        return;
    }

    while (!m_dataQueue.isEmpty()) {
        m_socket->write(m_dataQueue.dequeue());
    }
}

void SessionThread::readMessage()
{
    if (!m_stream || m_stream->availableDataSize() == 0) {
        return;
    }

    Message message;
    QList<Message::Part> *payload = &message.content;

    try {
        while (!m_stream->atCommandEnd()) {
            if (m_stream->hasString()) {
                QByteArray string = m_stream->readString();
                if (string == "NIL") {
                    *payload << Message::Part(QList<QByteArray>());
                } else {
                    *payload << Message::Part(string);
                }
            } else if (m_stream->hasList()) {
                *payload << Message::Part(m_stream->readParenthesizedList());
            } else if (m_stream->hasResponseCode()) {
                payload = &message.responseCode;
            } else if (m_stream->atResponseCodeEnd()) {
                payload = &message.content;
            } else if (m_stream->hasLiteral()) {
                QByteArray literal;
                while (!m_stream->atLiteralEnd()) {
                    literal += m_stream->readLiteralPart();
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

        emit responseReceived(message);

    } catch (KIMAP::ImapParserException e) {
        qCWarning(KIMAP_LOG) << "The stream parser raised an exception:" << e.what();
    }

    if (m_stream->availableDataSize() > 1) {
        QMetaObject::invokeMethod(this, "readMessage", Qt::QueuedConnection);
    }

}

void SessionThread::abort()
{
    m_socket->abort();
}

void SessionThread::closeSocket()
{
    QMetaObject::invokeMethod(this, "doCloseSocket", Qt::QueuedConnection);
}

void SessionThread::doCloseSocket()
{
    if (!m_socket) {
        return;
    }
    qCDebug(KIMAP_LOG) << "close";
    m_socket->close();
}

void SessionThread::reconnect()
{
    if (m_socket == Q_NULLPTR) { // threadQuit already called
        return;
    }
    if (m_socket->state() != SessionSocket::ConnectedState &&
        m_socket->state() != SessionSocket::ConnectingState) {
        m_socket->connectToHost(m_hostName, m_port);
    }
}

void SessionThread::startSsl(QSsl::SslProtocol version)
{
    if (!m_socket) {
        return;
    }

    connect(m_socket, &QSslSocket::encrypted, this, &SessionThread::sslConnected);
    m_socket->startClientEncryption();
}

void SessionThread::slotSocketDisconnected()
{
    emit socketDisconnected();
}

void SessionThread::slotSocketSslError(const QList<QSslError> &errors)
{
    if (!m_socket) {
        return;
    }
    emit sslErrors(errors);
}

void SessionThread::ignoreErrors(const QList<QSslError> &errors)
{
    m_socket->ignoreSslErrors(errors);
}

void SessionThread::slotSocketError(QAbstractSocket::SocketError error)
{
    emit socketError(error);
}

void SessionThread::sslConnected()
{
    if (!m_socket) {
        return;
    }
    emit encryptionNegotiationResult(true);
}


#include "moc_sessionthread_p.cpp"
