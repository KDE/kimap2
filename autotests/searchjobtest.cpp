/*
 * Copyright (C) 2013  Daniel Vrátil <dvratil@redhat.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 */

#include <qtest.h>

#include "kimap2test/fakeserver.h"
#include "kimap/loginjob.h"
#include "kimap/session.h"
#include "kimap/searchjob.h"

#include <QtTest>

typedef QPair< KIMAP2::SearchJob::SearchCriteria, QByteArray > SearchCriteriaValuePair;

Q_DECLARE_METATYPE(QList<SearchCriteriaValuePair>)
Q_DECLARE_METATYPE(KIMAP2::SearchJob::SearchLogic)
Q_DECLARE_METATYPE(KIMAP2::Term)

#define searchPair(a,b) qMakePair<KIMAP2::SearchJob::SearchCriteria, QByteArray>( a, b )

class SearchJobTest: public QObject
{
    Q_OBJECT

private Q_SLOTS:

    void testSearch_data()
    {
        QTest::addColumn<QList<QByteArray> >("scenario");
        QTest::addColumn<bool>("uidbased");
        QTest::addColumn<int>("expectedResultsCount");
        QTest::addColumn<KIMAP2::SearchJob::SearchLogic>("searchLogic");
        QTest::addColumn<QList<SearchCriteriaValuePair> >("searchCriteria");

        QList<QByteArray> scenario;
        QList<SearchCriteriaValuePair> criteria;
        scenario << FakeServer::preauth()
                 << "C: A000001 UID SEARCH HEADER Message-Id <12345678@mail.box>"
                 << "S: * SEARCH 10 12"
                 << "S: A000001 OK search done";

        criteria << searchPair(KIMAP2::SearchJob::Header, "Message-Id <12345678@mail.box>");
        QTest::newRow("uidbased header search") << scenario << true << 2 << KIMAP2::SearchJob::And << criteria;

        scenario.clear();
        criteria.clear();
        scenario << FakeServer::preauth()
                 << "C: A000001 SEARCH OR (NEW) (HEADER Message-Id <12345678@mail.box>)"
                 << "S: * SEARCH"
                 << "S: A000001 OK search done";

        criteria << searchPair(KIMAP2::SearchJob::New, QByteArray())
                 << searchPair(KIMAP2::SearchJob::Header, "Message-Id <12345678@mail.box>");
        QTest::newRow("OR search with no results") << scenario << false << 0 << KIMAP2::SearchJob::Or << criteria;

        scenario.clear();
        criteria.clear();
        scenario << FakeServer::preauth()
                 << "C: A000001 SEARCH TO {25}\r\n<testuser@kde.testserver>"
                 << "S: * SEARCH 1"
                 << "S: A000001 OK search done";
        criteria << searchPair(KIMAP2::SearchJob::To, "<testuser@kde.testserver>");
        QTest::newRow("literal data search") << scenario << false << 1 << KIMAP2::SearchJob::And << criteria;

        scenario.clear();
        criteria.clear();
        scenario << FakeServer::preauth()
                 << "C: A000001 UID SEARCH NOT (NEW)"
                 << "S: * SEARCH 1 2 3 4 5 6"
                 << "S: A000001 OK search done";
        criteria << searchPair(KIMAP2::SearchJob::New, QByteArray());
        QTest::newRow("uidbased NOT NEW search") << scenario << true << 6 << KIMAP2::SearchJob::Not << criteria;
    }

    void testSearch()
    {
        QFETCH(QList<QByteArray>, scenario);
        QFETCH(bool, uidbased);
        QFETCH(int, expectedResultsCount);
        QFETCH(KIMAP2::SearchJob::SearchLogic, searchLogic);
        QFETCH(QList<SearchCriteriaValuePair>, searchCriteria);

        FakeServer fakeServer;
        fakeServer.setScenario(scenario);
        fakeServer.startAndWait();

        KIMAP2::Session session(QLatin1String("127.0.0.1"), 5989);

        KIMAP2::SearchJob *job = new KIMAP2::SearchJob(&session);
        job->setUidBased(uidbased);
        job->setSearchLogic(searchLogic);
        Q_FOREACH (const SearchCriteriaValuePair &pair, searchCriteria) {
            if (pair.second.isEmpty()) {
                job->addSearchCriteria(pair.first);
            } else {
                job->addSearchCriteria(pair.first, pair.second);
            }
        }

        bool result = job->exec();
        QVERIFY(result);
        if (result) {
            QVector<qint64> foundItems = job->results();
            QCOMPARE(foundItems.size(), expectedResultsCount);
        }

        fakeServer.quit();
    }

    void testSearchTerm_data()
    {
        QTest::addColumn<QList<QByteArray> >("scenario");
        QTest::addColumn<bool>("uidbased");
        QTest::addColumn<int>("expectedResultsCount");
        QTest::addColumn<KIMAP2::Term>("searchTerm");

        {
            QList<QByteArray> scenario;
            scenario << FakeServer::preauth()
                     << "C: A000001 UID SEARCH HEADER Message-Id \"<12345678@mail.box>\""
                     << "S: * SEARCH 10 12"
                     << "S: A000001 OK search done";

            QTest::newRow("uidbased header search") << scenario << true << 2 << KIMAP2::Term(QLatin1String("Message-Id"), QLatin1String("<12345678@mail.box>"));
        }
        {
            QList<QByteArray> scenario;
            scenario << FakeServer::preauth()
                     << "C: A000001 SEARCH OR NEW HEADER Message-Id \"<12345678@mail.box>\""
                     << "S: * SEARCH"
                     << "S: A000001 OK search done";

            QTest::newRow("OR search with no results") << scenario << false << 0 << KIMAP2::Term(KIMAP2::Term::Or, QVector<KIMAP2::Term>() << KIMAP2::Term(KIMAP2::Term::New) << KIMAP2::Term(QLatin1String("Message-Id"), QLatin1String("<12345678@mail.box>")));
        }
        {
            QList<QByteArray> scenario;
            scenario << FakeServer::preauth()
                     << "C: A000001 SEARCH TO \"<testuser@kde.testserver>\""
                     << "S: * SEARCH 1"
                     << "S: A000001 OK search done";
            QTest::newRow("literal data search") << scenario << false << 1 << KIMAP2::Term(KIMAP2::Term::To, QLatin1String("<testuser@kde.testserver>"));
        }
        {
            QList<QByteArray> scenario;
            scenario << FakeServer::preauth()
                     << "C: A000001 UID SEARCH NOT NEW"
                     << "S: * SEARCH 1 2 3 4 5 6"
                     << "S: A000001 OK search done";
            QTest::newRow("uidbased NOT NEW search") << scenario << true << 6 << KIMAP2::Term(KIMAP2::Term::New).setNegated(true);
        }
    }

    void testSearchTerm()
    {
        QFETCH(QList<QByteArray>, scenario);
        QFETCH(bool, uidbased);
        QFETCH(int, expectedResultsCount);
        QFETCH(KIMAP2::Term, searchTerm);

        FakeServer fakeServer;
        fakeServer.setScenario(scenario);
        fakeServer.startAndWait();

        KIMAP2::Session session(QLatin1String("127.0.0.1"), 5989);

        KIMAP2::SearchJob *job = new KIMAP2::SearchJob(&session);
        job->setUidBased(uidbased);
        job->setTerm(searchTerm);

        bool result = job->exec();
        QVERIFY(result);
        if (result) {
            QVector<qint64> foundItems = job->results();
            QCOMPARE(foundItems.size(), expectedResultsCount);
        }

        fakeServer.quit();
    }

};

QTEST_GUILESS_MAIN(SearchJobTest)

#include "searchjobtest.moc"
