/*

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
#include "kimap/session.h"
#include "kimap/fetchjob.h"

#include <QtTest>

Q_DECLARE_METATYPE(KIMAP2::FetchJob::FetchScope)

class Benchmark: public QObject
{
    Q_OBJECT

private:
    QStringList m_signals;

    QMap<qint64, qint64> m_uids;
    QMap<qint64, qint64> m_sizes;
    QMap<qint64, KIMAP2::MessageFlags> m_flags;
    QMap<qint64, KIMAP2::MessagePtr> m_messages;
    QMap<qint64, KIMAP2::MessageAttribute> m_attrs;

public Q_SLOTS:
    void onHeadersReceived(const QString &/*mailBox*/,
                           const QMap<qint64, qint64> &uids,
                           const QMap<qint64, qint64> &sizes,
                           const QMap<qint64, KIMAP2::MessageAttribute> &attrs,
                           const QMap<qint64, KIMAP2::MessageFlags> &flags,
                           const QMap<qint64, KIMAP2::MessagePtr> &messages)
    {
        m_signals << QStringLiteral("headersReceived");
        m_uids.unite(uids);
        m_sizes.unite(sizes);
        m_flags.unite(flags);
        m_messages.unite(messages);
        m_attrs.unite(attrs);
    }

    void onMessagesReceived(const QString &/*mailbox*/,
                            const QMap<qint64, qint64> uids,
                            const QMap<qint64, KIMAP2::MessageAttribute> &attrs,
                            const QMap<qint64, KIMAP2::MessagePtr> &messages)
    {
        m_signals << QStringLiteral("messagesReceived");
        m_uids.unite(uids);
        m_messages.unite(messages);
        m_attrs.unite(attrs);
    }

private Q_SLOTS:

    void testFetchParts()
    {
        int count = 5000;
        int parsedBytes = 0;
        QList<QByteArray> scenario;
        scenario << FakeServer::preauth();
        parsedBytes += scenario.last().size();
        scenario << "C: A000001 FETCH 1:* (BODY.PEEK[HEADER.FIELDS (TO FROM MESSAGE-ID REFERENCES IN-REPLY-TO SUBJECT DATE)] BODY.PEEK[1.1.1.MIME] BODY.PEEK[1.1.1] FLAGS UID)";
        for (int i = 1; i <= count; i++) {
            scenario << QString("S: * %1 FETCH (UID %2 FLAGS (\\Seen) BODY[HEADER.FIELDS (TO FROM MESSAGE-ID REFERENCES IN-REPLY-TO SUBJECT DATE)] {154}\r\nFrom: Joe Smith <smith@example.com>\r\nDate: Wed, 2 Mar 2011 11:33:24 +0700\r\nMessage-ID: <1234@example.com>\r\nSubject: hello\r\nTo: Jane <jane@example.com>\r\n\r\n BODY[1.1.1] {28}\r\nHi Jane, nice to meet you!\r\n BODY[1.1.1.MIME] {48}\r\nContent-Type: text/plain; charset=ISO-8859-1\r\n\r\n)\r\n").arg(i).arg(i).toLatin1();
            parsedBytes += scenario.last().size();
        };
        scenario << "S: A000001 OK fetch done";
        parsedBytes += scenario.last().size();

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
        job->setSequenceSet(KIMAP2::ImapSet(1, 0));
        job->setScope(scope);

        connect(job, SIGNAL(headersReceived(QString,
                                            QMap<qint64, qint64>,
                                            QMap<qint64, qint64>,
                                            QMap<qint64, KIMAP2::MessageAttribute>,
                                            QMap<qint64, KIMAP2::MessageFlags>,
                                            QMap<qint64, KIMAP2::MessagePtr>)),
                this, SLOT(onHeadersReceived(QString,
                                             QMap<qint64, qint64>,
                                             QMap<qint64, qint64>,
                                             QMap<qint64, KIMAP2::MessageAttribute>,
                                             QMap<qint64, KIMAP2::MessageFlags>,
                                             QMap<qint64, KIMAP2::MessagePtr>)));

        QTime time;
        time.start();

        bool result = job->exec();

        qWarning() << "Reading " << count << " messages took: " << time.elapsed() << " ms.";
        qWarning() << parsedBytes << " bytes expected to be parsed";

        QVERIFY(result);
        QVERIFY(m_signals.count() > 0);
        QCOMPARE(m_uids.count(), count);
        QCOMPARE(m_messages.count(), count);
        QCOMPARE(m_attrs.count(), 0);

        // Check that we received the message header
        QVERIFY(m_messages[2]);
        m_messages[2]->parse();
        QCOMPARE(m_messages[2]->messageID()->identifier(), QByteArray("1234@example.com"));

        fakeServer.quit();

        m_signals.clear();
        m_uids.clear();
        m_sizes.clear();
        m_flags.clear();
        m_messages.clear();
        m_attrs.clear();
    }

};

QTEST_GUILESS_MAIN(Benchmark)

#include "benchmark.moc"
