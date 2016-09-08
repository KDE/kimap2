/*
    Copyright (c) 2006 - 2007 Volker Krause <vkrause@kde.org>
    Copyright (c) 2009 Andras Mantia <amantia@kde.org>

    Copyright (c) 2010 Klarälvdalens Datakonsult AB, a KDAB Group company <info@kdab.com>
    Author: Kevin Ottens <kevin@kdab.com>

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

#include "imapstreamparser.h"

#include <ctype.h>
#include <QIODevice>
#include <QDebug>

using namespace KIMAP2;

ImapStreamParser::ImapStreamParser(QIODevice *socket, bool serverModeEnabled)
    : m_socket(socket),
    m_isServerModeEnabled(serverModeEnabled),
    m_position(0),
    m_readPosition(0),
    m_literalSize(0),
    m_insufficientData(false),
    m_bufferSize(16000)
{
    m_data1.resize(m_bufferSize);
    m_data2.resize(m_bufferSize);
    m_current = &m_data1;
}

QByteArray &ImapStreamParser::buffer()
{
    return *m_current;
}

const QByteArray &ImapStreamParser::buffer() const
{
    return *m_current;
}

char ImapStreamParser::at(int pos) const
{
    return m_current->constData()[pos];
}

QByteArray ImapStreamParser::mid(int start, int end)  const
{
    return buffer().mid(start, end);
}

int ImapStreamParser::length() const
{
    return m_readPosition;
}

int ImapStreamParser::indexOf(const char c, int offset) const
{
    return buffer().indexOf(c, offset);
}

QString ImapStreamParser::readUtf8String()
{
    QByteArray tmp;
    tmp = readString();
    QString result = QString::fromUtf8(tmp);
    return result;
}

QByteArray ImapStreamParser::readString()
{
    QByteArray result;
    stripLeadingSpaces();
    if (!dataAvailable()) {
        return QByteArray();
    }

    // literal string
    if (hasLiteral()) {
        while (!atLiteralEnd()) {
            result += readLiteralPart();
            if (m_insufficientData) {
                return QByteArray();
            }
        }
        return result;
    }

    // quoted string
    return parseQuotedString();
}

bool ImapStreamParser::hasString()
{
    int savedPos = m_position;
    stripLeadingSpaces();
    const int pos = m_position;
    m_position = savedPos;
    if (!dataAvailable(pos)) {
        return false;
    }
    const auto c = at(pos);
    if (c == '{') {
        return true; //literal string
    }
    if (c == '"') {
        return true; //quoted string
    }
    // is this an unquoted string, or something else?
    const bool isUnquotedString =
           !(c == ' ' ||
             c == '(' ||
             c == ')' ||
             c == '[' ||
             c == ']' ||
             c == '\n' ||
             c == '\r');
    return isUnquotedString;
}

int ImapStreamParser::readUntil(const char c)
{
    int end = -1;
    int extraBytes = 0;
    while (end == -1) {
        end = indexOf(c, m_position);
        extraBytes++;
        if (end == -1) {
            if (!dataAvailable(m_position + extraBytes)) {
                //Couldn't find end, which means we have insufficient data
                return false;
            }
        }
    }
    return end;
}

bool ImapStreamParser::hasLiteral()
{
    if (!dataAvailable()) {
        return false;
    }
    int savedPos = m_position;
    stripLeadingSpaces();
    if (at(m_position) == '{') {
        //Look for } and read more until we find it or run out of data.
        int end = readUntil('}');
        if (end == -1) {
            //Couldn't find end
            m_position = savedPos;
            return false;
        }
        Q_ASSERT(end > m_position);
        m_literalSize = mid(m_position + 1, end - m_position - 1).toInt();
        // strip CRLF
        m_position = end + 1;
        // ensure that the CRLF is available
        if (!dataAvailable(m_position + 1)) {
            m_position = savedPos;
            return false;
        }
        if (m_position < length() && at(m_position) == '\r') {
            advance();
        }
        if (m_position < length() && at(m_position) == '\n') {
            advance();
        }

        if (m_isServerModeEnabled && m_literalSize > 0) {
            sendContinuationResponse(m_literalSize);
        }
        return true;
    } else {
        m_position = savedPos;
        return false;
    }
}

bool ImapStreamParser::atLiteralEnd() const
{
    return (m_literalSize == 0) || m_insufficientData;
}

QByteArray ImapStreamParser::readLiteralPart()
{
    static const qint64 maxLiteralPartSize = 4096;
    int size = qMin(maxLiteralPartSize, m_literalSize);

    if (!dataAvailable(m_position + size)) {
        return QByteArray();
    }

    if (length() < m_position + size) {   // Still not enough data
        // Take what's already there
        size = length() - m_position;
    }

    QByteArray result = mid(m_position, size);
    m_position += size;
    m_literalSize -= size;
    Q_ASSERT(m_literalSize >= 0);

    return result;
}

bool ImapStreamParser::hasList()
{
    if (!dataAvailable()) {
        return false;
    }
    int savedPos = m_position;
    stripLeadingSpaces();
    int pos = m_position;
    m_position = savedPos;
    if (at(pos) == '(') {
        return true;
    }
    return false;
}

bool ImapStreamParser::atListEnd()
{
    return advanceOver(')');
}

void ImapStreamParser::saveState()
{
    m_savedState = m_position;
}

void ImapStreamParser::restoreState()
{
    m_position = m_savedState;
}

bool ImapStreamParser::insufficientData()
{
    return m_insufficientData;
}

QList<QByteArray> ImapStreamParser::readParenthesizedList()
{
    QList<QByteArray> result;
    if (!dataAvailable()) {
        return QList<QByteArray>();
    }

    stripLeadingSpaces();
    if (at(m_position) != '(') {
        return result; //no list found
    }

    bool concatToLast = false;
    int count = 0;
    int sublistbegin = -1;
    Q_FOREVER {
        advance();
        if (!dataAvailable()) {
            return QList<QByteArray>();
        }
        const auto c = at(m_position);
        if (c == '(') { //Count parenthesis
            ++count;
            if (count == 1) {
                sublistbegin = m_position;
            }
        } else if (c == ')') { //Count parenthesis
            //End of the list
            if (count <= 0) {
                advance();
                return result;
            }
            //End of a sublist
            if (count == 1) {
                result.append(mid(sublistbegin, m_position - sublistbegin + 1));
            }
            --count;
        } else if (c == ' ') { //Skip over whitespace
            //Skip over whitespace
        } else if (c == '[') {
            concatToLast = true;
            if (result.isEmpty()) {
                result.append(QByteArray());
            }
            result.last() += '[';
        } else if (c == ']') {
            concatToLast = false;
            result.last() += ']';
        } else if (c == '"' && count > 0) { //Parse quoted strings
            parseQuotedString();
            m_position--;
        } else if (count == 0) {
            QByteArray ba;
            if (hasLiteral()) {
                while (!atLiteralEnd()) {
                    ba += readLiteralPart();
                    if (m_insufficientData) {
                        return QList<QByteArray>();
                    }
                }
            } else if (hasString()) {
                ba = readString();
            }
            if (m_insufficientData) {
                return QList<QByteArray>();
            }
            // We might sometime get some unwanted CRLF, but we're still not at the end
            // of the list, would make further string reads fail so eat the CRLFs.
            while ((m_position < length()) &&
                    (at(m_position) == '\r' || at(m_position) == '\n')) {
                advance();
            }
            m_position--;
            if (concatToLast) {
                result.last() += ba;
            } else {
                result.append(ba);
            }
        }
    }
    qWarning() << "Failed to read the ParenthesizedList";
    Q_ASSERT(false);
    return QList<QByteArray>();
}

bool ImapStreamParser::advanceOver(const char c)
{
    if (!dataAvailable()) {
        return false;
    }
    int savedPos = m_position;
    stripLeadingSpaces();
    int pos = m_position;
    m_position = savedPos;
    if (at(pos) == c) {
        m_position = pos + 1;
        return true;
    }
    return false;
}

bool ImapStreamParser::hasResponseCode()
{
    return advanceOver('[');
}

void ImapStreamParser::advance(int i)
{
    m_position += i;
}

bool ImapStreamParser::atResponseCodeEnd()
{
    return advanceOver(']');
}

QByteArray ImapStreamParser::parseQuotedString()
{
    stripLeadingSpaces();
    int end = m_position;
    QByteArray result;
    if (!dataAvailable()) {
        return QByteArray();
    }

    bool foundSlash = false;
    // quoted string
    if (at(m_position) == '"') {
        advance();
        int i = m_position;
        Q_FOREVER {
            if (!dataAvailable(i)) {
                m_position = i;
                return QByteArray();
            }
            if (at(i) == '\\') {
                i += 2;
                foundSlash = true;
                continue;
            }
            if (at(i) == '"') {
                result = mid(m_position, i - m_position);
                end = i + 1; // skip the '"'
                break;
            }
            ++i;
        }
    } else { //unquoted string
        int i = m_position;
        Q_FOREVER {
            if (!dataAvailable(i)) {
                m_position = i;
                return QByteArray();
            }
            const auto c = at(i);
            if (c == ' ' ||
            c == '(' ||
            c == ')' ||
            c == '[' ||
            c == ']' ||
            c == '\n' ||
            c == '\r' ||
            c == '"') {
                end = i;
                break;
            }
            if (at(i) == '\\') {
                foundSlash = true;
            }
            i++;
        }

        result = mid(m_position, end - m_position);
    }

    // strip quotes
    if (foundSlash) {
        while (result.contains("\\\"")) {
            result.replace("\\\"", "\"");
        }
        while (result.contains("\\\\")) {
            result.replace("\\\\", "\\");
        }
    }
    m_position = end;
    return result;
}

qint64 ImapStreamParser::readNumber(bool *ok)
{
    qint64  result;
    if (ok) {
        *ok = false;
    }
    stripLeadingSpaces();
    if (!dataAvailable()) {
        return 0;
    }
    int i = m_position;
    Q_FOREVER {
        if (!dataAvailable(i)) {
            m_position = i;
            return 0;
        }
        if (!isdigit(at(i)))
        {
            break;
        }
        ++i;
    }
    const QByteArray tmp = mid(m_position, i - m_position);
    result = tmp.toLongLong(ok);
    m_position = i;
    return result;
}

void ImapStreamParser::stripLeadingSpaces()
{
    if (!dataAvailable()) {
        return;
    }
    for (int i = m_position; i < length(); ++i) {
        if (at(i) != ' ') {
            m_position = i;
            return;
        }
    }
    m_position = length();
}

bool ImapStreamParser::waitForMoreData(bool wait)
{
    if (wait) {
        if (m_socket->bytesAvailable() > 0 ||
                m_socket->waitForReadyRead(30000)) {
            readFromSocket();
        } else {
            return false;
        }
    }
    return true;
}

void ImapStreamParser::setData(const QByteArray &data)
{
    buffer() = data;
}

QByteArray ImapStreamParser::data() const
{
    return buffer();
}

bool ImapStreamParser::dataAvailable()
{
    return dataAvailable(m_position);
}

bool ImapStreamParser::dataAvailable(int i)
{
    auto gotEnough = i < length();
    if (!gotEnough) {
        if (readFromSocket()) {
            return dataAvailable(i);
        }
    }
    m_insufficientData = !gotEnough;
    return gotEnough;
}

int ImapStreamParser::readFromSocket()
{
    const auto amountToRead = qMin(m_socket->bytesAvailable(), qint64(100));
    if (amountToRead) {
        auto spaceLeft = m_bufferSize - m_readPosition;
        if (amountToRead > spaceLeft) {
            qWarning() << "No space left";
            //We should never get here.
            Q_ASSERT(false);
        } else {
            const auto readBytes = m_socket->read(buffer().data() + m_readPosition, amountToRead);
            if (readBytes < 0) {
                qWarning() << "Failed to read data";
                return 0;
            }
            m_readPosition += readBytes;
            m_insufficientData = false;
            return readBytes;
        }
    } else {
    }
    return 0;
}

void ImapStreamParser::parseStream()
{
    Message message;
    QList<Message::Part> *payload = &message.content;

    if (!dataAvailable()) {
        return;
    }
    saveState();

    while (!atCommandEnd()) {
        if (hasString()) {
            const auto string = readString();
            if (!insufficientData()) {
                if (string == "NIL") {
                    *payload << Message::Part(QList<QByteArray>());
                } else {
                    *payload << Message::Part(string);
                }
            }
        } else if (hasList()) {
            const auto list = readParenthesizedList();
            if (!insufficientData()) {
                *payload << Message::Part(list);
            }
        } else if (hasResponseCode()) {
            payload = &message.responseCode;
        } else if (atResponseCodeEnd()) {
            payload = &message.content;
        } else if (hasLiteral()) {
            QByteArray literal;
            while (!atLiteralEnd()) {
                literal += readLiteralPart();
            }
            if (!insufficientData()) {
                *payload << Message::Part(literal);
            }
        } else {
            //If we get here but didn't run into an insufficient-data condition,
            //then something is wrong.
            if (!insufficientData()) {
                qWarning() << "Inconsistent data: " << data();
                m_socket->close();
                return;
            }
            break;
        }
    }

    if (insufficientData()) {
        restoreState();
    } else {
        trimBuffer();
        Q_ASSERT(responseReceived);
        responseReceived(message);
    }
}

void ImapStreamParser::trimBuffer()
{
    auto remainderSize = m_readPosition - m_position;
    QByteArray *otherBuffer;
    if (m_current == &m_data1) {
        otherBuffer = &m_data2;
    } else {
        otherBuffer = &m_data1;
    }
    otherBuffer->replace(0, remainderSize, buffer().constData() + m_position, remainderSize);
    m_current = otherBuffer;
    m_readPosition = remainderSize;
    m_position = 0;
}

QByteArray ImapStreamParser::readRemainingData()
{
    return mid(m_position);
}

int ImapStreamParser::availableDataSize() const
{
    return m_socket->bytesAvailable() + buffer().size() - m_position;
}

bool ImapStreamParser::atCommandEnd()
{
    int savedPos = m_position;

    stripLeadingSpaces();

    if (!dataAvailable()) {
        m_position = savedPos;
        return false;
    }

    if (at(m_position) == '\n' || at(m_position) == '\r') {
        if (at(m_position) == '\r') {
            advance();
        }
        if (m_position < length() && at(m_position) == '\n') {
            advance();
        }


        return true; //command end
    }
    m_position = savedPos;
    return false; //something else
}

QByteArray ImapStreamParser::readUntilCommandEnd()
{
    QByteArray result;
    int paranthesisBalance = 0;
    Q_FOREVER {
        if (!dataAvailable()) {
            waitForMoreData(true);
        }
        auto beforeLiteral = m_position;
        if (at(m_position) == '{' && hasLiteral()) {
            result.append(mid(beforeLiteral, m_position - beforeLiteral));
            QByteArray literal;
            while (!atLiteralEnd() || m_insufficientData) {
                if (m_insufficientData) {
                    waitForMoreData(true);
                }
                literal += readLiteralPart();
            }
            result += literal;
        }
        if (at(m_position) == '(') {
            paranthesisBalance++;
        }
        if (at(m_position) == ')') {
            paranthesisBalance--;
        }
        if ((m_position == length() && paranthesisBalance == 0) ||
                at(m_position) == '\n'  || at(m_position) == '\r')
        {
            break; //command end
        }
        result.append(at(m_position));
        advance();
    }
    atCommandEnd();
    trimBuffer();
    return result;
}

void ImapStreamParser::sendContinuationResponse(qint64 size)
{
    QByteArray block = "+ Ready for literal data (expecting " +
                       QByteArray::number(size) + " bytes)\r\n";
    m_socket->write(block);
    m_socket->waitForBytesWritten(30000);
}

void ImapStreamParser::onResponseReceived(std::function<void(const Message &)> f)
{
    responseReceived = f;
}
