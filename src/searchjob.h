/*
    Copyright (c) 2009 Andras Mantia <amantia@kde.org>
    Copyright (c) 2014 Christian Mollekopf <mollekopf@kolabsys.com>

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

#ifndef KIMAP2_SEARCHJOB_H
#define KIMAP2_SEARCHJOB_H

#include "kimap2_export.h"

#include "job.h"
#include <QSharedPointer>

class QDate;

namespace KIMAP2
{

class ImapSet;

class Session;
struct Message;
class SearchJobPrivate;

/**
 * A query term.
 * Refer to the IMAP RFC for the meaning of the individual terms.
 * @since 4.13
 */
class KIMAP2_EXPORT Term
{
public:
    enum Relation {
        And,
        Or
    };

    enum SearchKey {
        All,
        Bcc,
        Body,
        Cc,
        From,
        Subject,
        Text,
        To,
        Keyword
    };

    enum BooleanSearchKey {
        New,
        Old,
        Recent,
        Seen,
        Draft,
        Deleted,
        Flagged,
        Answered
    };

    enum DateSearchKey {
        Before,
        On,
        Since,
        SentBefore,
        SentOn,
        SentSince
    };
    enum NumberSearchKey {
        Larger,
        Smaller
    };
    enum SequenceSearchKey {
        Uid,
        SequenceNumber
    };

    Term();
    ~Term() = default; // silence clazy rule of three warning
    Term(Relation relation, const QVector<Term> &subterms);
    Term(SearchKey key, const QString &value);
    Term(BooleanSearchKey key);
    Term(DateSearchKey key, const QDate &date);
    Term(NumberSearchKey key, int value);
    Term(SequenceSearchKey key, const KIMAP2::ImapSet &);
    Term(const QString &header, const QString &value);

    Term(const Term &other);

    Term &operator=(const Term &other);
    bool operator==(const Term &other) const;

    bool isNull() const;

    Term &setFuzzy(bool fuzzy);
    Term &setNegated(bool negated);

    QByteArray serialize() const;

private:
    class Private;
    QSharedPointer<Private> d;
};

class KIMAP2_EXPORT SearchJob : public Job
{
    Q_OBJECT
    Q_DECLARE_PRIVATE(SearchJob)

    friend class SessionPrivate;

public:
    enum SearchLogic {
        And = 0,
        Or,
        Not
    };

    enum SearchCriteria {
        All = 0,
        Answered,
        BCC,
        Before,
        Body,
        CC,
        Deleted,
        Draft,
        Flagged,
        From,
        Header,
        Keyword,
        Larger,
        New,
        Old,
        On,
        Recent,
        Seen,
        SentBefore,
        SentOn,
        SentSince,
        Since,
        Smaller,
        Subject,
        Text,
        To,
        Uid,
        Unanswered,
        Undeleted,
        Undraft,
        Unflagged,
        Unkeyword,
        Unseen
    };

    explicit SearchJob(Session *session);
    virtual ~SearchJob();

    void setUidBased(bool uidBased);
    bool isUidBased() const;

    void setCharset(const QByteArray &charSet);
    QByteArray charset() const;

    /**
     * Get the search result, as a list of sequence numbers or UIDs, based on the isUidBased status
     * @return the found items
     * @since 4.6
     */
    QVector<qint64> results() const;

    /**
     * Sets the search term.
     * @param term The search term.
     * @since 4.13
     */
    void setTerm(const Term &);

protected:
    void doStart() Q_DECL_OVERRIDE;
    void handleResponse(const Message &response) Q_DECL_OVERRIDE;
};

}

#endif
