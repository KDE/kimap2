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

#include "namespacejob.h"

#include "kimap_debug.h"

#include "job_p.h"
#include "listjob.h"
#include "message_p.h"
#include "rfccodecs.h"
#include "session_p.h"

static QList<QByteArray> parseParenthesizedList(const QByteArray &data)
{
    QList<QByteArray> result;
    if (data.isEmpty()) {
        return result;
    }
    if (data.at(0) != '(') {
        return result;
    }
    bool parsingString = false;
    int start = 0;
    for (int pos = 1; pos < data.size(); pos++) {
        //Match unescaped quotes
        if (data.at(pos) == '\"' && data.at(pos - 1) != '\\') {
            if (parsingString) {
                parsingString = false;
                result << data.mid(start, pos - start);
            } else {
                parsingString = true;
                start = pos + 1;
            }
        }
    }
    return result;
}

namespace KIMAP2
{
class NamespaceJobPrivate : public JobPrivate
{
public:
    NamespaceJobPrivate(Session *session,  const QString &name) : JobPrivate(session, name) { }
    ~NamespaceJobPrivate() { }

    QList<MailBoxDescriptor> processNamespaceList(const QList<QByteArray> &namespaceList)
    {
        QList<MailBoxDescriptor> result;

        foreach (const QByteArray &namespaceItem, namespaceList) {
            QList<QByteArray> parts = parseParenthesizedList(namespaceItem);
            if (parts.size() < 2) {
                qWarning() << "Not enough parts in namespace item " << namespaceItem;
                continue;
            }
            MailBoxDescriptor descriptor;
            descriptor.name = QString::fromUtf8(decodeImapFolderName(parts[0]));
            descriptor.separator = QLatin1Char(parts[1][0]);

            result << descriptor;
        }

        return result;
    }

    QList<MailBoxDescriptor> personalNamespaces;
    QList<MailBoxDescriptor> userNamespaces;
    QList<MailBoxDescriptor> sharedNamespaces;
};
}

using namespace KIMAP2;

NamespaceJob::NamespaceJob(Session *session)
    : Job(*new NamespaceJobPrivate(session, "Namespace"))
{
}

NamespaceJob::~NamespaceJob()
{
}

QList<MailBoxDescriptor> NamespaceJob::personalNamespaces() const
{
    Q_D(const NamespaceJob);
    return d->personalNamespaces;
}

QList<MailBoxDescriptor> NamespaceJob::userNamespaces() const
{
    Q_D(const NamespaceJob);
    return d->userNamespaces;
}

QList<MailBoxDescriptor> NamespaceJob::sharedNamespaces() const
{
    Q_D(const NamespaceJob);
    return d->sharedNamespaces;
}

bool NamespaceJob::containsEmptyNamespace() const
{
    Q_D(const NamespaceJob);
    QList<MailBoxDescriptor> completeList = d->personalNamespaces
                                            + d->userNamespaces
                                            + d->sharedNamespaces;

    foreach (const MailBoxDescriptor &descriptor, completeList) {
        if (descriptor.name.isEmpty()) {
            return true;
        }
    }

    return false;
}

void NamespaceJob::doStart()
{
    Q_D(NamespaceJob);
    d->sendCommand("NAMESPACE", {});
}

void NamespaceJob::handleResponse(const Message &response)
{
    Q_D(NamespaceJob);
    if (handleErrorReplies(response) == NotHandled) {
        if (response.content.size() >= 5 &&
                response.content[1].toString() == "NAMESPACE") {
            // Personal namespaces
            d->personalNamespaces = d->processNamespaceList(response.content[2].toList());

            // User namespaces
            d->userNamespaces = d->processNamespaceList(response.content[3].toList());

            // Shared namespaces
            d->sharedNamespaces = d->processNamespaceList(response.content[4].toList());
        }
    }
}
