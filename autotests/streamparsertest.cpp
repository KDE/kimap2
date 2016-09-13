/*
   Copyright (C) 2013 Christian Mollekopf <mollekopf@kolabsys.com>

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

#include "imapstreamparser.h"
#include <message_p.h>

#include <QtTest>

using namespace KIMAP2;

class StreamParserTest: public QObject
{
    Q_OBJECT

    QByteArray part1;
    QByteArray part2;
    QByteArray part3;
    QByteArray part4;
    QByteArray completeMessage;
    QList<QByteArray> expectedList;

    void printResponse(const Message &response)
    {
            qDebug() << "content: " << response.content.size();
            for (const auto &c : response.content) {
                if (c.type() == Message::Part::List) {
                    qDebug() << c.toList();
                } else {
                    qDebug() << c.toString();
                }
            }
            qDebug() << "response: " << response.responseCode.size();
            for (const auto &c : response.responseCode) {
                if (c.type() == Message::Part::List) {
                    qDebug() << c.toList();
                } else {
                    qDebug() << c.toString();
                }
            }
    }

private Q_SLOTS:

    void init()
    {

        // part1 = "* 230 FETCH (FLAGS (\\Recent \\Seen) UID 230 INTERNALDATE \" 1-Nov-2013 13:31:17 +0100\" RFC822.SIZE 37)\r\n";
        // completeMessage = part1;
        part1 = "* 230 FETCH (FLAGS (\\Recent \\Seen) UID 230 INTERNALDATE \" 1-Nov-2013 13:31:17 +0100\" RFC822.SIZE 37 BODY[] {37}\r";
        part2 = "\nDate: Fri, 01 Nov 2013 12:31:13 +0000";
        part3 = "body\n";
        part4 = ")\r\n";
        completeMessage = part1 + part2 + part3 + part4;

        expectedList.clear();
        expectedList << "FLAGS";
        expectedList << "(\\Recent \\Seen)";
        expectedList << "UID";
        expectedList << "230";
        expectedList << "INTERNALDATE";
        expectedList << " 1-Nov-2013 13:31:17 +0100";
        expectedList << "RFC822.SIZE";
        expectedList << "37";
        expectedList << "BODY[]";
        expectedList << "Date: Fri, 01 Nov 2013 12:31:13 +0000";
        expectedList << "body\n";
    }

    void testParse()
    {
        QByteArray buffer;
        QBuffer socket(&buffer);
        socket.open(QBuffer::WriteOnly);
        QVERIFY(socket.write(completeMessage) != -1);

        QBuffer readSocket(&buffer);
        readSocket.open(QBuffer::ReadOnly);
        ImapStreamParser parser(&readSocket);

        QVERIFY(parser.availableDataSize() != 0);
        bool gotResponse = false;
        Message message;
        parser.onResponseReceived([this, &gotResponse, &message](const Message &response) {
            gotResponse = true;
            printResponse(response);
            message = response;
        });
        parser.parseStream();
        QVERIFY(gotResponse);
        QCOMPARE(message.content.last().toList(), expectedList);
        QVERIFY(parser.availableDataSize() == 0);
        QVERIFY(!parser.error());
    }

    void testParseInPieces()
    {
        QByteArray buffer;
        QBuffer socket(&buffer);
        socket.open(QBuffer::WriteOnly);

        QBuffer readSocket(&buffer);
        readSocket.open(QBuffer::ReadOnly);
        ImapStreamParser parser(&readSocket);

        bool gotResponse = false;
        Message message;
        parser.onResponseReceived([this, &gotResponse, &message](const Message &response) {
            gotResponse = true;
            printResponse(response);
            message = response;
        });
        parser.parseStream();
        QVERIFY(!gotResponse);
        QVERIFY(socket.write(part1) != -1);
        parser.parseStream();
        QVERIFY(!gotResponse);
        QVERIFY(socket.write(part2) != -1);
        parser.parseStream();
        QVERIFY(!gotResponse);
        QVERIFY(socket.write(part3) != -1);
        parser.parseStream();
        QVERIFY(!gotResponse);
        QVERIFY(socket.write(part4) != -1);
        parser.parseStream();

        QVERIFY(gotResponse);
        QCOMPARE(message.content.last().toList(), expectedList);
        QVERIFY(parser.availableDataSize() == 0);
        QVERIFY(!parser.error());
    }

    //Try parsing angled brackets in a list (which we treat as string).
    void testParse2()
    {
        QByteArray buffer;
        QBuffer socket(&buffer);
        socket.open(QBuffer::WriteOnly);
        QVERIFY(socket.write("* 230 FETCH (BODY[HEADER FOOBAR (FOO BAR)])\r\n") != -1);

        QBuffer readSocket(&buffer);
        readSocket.open(QBuffer::ReadOnly);
        ImapStreamParser parser(&readSocket);

        QList<QByteArray> expectedList;
        expectedList << "BODY[HEADER FOOBAR (FOO BAR)]";

        QVERIFY(parser.availableDataSize() != 0);
        bool gotResponse = false;
        Message message;
        parser.onResponseReceived([this, &gotResponse, &message](const Message &response) {
            gotResponse = true;
            printResponse(response);
            message = response;
        });
        parser.parseStream();
        QVERIFY(gotResponse);
        QCOMPARE(message.content.last().toList(), expectedList);
        QVERIFY(parser.availableDataSize() == 0);
        QVERIFY(!parser.error());
    }

    void testParse3()
    {
        QByteArray buffer;
        QBuffer socket(&buffer);
        socket.open(QBuffer::WriteOnly);
        QVERIFY(socket.write("* 1 FETCH (UID 10 BODYSTRUCTURE (\"TEXT\" \"PLAIN\" (\"CHARSET\" \"ISO-8859-1\") NIL NIL \"7BIT\" 5 1 NIL NIL NIL))\r\n") != -1);

        QBuffer readSocket(&buffer);
        readSocket.open(QBuffer::ReadOnly);
        ImapStreamParser parser(&readSocket);

        QList<QByteArray> expectedList;
        expectedList << "UID";
        expectedList << "10";
        expectedList << "BODYSTRUCTURE";
        expectedList << "(\"TEXT\" \"PLAIN\" (\"CHARSET\" \"ISO-8859-1\") NIL NIL \"7BIT\" 5 1 NIL NIL NIL)";

        QVERIFY(parser.availableDataSize() != 0);
        bool gotResponse = false;
        Message message;
        parser.onResponseReceived([this, &gotResponse, &message](const Message &response) {
            gotResponse = true;
            printResponse(response);
            message = response;
        });
        parser.parseStream();
        QVERIFY(gotResponse);
        QCOMPARE(message.content.last().toList(), expectedList);
        QVERIFY(parser.availableDataSize() == 0);
        QVERIFY(!parser.error());
    }

    void testParse4()
    {
        QByteArray buffer;
        QBuffer socket(&buffer);
        socket.open(QBuffer::WriteOnly);
        QVERIFY(socket.write("* 2 FETCH (UID 20 FLAGS (\\Seen) BODY[HEADER.FIELDS (TO FROM MESSAGE-ID REFERENCES IN-REPLY-TO SUBJECT DATE)] {154}\r\nFrom: Joe Smith <smith@example.com>\r\nDate: Wed, 2 Mar 2011 11:33:24 +0700\r\nMessage-ID: <1234@example.com>\r\nSubject: hello\r\nTo: Jane <jane@example.com>\r\n\r\n BODY[1.1.1] {28}\r\nHi Jane, nice to meet you!\r\n BODY[1.1.1.MIME] {48}\r\nContent-Type: text/plain; charset=ISO-8859-1\r\n\r\n)\r\n") != -1);

        QBuffer readSocket(&buffer);
        readSocket.open(QBuffer::ReadOnly);
        ImapStreamParser parser(&readSocket);

        QList<QByteArray> expectedList;
        expectedList << "UID";
        expectedList << "20";
        expectedList << "FLAGS" << "(\\Seen)" << "BODY[HEADER.FIELDS (TO FROM MESSAGE-ID REFERENCES IN-REPLY-TO SUBJECT DATE)]";
        expectedList << "From: Joe Smith <smith@example.com>\r\nDate: Wed, 2 Mar 2011 11:33:24 +0700\r\nMessage-ID: <1234@example.com>\r\nSubject: hello\r\nTo: Jane <jane@example.com>\r\n\r\n";
        expectedList << "BODY[1.1.1]";
        expectedList << "Hi Jane, nice to meet you!\r\n";
        expectedList << "BODY[1.1.1.MIME]";
        expectedList << "Content-Type: text/plain; charset=ISO-8859-1\r\n\r\n";

        QVERIFY(parser.availableDataSize() != 0);
        bool gotResponse = false;
        Message message;
        parser.onResponseReceived([this, &gotResponse, &message](const Message &response) {
            gotResponse = true;
            printResponse(response);
            message = response;
        });
        parser.parseStream();
        QVERIFY(gotResponse);
        QCOMPARE(message.content.last().toList(), expectedList);
        QVERIFY(parser.availableDataSize() == 0);
        QVERIFY(!parser.error());
    }

    void testParseEmptyFlags()
    {
        QByteArray buffer;
        QBuffer socket(&buffer);
        socket.open(QBuffer::WriteOnly);
        QVERIFY(socket.write("* 1 FETCH ( FLAGS () UID 1 )\r\n") != -1);

        QBuffer readSocket(&buffer);
        readSocket.open(QBuffer::ReadOnly);
        ImapStreamParser parser(&readSocket);

        QList<QByteArray> expectedList;
        expectedList << "FLAGS";
        expectedList << "()";
        expectedList << "UID";
        expectedList << "1";

        QVERIFY(parser.availableDataSize() != 0);
        bool gotResponse = false;
        Message message;
        parser.onResponseReceived([this, &gotResponse, &message](const Message &response) {
            gotResponse = true;
            printResponse(response);
            message = response;
        });
        parser.parseStream();
        QVERIFY(gotResponse);
        QCOMPARE(message.content.last().toList(), expectedList);
        QVERIFY(parser.availableDataSize() == 0);
    }

    void testResponseCode()
    {
        const auto payloadSize = 32000;
        QByteArray payload;
        payload.fill('c', payloadSize);
        QByteArray buffer;
        QBuffer socket(&buffer);
        socket.open(QBuffer::WriteOnly);
        QVERIFY(socket.write("A000001 OK [READ-WRITE] SELECT completed\r\n") != -1);

        QBuffer readSocket(&buffer);
        readSocket.open(QBuffer::ReadOnly);
        ImapStreamParser parser(&readSocket);

        QVERIFY(parser.availableDataSize() != 0);
        bool gotResponse = false;
        Message message;
        parser.onResponseReceived([this, &gotResponse, &message](const Message &response) {
            gotResponse = true;
            printResponse(response);
            message = response;
        });
        parser.parseStream();
        QVERIFY(gotResponse);
        QCOMPARE(message.content.at(0).toString(), QByteArray("A000001"));
        QCOMPARE(message.content.at(1).toString(), QByteArray("OK"));
        QCOMPARE(message.content.at(2).toString(), QByteArray("SELECT"));
        QCOMPARE(message.content.at(3).toString(), QByteArray("completed"));
        QCOMPARE(message.responseCode.at(0).toString(), QByteArray("READ-WRITE"));
        QVERIFY(parser.availableDataSize() == 0);
        QVERIFY(!parser.error());
    }

    void testPermanentFlags()
    {
        QByteArray buffer;
        QBuffer socket(&buffer);
        socket.open(QBuffer::WriteOnly);
        QVERIFY(socket.write("* OK [PERMANENTFLAGS (\\Deleted \\Seen \\*)] Limited\r\n") != -1);

        QBuffer readSocket(&buffer);
        readSocket.open(QBuffer::ReadOnly);
        ImapStreamParser parser(&readSocket);

        QVERIFY(parser.availableDataSize() != 0);
        bool gotResponse = false;
        Message message;
        parser.onResponseReceived([this, &gotResponse, &message](const Message &response) {
            gotResponse = true;
            printResponse(response);
            message = response;
        });
        parser.parseStream();
        QVERIFY(gotResponse);
        QCOMPARE(message.content.at(0).toString(), QByteArray("*"));
        QCOMPARE(message.content.at(1).toString(), QByteArray("OK"));
        QCOMPARE(message.content.at(2).toString(), QByteArray("Limited"));
        QCOMPARE(message.responseCode.at(0).toString(), QByteArray("PERMANENTFLAGS"));
        QCOMPARE(message.responseCode.at(1).toList(), QList<QByteArray>() << "\\Deleted" << "\\Seen" << "\\*");
        QVERIFY(parser.availableDataSize() == 0);
        QVERIFY(!parser.error());
    }

    void testParseLargeLiteral()
    {
        const auto payloadSize = 32000;
        QByteArray payload;
        payload.fill('c', payloadSize);
        const auto data = QString("* 11 FETCH (UID 123 BODY[HEADER] {%1}\r\n").arg(payloadSize).toLatin1() + payload + " FLAGS ())\r\n";
        QByteArray buffer;
        QBuffer socket(&buffer);
        socket.open(QBuffer::WriteOnly);
        QVERIFY(socket.write(data) != -1);

        QBuffer readSocket(&buffer);
        readSocket.open(QBuffer::ReadOnly);
        ImapStreamParser parser(&readSocket);

        QList<QByteArray> expectedList;
        expectedList << "UID";
        expectedList << "123";
        expectedList << "BODY[HEADER]";
        expectedList << payload;
        expectedList << "FLAGS";
        expectedList << "()";

        QVERIFY(parser.availableDataSize() != 0);
        bool gotResponse = false;
        Message message;
        parser.onResponseReceived([this, &gotResponse, &message](const Message &response) {
            gotResponse = true;
            printResponse(response);
            message = response;
        });
        parser.parseStream();
        QVERIFY(gotResponse);
        QCOMPARE(message.content.last().toList(), expectedList);
        QVERIFY(parser.availableDataSize() == 0);
        QVERIFY(!parser.error());
    }

};

QTEST_GUILESS_MAIN(StreamParserTest)

#include "streamparsertest.moc"
