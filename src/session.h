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

#ifndef KIMAP2_SESSION_H
#define KIMAP2_SESSION_H

#include "kimap2_export.h"

#include <QtCore/QObject>
#include <QtNetwork/QSsl>
#include <QtNetwork/QSslSocket>

namespace KIMAP2
{

class SessionPrivate;
class JobPrivate;
struct Message;

class KIMAP2_EXPORT Session : public QObject
{
    Q_OBJECT
    Q_ENUMS(State)

    friend class JobPrivate;

public:
    enum State { Disconnected = 0, NotAuthenticated, Authenticated, Selected };

    Session(const QString &hostName, quint16 port, QObject *parent = Q_NULLPTR);
    ~Session();

    QString hostName() const;
    quint16 port() const;
    State state() const;

    /**
     * Returns the name that has been set with LoginJob::setUserName()
     * The user name is useful to uniquely identify an IMAP resource, in combination with the host name
     * @note If the Session was pre-authenticated, userName() will return an empty string
     */
    QString userName() const;

    QByteArray serverGreeting() const;

    void setErrorHandler();

    /**
     * Set the session timeout. The default is 30 seconds.
     * @param timeout The socket timeout in seconds, negative values disable the timeout.
     */
    void setTimeout(int timeout);

    /**
     * Returns the session timeout.
     */
    int timeout() const;

    /**
     * Returns the currently selected mailbox.
     */
    QString selectedMailBox() const;

    int jobQueueSize() const;

    void close();
    void ignoreErrors(const QList<QSslError> &errors);

Q_SIGNALS:
    void jobQueueSizeChanged(int queueSize);
    void sslErrors(const QList<QSslError> &errors);

    /**
      Emitted when the Session couldn't connect to the host.

      Likely reasons: invalid host address, no internet connectivity, firewall blocking rules,
      etc...

      Pending jobs in the queue will be deleted, and the first job in the queue will be failed. (ie:
      it will have its result signal emitted with a non-zero error code.)
    */
    void connectionFailed();

    /**
      Emitted when the session's state changes.

      You can use this signal to detect a connection loss (ie: stateChanged is emitted with newState
      == KIMAP2::Session::Disconnected)

      If you want to receive the stateChanged arguments in your slot, you must register the State
      enum with @c Q_DECLARE_METATYPE(KIMAP2::Session::State) and @c qRegisterMetaType<KIMAP2::Session::State>();
    */
    void stateChanged(KIMAP2::Session::State newState, KIMAP2::Session::State oldState);

private:
    friend class SessionPrivate;
    SessionPrivate *const d;
};

}

#endif
