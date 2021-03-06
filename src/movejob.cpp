/*
    Copyright (c) 2016 Daniel Vrátil <dvratil@kde.org>

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

#include "movejob.h"

#include "job_p.h"
#include "message_p.h"
#include "session_p.h"
#include "rfccodecs.h"

//TODO: when custom error codes are introduced, handle the NO [TRYCREATE] response

namespace KIMAP2
{
class MoveJobPrivate : public JobPrivate
{
public:
    MoveJobPrivate(Session *session, const QString &name) 
        : JobPrivate(session, name)
        , uidBased(false)
    {}

    ~MoveJobPrivate()
    {}

    QString mailBox;
    ImapSet set;
    bool uidBased;
    ImapSet resultingUids;
};
}

using namespace KIMAP2;

MoveJob::MoveJob(Session *session)
    : Job(*new MoveJobPrivate(session, "Move"))
{
    Q_D(MoveJob);
    d->uidBased = false;
}

MoveJob::~MoveJob()
{
}

void MoveJob::setMailBox(const QString &mailBox)
{
    Q_D(MoveJob);
    d->mailBox = mailBox;
}

QString MoveJob::mailBox() const
{
    Q_D(const MoveJob);
    return d->mailBox;
}

void MoveJob::setSequenceSet(const ImapSet &set)
{
    Q_D(MoveJob);
    d->set = set;
}

ImapSet MoveJob::sequenceSet() const
{
    Q_D(const MoveJob);
    return d->set;
}

void MoveJob::setUidBased(bool uidBased)
{
    Q_D(MoveJob);
    d->uidBased = uidBased;
}

bool MoveJob::isUidBased() const
{
    Q_D(const MoveJob);
    return d->uidBased;
}

ImapSet MoveJob::resultingUids() const
{
    Q_D(const MoveJob);
    return d->resultingUids;
}

void MoveJob::doStart()
{
    Q_D(MoveJob);

    d->set.optimize();
    QByteArray parameters = d->set.toImapSequenceSet() + ' ';
    parameters += '\"' + KIMAP2::encodeImapFolderName(d->mailBox.toUtf8()) + '\"';

    QByteArray command = "MOVE";
    if (d->uidBased) {
        command = "UID " + command;
    }

    d->sendCommand(command, parameters);
}

void MoveJob::handleResponse(const Message &response)
{
    Q_D(MoveJob);

    for (QList<Message::Part>::ConstIterator it = response.responseCode.begin();
            it != response.responseCode.end(); ++it) {
        if (it->toString() == "COPYUID") {
            it = it + 3;
            if (it < response.responseCode.end()) {
                d->resultingUids = ImapSet::fromImapSequenceSet(it->toString());
            }
            break;
        }
    }

    handleErrorReplies(response);
}
