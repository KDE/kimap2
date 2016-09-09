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

#ifndef KIMAP2_FETCHJOB_H
#define KIMAP2_FETCHJOB_H

#include "kimap2_export.h"

#include "imapset.h"
#include "job.h"

#include <kmime/kmime_content.h>
#include <kmime/kmime_message.h>

namespace KIMAP2
{

class Session;
struct Message;
class FetchJobPrivate;

typedef QSharedPointer<KMime::Content> ContentPtr;
typedef QMap<QByteArray, ContentPtr> MessageParts;

typedef QSharedPointer<KMime::Message> MessagePtr;
typedef QList<QByteArray> MessageFlags;

typedef QPair<QByteArray, QVariant> MessageAttribute;
typedef QList<MessageAttribute> MessageAttributes;

/**
 * Fetch message data from the server
 *
 * All data is returned using the signals, so you need to connect to
 * the relevant signal (or all of them) before starting the job.
 *
 * This job will always use BODY.PEEK rather than BODY to fetch message
 * content, so it will not set the \Seen flag.
 *
 * This job can only be run when the session is in the selected state.
 */
class KIMAP2_EXPORT FetchJob : public Job
{
    Q_OBJECT
    Q_DECLARE_PRIVATE(FetchJob)

    friend class SessionPrivate;

public:
    /**
     * Used to indicate what message data should be fetched.
     *
     * This doesn't provide the same fine-grained control over
     * what is fetched that the IMAP FETCH command normally
     * does, but the common cases are catered for.
     */
    class KIMAP2_EXPORT FetchScope
    {
    public:
        FetchScope();

        /**
         * Used to indicate what part of the message should be fetched.
         */
        enum Mode {
            /**
             * Fetch RFC-2822 or MIME message headers.
             *
             * To fetch MIME headers for a MIME part, populate the @p parts field.
             *
             * If the RFC-2822 headers are requested (so @p parts is empty), the
             * returned information is:
             * - To, From, Message-id, References In-Reply-To, Subject and Date headers
             * - The message size (in octets)
             * - The internal date of the message
             * - The message flags
             * - The message UID
             */
            Headers,
            /**
             * Fetch the message flags (the UID is also fetched)
             */
            Flags,
            /**
             * Fetch the MIME message body structure (the UID is also fetched)
             */
            Structure,
            /**
             * Fetch the message content (the UID is also fetched)
             *
             * To fetch only certain MIME parts (see Structure), populate the
             * @p parts field.
             */
            Content,
            /**
             * Fetch the complete message.
             */
            Full,
            /**
             * Fetch the message MIME headers and the content of parts specified in the @p parts
             * field.
             *
             * If @p parts is empty, this mode will return the full message, just like
             * FetchScope::Content
             *
             * Use case:
             * -# Start a FetchJob with the FetchScope::Structure mode to retrieve the structure
             *    of the message.
             * -# Parse the structure to identify the parts that are interesting (ie: probably
             *    everything but attachments).
             * -# Start another FetchJob with FetchScope::HeaderAndContent to fetch those parts.
             * -# At the request of the user, you can repeat the step above to fetch the attachments.
             */
            HeaderAndContent,

            /**
             * Fetch message size (in octets), internal date of the message, flags, UID
             * and all RFC822 headers.
             *
             * The @p parts field is ignored when using this scope
             */
            FullHeaders
        };

        /**
         * Specify which message parts to operate on.
         *
         * This refers to multipart-MIME message parts or MIME-IMB encapsulated
         * message parts.
         *
         * Note that this is ignored unless @p mode is Headers or Content.
         *
         * If @p mode is Headers, this sets the parts to get the MIME headers
         * for.  If this list is empty, the headers for the whole message
         * (the RFC-2822 headers) are fetched.
         *
         * If @p mode is Content, this sets the parts to fetch.  Parts are
         * fetched wholesale.  If this list is empty, the whole message body
         * is fetched (all MIME parts together).
         */
        QList<QByteArray> parts;

        /**
         * Specify what message data should be fetched.
         */
        Mode mode;

        /**
         * Specify to fetch only items with mod-sequence higher then @p changedSince.
         *
         * The server must have CONDSTORE capability (RFC4551).
         *
         * Default value is 0 (ignored).
         *
         */
        quint64 changedSince;

        /**
        * Enables retrieving of Gmail-specific extensions
        *
        * The FETCH response will contain X-GM-MSGID, X-GM-THRID and X-GM-LABELS
        *
        * Do NOT enable this, unless talking to Gmail servers, otherwise the
        * request may fail.
        */
        bool gmailExtensionsEnabled;
    };

    class KIMAP2_EXPORT Result
    {
    public:
        qint64 sequenceNumber;
        qint64 uid;
        qint64 size;
        KIMAP2::MessageFlags flags;
        KIMAP2::MessagePtr message;
        KIMAP2::MessageParts parts;
        KIMAP2::MessageAttributes attributes;
    };

    explicit FetchJob(Session *session);
    virtual ~FetchJob();

    /**
     * Set which messages to fetch data for.
     *
     * If sequence numbers are given, isUidBased() should be false.  If UIDs
     * are given, isUidBased() should be true.
     *
     * @param set  the sequence numbers or UIDs of the messages to fetch data for
     */
    void setSequenceSet(const ImapSet &set);
    /**
     * The messages that will be fetched.
     */
    ImapSet sequenceSet() const;

    /**
     * Set how the sequence set should be interpreted.
     *
     * @param uidBased  if @c true the argument to setSequenceSet will be
     *                  interpreted as UIDs, if @c false it will be interpreted
     *                  as sequence numbers
     */
    void setUidBased(bool uidBased);
    /**
     * How to interpret the sequence set.
     *
     * @return  if @c true the result of sequenceSet() should be
     *          interpreted as UIDs, if @c false it should be interpreted
     *          as sequence numbers
     */
    bool isUidBased() const;

    /**
     * Sets what data should be fetched.
     *
     * The default scope is FetchScope::Content (all content parts).
     *
     * @param scope  a FetchScope object describing what data
     *               should be fetched
     */
    void setScope(const FetchScope &scope);
    /**
     * Specifies what data will be fetched.
     */
    FetchScope scope() const;

Q_SIGNALS:
    void resultReceived(const Result &);

protected:
    void doStart() Q_DECL_OVERRIDE;
    void handleResponse(const Message &response) Q_DECL_OVERRIDE;
};

}

#endif
