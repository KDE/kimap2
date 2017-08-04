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

#include "job.h"
#include "job_p.h"
#include "message_p.h"
#include "session_p.h"

#include "kimap_debug.h"

using namespace KIMAP2;

void JobPrivate::sendCommand(const QByteArray &command, const QByteArray &args)
{
    tags << sessionInternal()->sendCommand(command, args);
    m_command = command + "" + args;
}

Job::Job(Session *session)
    : KJob(session), d_ptr(new JobPrivate(session, "Job"))
{
}

Job::Job(JobPrivate &dd)
    : KJob(dd.m_session), d_ptr(&dd)
{
}

Job::~Job()
{
    delete d_ptr;
}

Session *Job::session() const
{
    Q_D(const Job);
    return d->m_session;
}

void Job::start()
{
    Q_D(Job);
    d->sessionInternal()->addJob(this);
}

void Job::handleResponse(const Message &response)
{
    handleErrorReplies(response);
}

void Job::connectionLost()
{
    Q_D(Job);
    setError(KJob::UserDefinedError);
    setErrorText("Connection to server lost: " + d->m_errorMessage);
    emitResult();
}

void Job::setSocketError(QAbstractSocket::SocketError error)
{
    Q_D(Job);
    d->m_socketError = error;
}

void Job::setErrorMessage(const QString &msg)
{
    Q_D(Job);
    d->m_errorMessage = msg;
}

Job::HandlerResponse Job::handleErrorReplies(const Message &response)
{
    Q_D(Job);
    // qCDebug(KIMAP2_LOG) << response.toString();

    if (!response.content.isEmpty() &&
            d->tags.contains(response.content.first().toString())) {
        if (response.content.size() < 2) {
            setErrorText(QString("%1 failed, malformed reply from the server.").arg(d->m_name));
        } else if (response.content[1].toString() != "OK") {
            setError(UserDefinedError);
            setErrorText(QString("%1 failed, server replied: %2.\n Sent command: %3").arg(d->m_name).arg(QLatin1String(response.toString().constData())).arg(QString(d->m_currentCommand)));
        }
        d->tags.removeAll(response.content.first().toString());
        if (d->tags.isEmpty()) {   // Only emit result when the last command returned
            emitResult();
        }
        return Handled;
    }

    return NotHandled;
}
