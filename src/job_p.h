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

#ifndef KIMAP2_JOB_P_H
#define KIMAP2_JOB_P_H

#include "session.h"
#include <QtNetwork/QAbstractSocket>

namespace KIMAP2
{

class SessionPrivate;

class JobPrivate
{
public:
    JobPrivate(Session *session, const QString &name) : m_session(session), m_socketError(QAbstractSocket::UnknownSocketError)
    {
        m_name = name;
    }
    virtual ~JobPrivate() { }

    inline SessionPrivate *sessionInternal()
    {
        return m_session->d;
    }

    inline const SessionPrivate *sessionInternal() const
    {
        return m_session->d;
    }

    void sendCommand(const QByteArray &command, const QByteArray &args);

    QList<QByteArray> tags;
    Session *m_session;
    QString m_name;
    QString m_errorMessage;
    QString m_currentCommand;
    QAbstractSocket::SocketError m_socketError;
};

}

#endif
