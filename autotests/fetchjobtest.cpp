/*
   Copyright (c) 2010 Klarälvdalens Datakonsult AB, a KDAB Group company <info@kdab.com>
   Author: Kevin Ottens <kevin@kdab.com>

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
*/

#include <qtest.h>

#include "kimap2test/fakeserver.h"
#include "kimap2/session.h"
#include "kimap2/fetchjob.h"

#include <QtTest>

Q_DECLARE_METATYPE(KIMAP2::FetchJob::FetchScope)

using namespace KIMAP2;

class FetchJobTest: public QObject
{
    Q_OBJECT

private:
    QStringList m_signals;

    QMap<qint64, qint64> m_uids;
    QMap<qint64, qint64> m_sizes;
    QMap<qint64, KIMAP2::MessageFlags> m_flags;
    QMap<qint64, KIMAP2::MessagePtr> m_messages;
    QMap<qint64, KIMAP2::MessageParts> m_parts;
    QMap<qint64, KIMAP2::MessageAttributes> m_attrs;

public Q_SLOTS:
    void onResultReceived(const FetchJob::Result &result)
    {
        qDebug() << "Received result " << result.sequenceNumber;
        m_signals << QStringLiteral("headersReceived");
        m_uids.insert(result.sequenceNumber, result.uid);
        m_sizes.insert(result.sequenceNumber, result.size);
        m_flags.insert(result.sequenceNumber, result.flags);
        m_messages.insert(result.sequenceNumber, result.message);
        if (!result.attributes.isEmpty()) {
            m_attrs.insert(result.sequenceNumber, result.attributes);
        }
        m_parts.insert(result.sequenceNumber, result.parts);
    }

private Q_SLOTS:

    void testFetch_data()
    {
        qRegisterMetaType<KIMAP2::FetchJob::FetchScope>();

        QTest::addColumn<bool>("uidBased");
        QTest::addColumn< KIMAP2::ImapSet >("set");
        QTest::addColumn<int>("expectedMessageCount");
        QTest::addColumn< QList<QByteArray> >("scenario");
        QTest::addColumn<KIMAP2::FetchJob::FetchScope>("scope");

        KIMAP2::FetchJob::FetchScope scope;
        scope.mode = KIMAP2::FetchJob::FetchScope::Flags;
        scope.changedSince = 123456789;

        QList<QByteArray> scenario;
        scenario << FakeServer::preauth()
                 << "C: A000001 FETCH 1:4 (FLAGS UID) (CHANGEDSINCE 123456789)"
                 << "S: * 1 FETCH ( FLAGS () UID 1 )"
                 << "S: * 2 FETCH ( FLAGS () UID 2 )"
                 << "S: * 3 FETCH ( FLAGS () UID 3 )"
                 << "S: * 4 FETCH ( FLAGS () UID 4 )"
                 << "S: A000001 OK fetch done";

        QTest::newRow("messages have empty flags (with changedsince)") << false << KIMAP2::ImapSet(1, 4) << 4
                << scenario << scope;

        scenario.clear();
        scope.changedSince = 0;
        scenario << FakeServer::preauth()
                 << "C: A000001 FETCH 1:4 (FLAGS UID)"
                 << "S: * 1 FETCH ( FLAGS () UID 1 )"
                 << "S: * 2 FETCH ( FLAGS () UID 2 )"
                 << "S: * 3 FETCH ( FLAGS () UID 3 )"
                 << "S: * 4 FETCH ( FLAGS () UID 4 )"
                 << "S: A000001 OK fetch done";

        QTest::newRow("messages have empty flags") << false << KIMAP2::ImapSet(1, 4) << 4
                << scenario << scope;

        scenario.clear();
        // kill the connection part-way through a list, with carriage returns at end
        // BUG 253619
        // this should fail, but it shouldn't crash
        scenario << FakeServer::preauth()
                 << "C: A000001 FETCH 11 (RFC822.SIZE INTERNALDATE BODY.PEEK[HEADER.FIELDS (TO FROM MESSAGE-ID REFERENCES IN-REPLY-TO SUBJECT DATE)] FLAGS UID)"
                 << "S: * 11 FETCH (RFC822.SIZE 770 INTERNALDATE \"11-Oct-2010 03:33:50 +0100\" BODY[HEADER.FIELDS (TO FROM MESSAGE-ID REFERENCES IN-REPLY-TO SUBJECT DATE)] {246}"
                 << "S: From: John Smith <jonathanr.smith@foobarbaz.com>\r\nTo: \"amagicemailaddress@foobarbazbarfoo.com\"\r\n\t<amagicemailaddress@foobarbazbarfoo.com>\r\nDate: Mon, 11 Oct 2010 03:34:48 +0100\r\nSubject: unsubscribe\r\nMessage-ID: <ASDFFDSASDFFDS@foobarbaz.com>\r\n\r\n"
                 << "X";
        scope.mode = KIMAP2::FetchJob::FetchScope::Headers;
        QTest::newRow("connection drop") << false << KIMAP2::ImapSet(11, 11) << 1 << scenario << scope;

        scenario.clear();
        // Important bit here if "([127.0.0.1])" which used to crash the stream parser
        scenario << FakeServer::preauth()
                 << "C: A000001 FETCH 11 (RFC822.SIZE INTERNALDATE BODY.PEEK[HEADER.FIELDS (TO FROM MESSAGE-ID REFERENCES IN-REPLY-TO SUBJECT DATE)] FLAGS UID)"
                 << "S: * 11 FETCH (RFC822.SIZE 770 INTERNALDATE \"11-Oct-2010 03:33:50 +0100\" BODY[HEADER.FIELDS (TO FROM MESSAGE-ID REFERENCES IN-REPLY-TO SUBJECT DATE)] {246}"
                 << "S: ([127.0.0.1])\r\nDate: Mon, 11 Oct 2010 03:34:48 +0100\r\nSubject: unsubscribe\r\nMessage-ID: <ASDFFDSASDFFDS@foobarbaz.com>\r\n\r\n"
                 << "X";
        scope.mode = KIMAP2::FetchJob::FetchScope::Headers;
        QTest::newRow("buffer overwrite") << false << KIMAP2::ImapSet(11, 11) << 1 << scenario << scope;

        scenario.clear();
        // We're assuming a buffer overwrite here which made us miss the opening parenthesis
        // for the properties list
        scenario << FakeServer::preauth()
                 << "C: A000001 FETCH 11 (RFC822.SIZE INTERNALDATE BODY.PEEK[HEADER.FIELDS (TO FROM MESSAGE-ID REFERENCES IN-REPLY-TO SUBJECT DATE)] FLAGS UID)"
                 << "S: * 11 FETCH {10}doh!\r\n\r\n\r\n)\r\n"
                 << "X";
        scope.mode = KIMAP2::FetchJob::FetchScope::Headers;
        QTest::newRow("buffer overwrite 2") << false << KIMAP2::ImapSet(11, 11) << 1 << scenario << scope;

        scenario.clear();
        scenario << FakeServer::preauth()
                 << "C: A000001 FETCH 11 (RFC822.SIZE INTERNALDATE BODY.PEEK[HEADER] FLAGS UID) (CHANGEDSINCE 123456789)"
                 << "S: * 11 FETCH (UID 123 RFC822.SIZE 770 INTERNALDATE \"11-Oct-2010 03:33:50 +0100\" BODY[HEADER] {245}"
                 << "S: From: John Smith <jonathanr.smith@foobarbaz.com>\r\nTo: \"amagicemailaddress@foobarbazbarfoo.com\"\r\n\t<amagicemailaddress@foobarbazbarfoo.com>\r\nDate: Mon, 11 Oct 2010 03:34:48 +0100\r\nSubject: unsubscribe\r\nMessage-ID: <ASDFFDSASDFFDS@foobarbaz.com>\r\n\r\n  FLAGS ())"
                 << "S: A000001 OK fetch done";
        scope.mode = KIMAP2::FetchJob::FetchScope::FullHeaders;
        scope.changedSince = 123456789;
        QTest::newRow("fetch full headers") << false << KIMAP2::ImapSet(11, 11) << 1
                                            << scenario << scope;

        scenario.clear();
        const auto payloadSize = 32000;
        QByteArray payload;
        payload.fill('c', payloadSize);
        scenario << FakeServer::preauth()
                 << "C: A000001 FETCH 11 (RFC822.SIZE INTERNALDATE BODY.PEEK[HEADER] FLAGS UID) (CHANGEDSINCE 123456789)"
                 << QString("S: * 11 FETCH (UID 123 RFC822.SIZE 770 INTERNALDATE \"11-Oct-2010 03:33:50 +0100\" BODY[HEADER] {%1}").arg(payloadSize).toLatin1()
                 << "S: " + payload + " FLAGS ())"
                 << "S: A000001 OK fetch done";
        scope.mode = KIMAP2::FetchJob::FetchScope::FullHeaders;
        scope.changedSince = 123456789;
        QTest::newRow("fetch large payload") << false << KIMAP2::ImapSet(11, 11) << 1
                                            << scenario << scope;
    }

    void testFetch()
    {
        QFETCH(bool, uidBased);
        QFETCH(KIMAP2::ImapSet, set);
        QFETCH(int, expectedMessageCount);
        QFETCH(QList<QByteArray>, scenario);
        QFETCH(KIMAP2::FetchJob::FetchScope, scope);

        FakeServer fakeServer;
        fakeServer.setScenario(scenario);
        fakeServer.startAndWait();

        KIMAP2::Session session(QStringLiteral("127.0.0.1"), 5989);

        KIMAP2::FetchJob *job = new KIMAP2::FetchJob(&session);
        job->setUidBased(uidBased);
        job->setSequenceSet(set);
        job->setScope(scope);

        connect(job, &FetchJob::resultReceived, this, &FetchJobTest::onResultReceived);

        bool result = job->exec();
        QEXPECT_FAIL("connection drop", "Expected failure on connection drop", Continue);
        QEXPECT_FAIL("buffer overwrite", "Expected failure on confused list", Continue);
        QEXPECT_FAIL("buffer overwrite 2", "Expected beginning of message missing", Continue);
        QVERIFY(result);
        if (result) {
            QVERIFY(m_signals.count() > 0);
            QCOMPARE(m_uids.count(), expectedMessageCount);
        }

        QVERIFY(fakeServer.isAllScenarioDone());
        fakeServer.quit();

        m_signals.clear();
        m_uids.clear();
        m_sizes.clear();
        m_flags.clear();
        m_messages.clear();
        m_parts.clear();
        m_attrs.clear();
    }

    void testFetchStructure()
    {
        QList<QByteArray> scenario;
        scenario << FakeServer::preauth()
                 << "C: A000001 FETCH 1:2 (BODYSTRUCTURE UID)"
                 << "S: * 1 FETCH (UID 10 BODYSTRUCTURE (\"TEXT\" \"PLAIN\" (\"CHARSET\" \"ISO-8859-1\") NIL NIL \"7BIT\" 5 1 NIL NIL NIL))"
                 << "S: * 2 FETCH (UID 20 BODYSTRUCTURE ((((\"TEXT\" \"PLAIN\" (\"CHARSET\" \"ISO-8859-1\") NIL NIL \"7BIT\" 72 4 NIL NIL NIL)(\"TEXT\" \"HTML\" (\"CHARSET\" \"ISO-8859-1\") NIL NIL \"QUOTED-PRINTABLE\" 281 5 NIL NIL NIL) \"ALTERNATIVE\" (\"BOUNDARY\" \"0001\") NIL NIL)(\"IMAGE\" \"GIF\" (\"NAME\" \"B56.gif\") \"<B56@goomoji.gmail>\" NIL \"BASE64\" 528 NIL NIL NIL) \"RELATED\" (\"BOUNDARY\" \"0002\") NIL NIL)(\"IMAGE\" \"JPEG\" (\"NAME\" \"photo.jpg\") NIL NIL \"BASE64\" 53338 NIL (\"ATTACHMENT\" (\"FILENAME\" \"photo.jpg\")) NIL) \"MIXED\" (\"BOUNDARY\" \"0003\") NIL NIL))"
                 << "S: A000001 OK fetch done";

        KIMAP2::FetchJob::FetchScope scope;
        scope.mode = KIMAP2::FetchJob::FetchScope::Structure;

        FakeServer fakeServer;
        fakeServer.setScenario(scenario);
        fakeServer.startAndWait();

        KIMAP2::Session session(QStringLiteral("127.0.0.1"), 5989);

        KIMAP2::FetchJob *job = new KIMAP2::FetchJob(&session);
        job->setUidBased(false);
        job->setSequenceSet(KIMAP2::ImapSet(1, 2));
        job->setScope(scope);

        connect(job, &FetchJob::resultReceived, this, &FetchJobTest::onResultReceived);

        bool result = job->exec();
        QVERIFY(result);
        QVERIFY(m_signals.count() > 0);
        QCOMPARE(m_uids.count(), 2);
        QCOMPARE(m_messages.count(), 2);
        QVERIFY(m_messages[1]);
        QCOMPARE(m_messages[1]->attachments().count(), 0);
        QVERIFY(m_messages[2]);
        QCOMPARE(m_messages[2]->attachments().count(), 1);
        QCOMPARE(m_messages[2]->contents().size(), 2);
        QCOMPARE(m_messages[2]->contents()[0]->contents().size(), 2);
        QCOMPARE(m_messages[2]->attachments().at(0)->contentDisposition()->filename(), QStringLiteral("photo.jpg"));

        fakeServer.quit();

        m_signals.clear();
        m_uids.clear();
        m_sizes.clear();
        m_flags.clear();
        m_messages.clear();
        m_parts.clear();
        m_attrs.clear();
    }

    void testFetchParts()
    {
        QList<QByteArray> scenario;
        scenario << FakeServer::preauth()
                 << "C: A000001 FETCH 2 (BODY.PEEK[HEADER.FIELDS (TO FROM MESSAGE-ID REFERENCES IN-REPLY-TO SUBJECT DATE)] BODY.PEEK[1.1.1.MIME] BODY.PEEK[1.1.1] FLAGS UID)"
                 << "S: * 2 FETCH (UID 20 FLAGS (\\Seen) BODY[HEADER.FIELDS (TO FROM MESSAGE-ID REFERENCES IN-REPLY-TO SUBJECT DATE)] {154}\r\nFrom: Joe Smith <smith@example.com>\r\nDate: Wed, 2 Mar 2011 11:33:24 +0700\r\nMessage-ID: <1234@example.com>\r\nSubject: hello\r\nTo: Jane <jane@example.com>\r\n\r\n BODY[1.1.1] {28}\r\nHi Jane, nice to meet you!\r\n BODY[1.1.1.MIME] {48}\r\nContent-Type: text/plain; charset=ISO-8859-1\r\n\r\n)\r\n"
                 << "S: A000001 OK fetch done";

        KIMAP2::FetchJob::FetchScope scope;
        scope.mode = KIMAP2::FetchJob::FetchScope::HeaderAndContent;
        scope.parts.clear();
        scope.parts.append("1.1.1");

        FakeServer fakeServer;
        fakeServer.setScenario(scenario);
        fakeServer.startAndWait();

        KIMAP2::Session session(QStringLiteral("127.0.0.1"), 5989);

        KIMAP2::FetchJob *job = new KIMAP2::FetchJob(&session);
        job->setUidBased(false);
        job->setSequenceSet(KIMAP2::ImapSet(2, 2));
        job->setScope(scope);

        connect(job, &FetchJob::resultReceived, this, &FetchJobTest::onResultReceived);

        bool result = job->exec();

        QVERIFY(result);
        QVERIFY(m_signals.count() > 0);
        QCOMPARE(m_uids.count(), 1);
        QCOMPARE(m_parts.count(), 1);
        QCOMPARE(m_attrs.count(), 0);

        // Check that we received the message header
        QCOMPARE(m_messages[2]->messageID()->identifier(), QByteArray("1234@example.com"));

        // Check that we recieved the flags
        QMap<qint64, KIMAP2::MessageFlags> expectedFlags;
        expectedFlags.insert(2, KIMAP2::MessageFlags() << "\\Seen");
        QCOMPARE(m_flags, expectedFlags);

        // Check that we didn't received the full message body, since we only requested a specific part
        QCOMPARE(m_messages[2]->decodedText().length(), 0);
        QCOMPARE(m_messages[2]->attachments().count(), 0);

        // Check that we received the part we requested
        QByteArray partId = m_parts[2].keys().first();
        QString text = m_parts[2].value(partId)->decodedText(true, true);
        QCOMPARE(partId, QByteArray("1.1.1"));
        QCOMPARE(text, QStringLiteral("Hi Jane, nice to meet you!")) ;

        fakeServer.quit();

        m_signals.clear();
        m_uids.clear();
        m_sizes.clear();
        m_flags.clear();
        m_messages.clear();
        m_parts.clear();
        m_attrs.clear();
    }

};

QTEST_GUILESS_MAIN(FetchJobTest)

#include "fetchjobtest.moc"
