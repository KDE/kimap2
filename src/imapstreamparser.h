/*
    Copyright (c) 2006 - 2007 Volker Krause <vkrause@kde.org>
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

#ifndef KIMAP2_IMAPSTREAMPARSER_P_H
#define KIMAP2_IMAPSTREAMPARSER_P_H

#include "kimap2_export.h"

#include <QtCore/QByteArray>
#include <QtCore/QList>
#include <QtCore/QString>
#include <functional>
#include <message_p.h>

class QIODevice;

namespace KIMAP2
{

/**
  Parser for IMAP messages that operates on a local socket stream.
*/
class KIMAP2_EXPORT ImapStreamParser
{
public:
    /**
     * Construct the parser.
     * @param socket the local socket to work with.
     * @param serverModeEnabled true if the parser has to assume we're writing a server (e.g. sends
     * continuation message automatically)
     */
    explicit ImapStreamParser(QIODevice *socket, bool serverModeEnabled = false);

    /**
     * Get a string from the message. If the upcoming data is not a quoted string, unquoted string or a literal,
     * the behavior is undefined. Use @ref hasString to be sure a string comes.
     * @return the next string from the message as an utf8 string
     */
    QString readUtf8String();

    /**
     * Same as above, but without decoding it to utf8.
     * @return the next string from the message
     */
    QByteArray readString();

    /**
     * Get he next parenthesized list. If the upcoming data is not a parenthesized list,
     * the behavior is undefined. Use @ref hasList to be sure a string comes.
     * @return the next parenthesized list.
     */
    QList<QByteArray> readParenthesizedList();

    /**
     * Get the next data as a number.
     * @param ok true if the data found was a number
     * @return the number
     */
    qint64 readNumber(bool *ok = Q_NULLPTR);

    /**
     * Check if the next data is a string or not.
     * @return true if a string follows
     */
    bool hasString();

    /**
     * Check if the next data is a literal data or not. If a literal is found, the
     * internal position pointer is set to the beginning of the literal data.
     * @return true if a literal follows
     */
    bool hasLiteral();

    /**
     * Read the next literal sequence. This might or might not be the full data. Example code to read a literal would be:
     * @code
     * ImapStreamParser parser;
     *  ...
     * if (parser.hasLiteral())
     * {
     *   while (!parser.atLiteralEnd())
     *   {
     *      QByteArray data = parser.readLiteralPart();
     *      // do something with the data
     *   }
     * }
     * @endcode
     *
     * @return part of a literal data
     */
    QByteArray readLiteralPart();

    /**
     * Check if the literal data end was reached. See @ref hasLiteral and @ref readLiteralPart .
     * @return true if the literal was completely read.
     */
    bool atLiteralEnd() const;

    /**
     * Check if the next data is a parenthesized list.
     * @return true if a parenthesized list comes.
    */
    bool hasList();

    /**
    * Check if the next data is a parenthesized list end.
    * @return true if a parenthesized list end.
     */
    bool atListEnd();

    /**
     * Check if the next data is a response code.
     * @return true if a response code comes.
     */
    bool hasResponseCode();

    /**
    * Check if the next data is a response code end.
    * @return true if a response code end.
     */
    bool atResponseCodeEnd();

    /**
     * Check if the command end was reached
     * @return true if the end of command is reached
     */
    bool atCommandEnd();

    /**
     * Return everything that remained from the command.
     * @return the remaining command data
     */
    QByteArray readUntilCommandEnd();

    /**
     * Return all the data that was read from the socket, but not processed yet.
     * @return the remaining unprocessed data
     */
    QByteArray readRemainingData();

    int availableDataSize() const;

    void setData(const QByteArray &data);
    QByteArray data() const;

    void parseStream();

    void saveState();
    void restoreState();

    bool insufficientData();

    /**
     * Remove already read data from the internal buffer if necessary.
     */
    void trimBuffer();

    void onResponseReceived(std::function<void(const Message &)>);

private:
    void stripLeadingSpaces();
    QByteArray parseQuotedString();

    /**
     * If the condition is true, wait for more data to be available from the socket.
     * If no data comes after a timeout (30000ms), it aborts and returns false.
     * @param wait the condition
     * @return true if more data is available
     */
    bool waitForMoreData(bool wait);

    /**
     * Inform the client to send more literal data.
     */
    void sendContinuationResponse(qint64 size);

    /**
     * Skip whitespace until @param char, and then increment by one.
     * Returns false if the next char isn't @param c.
     */
    bool advanceOver(const char c);
    void advance(int = 1);

    int readFromSocket();

    bool dataAvailable();
    bool dataAvailable(int pos);

    char at(int pos) const;
    QByteArray mid(int start, int end = -1)  const;
    int length() const;
    int indexOf(const char);

    QByteArray &buffer();
    const QByteArray &buffer() const;

    QIODevice *m_socket;
    bool m_isServerModeEnabled;
    int m_position;
    int m_readPosition;
    int m_savedState;
    qint64 m_literalSize;
    bool m_insufficientData;
    QByteArray m_data1;
    QByteArray m_data2;
    QByteArray *m_current;
    int m_bufferSize;

    std::function<void(const Message &)> responseReceived;
};

}

#endif
