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

#include "setmetadatajob.h"

#include "kimap_debug.h"

#include "metadatajobbase_p.h"
#include "message_p.h"
#include "session_p.h"
#include "rfccodecs.h"

namespace KIMAP2
{
class SetMetaDataJobPrivate : public MetaDataJobBasePrivate
{
public:
    SetMetaDataJobPrivate(Session *session, const QString &name) : MetaDataJobBasePrivate(session, name), metaDataErrors(Q_NULLPTR), maxAcceptedSize(-1) { }
    ~SetMetaDataJobPrivate() { }

    QMap<QByteArray, QByteArray> entries;
    QMap<QByteArray, QByteArray>::ConstIterator entriesIt;
    QByteArray entryName;
    SetMetaDataJob::MetaDataErrors metaDataErrors;
    qint64 maxAcceptedSize;
};
}

using namespace KIMAP2;

SetMetaDataJob::SetMetaDataJob(Session *session)
    : MetaDataJobBase(*new SetMetaDataJobPrivate(session, "SetMetaData"))
{
}

SetMetaDataJob::~SetMetaDataJob()
{
}

void SetMetaDataJob::doStart()
{
    Q_D(SetMetaDataJob);
    QByteArray parameters;
    parameters = '\"' + KIMAP2::encodeImapFolderName(d->mailBox.toUtf8()) + "\" ";
    d->entriesIt = d->entries.constBegin();

    QByteArray command = "SETMETADATA";
    bool bSimpleData = true;

    if (d->serverCapability == Annotatemore) {
        command = "SETANNOTATION";
        parameters += '\"' + d->entryName + "\" ";
    } else {
        for (; d->entriesIt != d->entries.constEnd(); ++d->entriesIt) {
            if (d->entriesIt.value().contains('\r') || d->entriesIt.value().contains('\n')) {
                bSimpleData = false;
                break;
            }
        }
        d->entriesIt = d->entries.constBegin();
    }

    parameters += '(';
    if (bSimpleData == true) {
        for ( ; d->entriesIt != d->entries.constEnd(); ++d->entriesIt ) {
            parameters += '\"' + d->entriesIt.key() + "\" ";
            if (d->entriesIt.value().isEmpty()) {
                parameters += "NIL";
            } else {
                parameters +=  "\"" + d->entriesIt.value() + "\"";
            }
            parameters += " ";

        }
        parameters[parameters.length() - 1] = ')';
    } else {
        if (!d->entries.isEmpty()) {
            parameters += '\"' + d->entriesIt.key() + "\"";
            int size = d->entriesIt.value().size();
            parameters += " {" + QByteArray::number( size==0 ? 3 : size ) + '}';
        }
    }

    if (d->entries.isEmpty()) {
        parameters += ')';
    }

    d->sendCommand(command, parameters);
//   qCDebug(KIMAP2_LOG) << "SENT: " << command << " " << parameters;
}

void SetMetaDataJob::handleResponse(const Message &response)
{
    Q_D(SetMetaDataJob);

    //TODO: Test if a server can really return more then one untagged NO response. If not, no need to OR the error codes
    if (!response.content.isEmpty() &&
        d->tags.contains(response.content.first().toString())) {
        if (response.content[1].toString() == "NO") {
            setError(UserDefinedError);
            setErrorText(QString("%1 failed, server replied: %2").arg(d->m_name).arg(QLatin1String(response.toString().constData())));
            if (response.content[2].toString() == "[ANNOTATEMORE TOOMANY]" ||
                    response.content[2].toString() == "[METADATA TOOMANY]") {
                d->metaDataErrors |= TooMany;
            } else if (response.content[2].toString() == "[ANNOTATEMORE TOOBIG]" ||
                       response.content[2].toString().startsWith("[METADATA MAXSIZE")) {    //krazy:exclude=strings
                d->metaDataErrors |= TooBig;
                d->maxAcceptedSize = -1;
                if (response.content[2].toString().startsWith("[METADATA MAXSIZE")) {     //krazy:exclude=strings
                    QByteArray max = response.content[2].toString();
                    max.replace("[METADATA MAXSIZE", "");   //krazy:exclude=doublequote_chars
                    max.replace("]", "");                   //krazy:exclude=doublequote_chars
                    d->maxAcceptedSize = max.toLongLong();
                }
            } else if (response.content[2].toString() == "[METADATA NOPRIVATE]") {
                d->metaDataErrors |= NoPrivate;
            }
        } else if (response.content.size() < 2) {
            setErrorText(QString("%1 failed, malformed reply from the server.").arg(d->m_name));
        } else if (response.content[1].toString() != "OK") {
            setError(UserDefinedError);
            setErrorText(QString("%1 failed, server replied: %2").arg(d->m_name).arg(QLatin1String(response.toString().constData())));
        }
        emitResult();
    } else if (d->serverCapability == Metadata && response.content[0].toString() == "+") {
        QByteArray content = "";
        if (d->entriesIt.value().isEmpty()) {
            content += "NIL";
        } else {
            content +=  d->entriesIt.value();
        }
        ++d->entriesIt;
        if (d->entriesIt == d->entries.constEnd()) {
            content += ')';
        } else {
            content += " \"" + d->entriesIt.key() + '\"';
            int size = d->entriesIt.value().size();
            content += " {" + QByteArray::number( size==0 ? 3 : size ) + '}';
        }
//      qCDebug(KIMAP2_LOG) << "SENT: " << content;
        d->sessionInternal()->sendData(content);
    }
}

void SetMetaDataJob::addMetaData(const QByteArray &name, const QByteArray &value)
{
    Q_D(SetMetaDataJob);
    if (d->serverCapability == Annotatemore && (name.startsWith("/shared") || name.startsWith("/private"))) {
        const QByteArray &attribute = d->getAttribute(name);
        d->entries[attribute] = value;
        d->entryName = d->removePrefix(name);
    } else {
        d->entries[name] = value;
    }
}

void SetMetaDataJob::setEntry(const QByteArray &entry)
{
    Q_D(SetMetaDataJob);
    d->entryName = entry;
}

SetMetaDataJob::MetaDataErrors SetMetaDataJob::metaDataErrors() const
{
    Q_D(const SetMetaDataJob);
    return d->metaDataErrors;
}
