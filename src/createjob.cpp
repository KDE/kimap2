/*
    Copyright (c) 2009 Andras Mantia <amantia@kde.org>

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

#include "createjob.h"

#include "job_p.h"
#include "message_p.h"
#include "rfccodecs.h"
#include "session_p.h"

namespace KIMAP2
{
class CreateJobPrivate : public JobPrivate
{
public:
    CreateJobPrivate(Session *session, const QString &name) : JobPrivate(session, name) { }
    ~CreateJobPrivate() { }

    QString mailBox;
};
}

using namespace KIMAP2;

CreateJob::CreateJob(Session *session)
    : Job(*new CreateJobPrivate(session, "Create"))
{
}

CreateJob::~CreateJob()
{
}

void CreateJob::doStart()
{
    Q_D(CreateJob);
    d->sendCommand("CREATE", '\"' + KIMAP2::encodeImapFolderName(d->mailBox.toUtf8()) + '\"');
}

void CreateJob::handleResponse(const Message &response)
{
    Q_D(CreateJob);

    if (!response.content.isEmpty() &&
        d->tags.contains(response.content.first().toString())) {
        if (response.content.size() >= 2 &&
            response.content[1].toString() == "NO") {
            for (auto it = response.responseCode.cbegin(), end = response.responseCode.cend();
                 it != end; ++it) {
                // ALREADYEXISTS can be considered a success during CREATE
                // cf. https://tools.ietf.org/html/rfc5530#section-3
                if (it->toString() == "ALREADYEXISTS") {
                    // Code copied from handleErrorReplies:
                    d->tags.removeAll(response.content.first().toString());
                    if (d->tags.isEmpty()) {   // Only emit result when the last command returned
                        emitResult();
                    }
                    return;
                }
            }
        }
    }

    handleErrorReplies(response);
}

void CreateJob::setMailBox(const QString &mailBox)
{
    Q_D(CreateJob);
    d->mailBox = mailBox;
}

QString CreateJob::mailBox() const
{
    Q_D(const CreateJob);
    return d->mailBox;
}
