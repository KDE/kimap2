/*
    Copyright (c) 2006 - 2007 Volker Krause <vkrause@kde.org>
    Copyright (c) 2009 Andras Mantia <amantia@kde.org>
    Copyright (c) 2017 Christian Mollekopf <mollekopf@kolabsys.com>

    Copyright (c) 2010 Klarälvdalens Datakonsult AB, a KDAB Group company <info@kdab.com>
    Author: Kevin Ottens <kevin@kdab.com>
    Copyright (c) 2016 Christian Mollekopf <mollekopf@kolabsys.com>

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
#include <stdlib.h>
#include <QIODevice>
#include <QDebug>

using namespace KIMAP2;

ImapStreamParser::ImapStreamParser(QIODevice *socket, bool serverModeEnabled)
    : m_socket(socket),
    m_isServerModeEnabled(serverModeEnabled),
    m_processing(false),
    m_position(0),
    m_readPosition(0),
    m_literalSize(0),
    m_bufferSize(16000),
    m_currentState(InitState),
    m_listCounter(0),
    m_stringStartPos(0),
    m_readingLiteral(false),
    m_error(false),
    m_list(nullptr)
{
    m_data1.resize(m_bufferSize);
    m_data2.resize(m_bufferSize);
    m_current = &m_data1;
    setupCallbacks();
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

QByteArray ImapStreamParser::mid(int start, int len)  const
{
    return buffer().mid(start, len);
}

QByteArray ImapStreamParser::midRef(int start, int len)  const
{
    return QByteArray::fromRawData(buffer().constData() + start, len);
}

int ImapStreamParser::length() const
{
    return m_readPosition;
}

int ImapStreamParser::readFromSocket()
{
    if (m_readingLiteral && !m_isServerModeEnabled) {
        Q_ASSERT(m_currentState == LiteralStringState);
        Q_ASSERT(m_literalSize > 0);
        const auto amountToRead = qMin(m_socket->bytesAvailable(), m_literalSize);
        Q_ASSERT(amountToRead > 0);
        auto pos = m_literalData.size();
        m_literalData.resize(m_literalData.size() + amountToRead);
        const auto readBytes = m_socket->read(m_literalData.data() + pos, amountToRead);
        m_literalSize -= readBytes;
        if (readBytes < 0) {
            qWarning() << "Failed to read data";
            return 0;
        }
        // qDebug() << "Read literal data: " << readBytes << m_literalSize;
        Q_ASSERT(m_literalSize >= 0);
        return readBytes;
    } else {
        if (m_readPosition == m_bufferSize) {
            // qDebug() << "Buffer is full, trimming";
            trimBuffer();
        }
        const auto amountToRead = qMin(m_socket->bytesAvailable(), qint64(m_bufferSize - m_readPosition));
        Q_ASSERT(amountToRead > 0);
        const auto readBytes = m_socket->read(buffer().data() + m_readPosition, amountToRead);
        if (readBytes < 0) {
            qWarning() << "Failed to read data";
            return 0;
        }
        m_readPosition += readBytes;
        // qDebug() << "Buffer: " << buffer().mid(0, m_readPosition);
        // qDebug() << "Read data: " << readBytes;
        return readBytes;
    }
}

void ImapStreamParser::setupCallbacks()
{
    onString([&](const char *data, const int size) {
        if (!m_message) {
            //We just assume that we always get a string first
            m_message.reset(new Message);
            m_currentPayload = &m_message->content;
        }
        if (m_list) {
            *m_list << QByteArray(data, size);
        } else {
            *m_currentPayload << Message::Part(QByteArray(data, size));
        }
    });
    onListStart([&](const char c) {
        if (c == '[') {
            m_currentPayload = &m_message->responseCode;
            return;
        }
        if (!m_list) {
            m_list = new QList<QByteArray>;
        }
    });

    onListEnd([&](const char c) {
        if (c == ']') {
            m_currentPayload = &m_message->content;
            return;
        }
        if (m_listCounter == 0) {
            Q_ASSERT(m_currentPayload);
            Q_ASSERT(m_list);
            *m_currentPayload << Message::Part(*m_list);
            delete m_list;
            m_list = nullptr;
        }
    });
    onLiteralStart([&](const int size) {
        m_literalData.clear();
        m_literalData.reserve(size);
    });
    onLiteralPart([&](const char *data, const int size) {
        m_literalData.append(QByteArray::fromRawData(data, size));
    });
    onLiteralEnd([&]() {
        string(m_literalData.constData(), m_literalData.size());
    });
    onLineEnd([&]() {
        if (m_list || m_listCounter != 0) {
            qWarning() << "List parsing in progress: " << m_listCounter;
            m_error = true;
        }
        if (m_literalSize || m_readingLiteral) {
            qWarning() << "Literal parsing in progress: " << m_literalSize;
            m_error = true;
        }
        Q_ASSERT(responseReceived);
        if (m_message) {
            responseReceived(*m_message);
            m_message.reset(nullptr);
        }
        m_currentPayload = nullptr;
    });
}

void ImapStreamParser::setState(States state)
{
    m_lastState = m_currentState;
    m_currentState = state;
}

void ImapStreamParser::forwardToState(States state)
{
    m_currentState = state;
}

void ImapStreamParser::resetState()
{
    m_currentState = m_lastState;
}

void ImapStreamParser::processBuffer()
{
    if (m_error) {
        qWarning() << "An error occurred";
        return;
    }
    if (m_currentState == LiteralStringState && m_literalSize == 0 && m_readingLiteral) {
        literalEnd();
        resetState();
        m_readingLiteral = false;
    }

    while (m_position < m_readPosition) {
        Q_ASSERT(m_position < length());
        const char c = buffer()[m_position];
        // qDebug() << "Checking :" << c << m_position << m_readPosition << m_currentState << m_listCounter;
        switch (m_currentState) {
            case InitState:
                if (c == '(') {
                    if (m_listCounter >= 1) {
                        //Parse sublists as string
                        setState(SublistString);
                        m_stringStartPos = m_position;
                        m_listCounter++;
                    } else {
                        m_listCounter++;
                        listStart(c);
                    }
                } else if (c == ')') {
                    if (m_listCounter == 0) {
                        qWarning() << "Brackets are off";
                        m_error = true;
                        return;
                    }
                    m_listCounter--;
                    listEnd(c);
                } else if (c == '[') {
                    if (m_listCounter >= 1) {
                        //Inside lists angle brackets are parsed as strings
                        setState(AngleBracketStringState);
                        m_stringStartPos = m_position;
                    } else {
                        //Response code start
                        listStart(c);
                    }
                } else if (c == ']') {
                    //Response code end
                    listEnd(c);
                } else if (c == ' ') {
                    //Skip whitespace
                    setState(WhitespaceState);
                } else if (c == '\r') {
                    setState(CRLFState);
                } else if (c == '{') {
                    setState(LiteralStringState);
                    m_stringStartPos = m_position + 1;
                } else if (c == '\"') {
                    setState(QuotedStringState);
                    m_stringStartPos = m_position + 1;
                } else {
                    setState(StringState);
                    m_stringStartPos = m_position;
                }
                break;
            case QuotedStringState:
                if (c == '\"' && buffer().at(m_position - 1) != '\\') {
                    //Unescaped quote
                    resetState();
                    const auto endPos = m_position;
                    string(buffer().constData() + m_stringStartPos, endPos - m_stringStartPos);
                    m_stringStartPos = 0;
                }
                break;
            case LiteralStringState:
                if (c == '}') {
                    m_literalSize = strtol(buffer().constData() + m_stringStartPos, nullptr, 10);
                    // qDebug() << "Found literal size: " << m_literalSize;
                    literalStart(m_literalSize);
                    m_readingLiteral = false;
                    m_stringStartPos = 0;
                    break;
                }
                if (!m_readingLiteral) {
                    //Skip CRLF after literal size
                    if (c == '\n') {
                        m_readingLiteral = true;
                        if (m_isServerModeEnabled && m_literalSize > 0) {
                            sendContinuationResponse(m_literalSize);
                        }
                    }
                } else {
                    Q_ASSERT(m_position < length());
                    if (m_literalSize) {
                        int size = m_literalSize;
                        if (length() < m_position + size) {
                            //If the literal is not complete we take what is available
                            size = length() - m_position;
                        }
                        literalPart(buffer().constData() + m_position, size);
                        m_position += size;
                        m_literalSize -= size;
                    }
                    if (m_literalSize <= 0) {
                        Q_ASSERT(m_literalSize == 0);
                        literalEnd();
                        resetState();
                        m_readingLiteral = false;
                    }
                    continue;
                }
                break;
            case StringState:
                if (c == ' ' ||
                    c == ')' || //End of list
                    c == '(' || //New list
                    //FIXME because we want to concat in sublists.
                    // c == '[' ||
                    c == ']' ||
                    c == '\r' || //CRLF
                    c == '\"') {
                    resetState();
                    string(buffer().constData() + m_stringStartPos, m_position - m_stringStartPos);
                    m_stringStartPos = 0;
                    continue;
                }
                //Inside lists we want to parse the angle brackets as part of the string.
                if (c == '[') {
                    if (m_listCounter >= 1) {
                        // qDebug() << "Switching to angle bracket state";
                        forwardToState(AngleBracketStringState);
                        break;
                    }
                }
                break;
            case AngleBracketStringState:
                if (c == ']') {
                    resetState();
                    string(buffer().constData() + m_stringStartPos, m_position - m_stringStartPos + 1);
                    m_stringStartPos = 0;
                }
                break;
            case SublistString:
                if (c == '(') {
                    m_listCounter++;
                } else if (c == ')') {
                    m_listCounter--;
                    if (m_listCounter <= 1) {
                        resetState();
                        string(buffer().constData() + m_stringStartPos, m_position - m_stringStartPos + 1);
                        m_stringStartPos = 0;
                    }
                }
                break;
            case WhitespaceState:
                if (c != ' ') {
                    //Skip whitespace
                    resetState();
                    continue;
                }
                break;
            case CRLFState:
                if (c == '\n') {
                    lineEnd();
                    resetState();
                } else {
                    //Skip over the \r that isn't part of the CRLF
                    resetState();
                    continue;
                }
                break;
        }
        m_position++;
    }
}

void ImapStreamParser::parseStream()
{
    if (m_processing) {
        return;
    }
    if (m_error) {
        qWarning() << "An error occurred";
        return;
    }
    m_processing = true;
    while (m_socket->bytesAvailable()) {
        if (readFromSocket() <= 0) {
            //If we're not making progress we could loop forever,
            //and given that we check beforehand if there is data,
            //this should never happen.
            qWarning() << "Read nothing from the socket.";
            m_error = true;
            Q_ASSERT(false);
            return;
        };
        processBuffer();
    }
    m_processing = false;
}

void ImapStreamParser::trimBuffer()
{
    int offset = m_position;
    if (m_stringStartPos) {
        offset = qMin(m_stringStartPos, m_position);
    }

    auto remainderSize = m_readPosition - offset;
    Q_ASSERT( remainderSize >= 0);
    QByteArray *otherBuffer;
    if (m_current == &m_data1) {
        otherBuffer = &m_data2;
    } else {
        otherBuffer = &m_data1;
    }
    if (remainderSize) {
        otherBuffer->replace(0, remainderSize, buffer().constData() + offset, remainderSize);
    }
    m_current = otherBuffer;
    m_readPosition = remainderSize;
    m_position -= offset;
    if (m_stringStartPos) {
        m_stringStartPos -= offset;
    }
    // qDebug() << "Buffer after trim: " << mid(0, m_readPosition);
}

int ImapStreamParser::availableDataSize() const
{
    return m_socket->bytesAvailable() + length() - m_position;
}

QByteArray ImapStreamParser::readUntilCommandEnd()
{
    QByteArray result;
    auto startPos = m_position;
    onLineEnd([&result, this, startPos]() {
        result = mid(startPos, m_position - startPos - 1);
    });
    Q_FOREVER {
        if (!m_socket->bytesAvailable()) {
            if (!m_socket->waitForReadyRead(10000)) {
                qWarning() << "No data available";
                return result;
            }
        }
        parseStream();
        if (!result.isEmpty() && m_currentState == InitState) {
            // qDebug() << "Got a result: " << m_readingLiteral;
            // result.append(m_literalData);
            break;
        }
    }
    qDebug() << "Read until command end: " << result;
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

bool ImapStreamParser::error() const
{
    return m_error;
}

QByteArray ImapStreamParser::currentBuffer() const
{
    return mid(0, m_readPosition);
}
