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
{
    m_socket = socket;
    m_isServerModeEnabled = serverModeEnabled;
    m_position = 0;
    m_literalSize = 0;
    m_insufficientData = false;
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
    const auto c = m_data.at(pos);
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

bool ImapStreamParser::hasLiteral()
{
    if (!dataAvailable()) {
        return false;
    }
    int savedPos = m_position;
    stripLeadingSpaces();
    if (m_data.at(m_position) == '{') {
        int end = m_data.indexOf('}', m_position);
        if (end == -1) {
            //Couldn't find end
            m_position = savedPos;
            return false;
        }
        Q_ASSERT(end > m_position);
        m_literalSize = m_data.mid(m_position + 1, end - m_position - 1).toInt();
        // strip CRLF
        m_position = end + 1;
        // ensure that the CRLF is available
        if (!dataAvailable(m_position + 1)) {
            m_position = savedPos;
            return false;
        }
        if (m_position < m_data.length() && m_data.at(m_position) == '\r') {
            ++m_position;
        }
        if (m_position < m_data.length() && m_data.at(m_position) == '\n') {
            ++m_position;
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

    if (m_data.length() < m_position + size) {   // Still not enough data
        // Take what's already there
        size = m_data.length() - m_position;
    }

    QByteArray result = m_data.mid(m_position, size);
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
    if (m_data.at(pos) == '(') {
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
    if (m_data.at(m_position) != '(') {
        return result; //no list found
    }

    bool concatToLast = false;
    int count = 0;
    int sublistbegin = m_position;
    int i = m_position + 1;
    Q_FOREVER {
        if (!dataAvailable(i)) {
            m_position = i;
            return QList<QByteArray>();
        }
        if (m_data.at(i) == '(') {
            ++count;
            if (count == 1) {
                sublistbegin = i;
            }
            ++i;
            continue;
        }
        if (m_data.at(i) == ')')
        {
            if (count <= 0) {
                m_position = i + 1;
                return result;
            }
            if (count == 1) {
                result.append(m_data.mid(sublistbegin, i - sublistbegin + 1));
            }
            --count;
            ++i;
            continue;
        }
        if (m_data.at(i) == ' ')
        {
            ++i;
            continue;
        }
        if (m_data.at(i) == '"')
        {
            if (count > 0) {
                m_position = i;
                parseQuotedString();
                i = m_position;
                continue;
            }
        }
        if (m_data.at(i) == '[')
        {
            concatToLast = true;
            if (result.isEmpty()) {
                result.append(QByteArray());
            }
            result.last() += '[';
            ++i;
            continue;
        }
        if (m_data.at(i) == ']')
        {
            concatToLast = false;
            result.last() += ']';
            ++i;
            continue;
        }
        if (count == 0)
        {
            m_position = i;
            QByteArray ba;
            if (hasLiteral()) {
                while (!atLiteralEnd()) {
                    ba += readLiteralPart();
                    if (m_insufficientData) {
                        return QList<QByteArray>();
                    }
                }
            } else {
                ba = readString();
                if (m_insufficientData) {
                    return QList<QByteArray>();
                }
            }

            // We might sometime get some unwanted CRLF, but we're still not at the end
            // of the list, would make further string reads fail so eat the CRLFs.
            while ((m_position < m_data.size()) &&
                    (m_data.at(m_position) == '\r' || m_data.at(m_position) == '\n')) {
                m_position++;
            }

            i = m_position - 1;
            if (concatToLast) {
                result.last() += ba;
            } else {
                result.append(ba);
            }
        }
        ++i;
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
    if (m_data.at(pos) == c) {
        m_position = pos + 1;
        return true;
    }
    return false;
}

bool ImapStreamParser::hasResponseCode()
{
    return advanceOver('[');
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
    if (m_data.at(m_position) == '"') {
        ++m_position;
        int i = m_position;
        Q_FOREVER {
            if (!dataAvailable(i)) {
                m_position = i;
                return QByteArray();
            }
            if (m_data.at(i) == '\\') {
                i += 2;
                foundSlash = true;
                continue;
            }
            if (m_data.at(i) == '"') {
                result = m_data.mid(m_position, i - m_position);
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
            const auto c = m_data.at(i);
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
            if (m_data.at(i) == '\\') {
                foundSlash = true;
            }
            i++;
        }

        result = m_data.mid(m_position, end - m_position);
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
        if (!isdigit(m_data.at(i)))
        {
            break;
        }
        ++i;
    }
    const QByteArray tmp = m_data.mid(m_position, i - m_position);
    result = tmp.toLongLong(ok);
    m_position = i;
    return result;
}

bool ImapStreamParser::dataAvailable()
{
    return dataAvailable(m_position);
}

bool ImapStreamParser::dataAvailable(int i)
{
    m_insufficientData = !(i < m_data.length());
    return !m_insufficientData;
}

void ImapStreamParser::stripLeadingSpaces()
{
    if (!dataAvailable()) {
        return;
    }
    for (int i = m_position; i < m_data.length(); ++i) {
        if (m_data.at(i) != ' ') {
            m_position = i;
            return;
        }
    }
    m_position = m_data.length();
}

bool ImapStreamParser::waitForMoreData(bool wait)
{
    if (wait) {
        if (m_socket->bytesAvailable() > 0 ||
                m_socket->waitForReadyRead(30000)) {
            const auto data = m_socket->readAll();
            m_data.append(data);
            m_insufficientData = false;
        } else {
            return false;
        }
    }
    return true;
}

void ImapStreamParser::setData(const QByteArray &data)
{
    m_data = data;
}

QByteArray ImapStreamParser::data() const
{
    return m_data;
}

bool ImapStreamParser::parse()
{
    m_data.append(m_socket->readAll());
    m_insufficientData = false;
    return m_data.contains("\r\n");
}

QByteArray ImapStreamParser::readRemainingData()
{
    return m_data.mid(m_position);
}

int ImapStreamParser::availableDataSize() const
{
    return m_socket->bytesAvailable() + m_data.size() - m_position;
}

bool ImapStreamParser::atCommandEnd()
{
    int savedPos = m_position;

    stripLeadingSpaces();

    if (!dataAvailable()) {
        m_position = savedPos;
        return false;
    }

    if (m_data.at(m_position) == '\n' || m_data.at(m_position) == '\r') {
        if (m_data.at(m_position) == '\r') {
            ++m_position;
        }
        if (m_position < m_data.length() && m_data.at(m_position) == '\n') {
            ++m_position;
        }


        return true; //command end
    }
    m_position = savedPos;
    return false; //something else
}

QByteArray ImapStreamParser::readUntilCommandEnd()
{
    QByteArray result;
    int i = m_position;
    int paranthesisBalance = 0;
    Q_FOREVER {
        parse();
        if (!dataAvailable(i)) {
            waitForMoreData(true);
        }
        if (m_data.at(i) == '{')
        {
            m_position = i - 1;
            hasLiteral(); //init literal size
            result.append( m_data.mid( i, m_position + 1 ) );
            while (!atLiteralEnd()) {
                const auto data = readLiteralPart();
                if (m_insufficientData) {
                    waitForMoreData(true);
                } else {
                    result.append(data);
                }
            }
            i = m_position;
        }
        if (m_data.at(i) == '(')
        {
            paranthesisBalance++;
        }
        if (m_data.at(i) == ')')
        {
            paranthesisBalance--;
        }
        if ((i == m_data.length() && paranthesisBalance == 0) ||
                m_data.at(i) == '\n'  || m_data.at(i) == '\r')
        {
            break; //command end
        }
        result.append(m_data.at(i));
        ++i;
    }
    m_position = i;
    atCommandEnd();
    return result;
}

void ImapStreamParser::sendContinuationResponse(qint64 size)
{
    QByteArray block = "+ Ready for literal data (expecting " +
                       QByteArray::number(size) + " bytes)\r\n";
    m_socket->write(block);
    m_socket->waitForBytesWritten(30000);
}

void ImapStreamParser::trimBuffer()
{
    if (m_position < 4096) {   // right() is expensive, so don't do it for every line
        return;
    }
    m_data = m_data.right(m_data.size() - m_position);
    m_position = 0;
}
