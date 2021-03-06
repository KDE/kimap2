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

#ifndef KIMAP2_JOB_H
#define KIMAP2_JOB_H

#include "kimap2_export.h"

#include <KJob>
#include <QtNetwork/QAbstractSocket>

namespace KIMAP2
{

class Session;
class SessionPrivate;
class JobPrivate;
struct Message;

enum ErrorCodes {
    ConnectionLost = KJob::UserDefinedError + 1,
    CommandFailed,
    CouldNotConnect,
    SslHandshakeFailed,
    HostNotFound,
    LoginFailed,
    LastError
};

class KIMAP2_EXPORT Job : public KJob
{
    Q_OBJECT
    Q_DECLARE_PRIVATE(Job)

    friend class SessionPrivate;

public:
    virtual ~Job();

    Session *session() const;

    void start() Q_DECL_OVERRIDE;

private:
    virtual void doStart() = 0;
    virtual void handleResponse(const Message &response);
    virtual void connectionLost();
    void setSocketError(QAbstractSocket::SocketError);
    void setErrorMessage(const QString &message);

protected:
    enum HandlerResponse {
        Handled = 0,
        NotHandled
    };

    HandlerResponse handleErrorReplies(const Message &response);

    explicit Job(Session *session);
    explicit Job(JobPrivate &dd);

    JobPrivate *const d_ptr;
};

}

#endif
