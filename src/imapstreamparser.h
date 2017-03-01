/*
    Copyright (c) 2006 - 2007 Volker Krause <vkrause@kde.org>
    Copyright (c) 2009 Andras Mantia <amantia@kde.org>
    Copyright (c) 2017 Christian Mollekopf <mollekopf@kolabsys.com>

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
#include <QtCore/QScopedPointer>
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
     * Return everything that remained from the command.
     * @return the remaining command data
     */
    QByteArray readUntilCommandEnd();

    int availableDataSize() const;

    void parseStream();

    void onResponseReceived(std::function<void(const Message &)>);

    bool error() const;

    QByteArray currentBuffer() const;

private:

    /**
     * Remove already read data from the internal buffer if necessary.
     */
    void trimBuffer();

    /**
     * Inform the client to send more literal data.
     */
    void sendContinuationResponse(qint64 size);


    int readFromSocket();
    void processBuffer();

    char at(int pos) const;
    QByteArray mid(int start, int end = -1)  const;
    QByteArray midRef(int start, int end)  const;
    int length() const;

    QByteArray &buffer();
    const QByteArray &buffer() const;

    QScopedPointer<Message> m_message;
    QList<Message::Part> *m_currentPayload;

    QIODevice *m_socket;
    bool m_isServerModeEnabled;
    int m_position;
    int m_readPosition;
    qint64 m_literalSize;
    bool m_insufficientData;
    QByteArray m_data1;
    QByteArray m_data2;
    QByteArray *m_current;
    int m_bufferSize;
    int m_readBlockSize;

    enum States {
        InitState,
        QuotedStringState,
        LiteralStringState,
        StringState,
        WhitespaceState,
        AngleBracketStringState,
        SublistString,
        CRLFState
    };
    States m_currentState;
    States m_lastState;

    void setState(States state);
    void forwardToState(States state);
    void resetState();

    int m_listCounter;
    int m_stringStartPos;
    bool m_readingLiteral;
    bool m_error;

    std::function<void(const char *data, const int size)> string;
    std::function<void(const char c)> listStart;
    std::function<void(const char c)> listEnd;
    std::function<void(int size)> literalStart;
    std::function<void(const char *data, const int size)> literalPart;
    std::function<void()> literalEnd;
    std::function<void()> lineEnd;

    void onString(std::function<void(const char *data, const int size)> f)
    {
        string = f;
    }

    void onListStart(std::function<void(const char c)> f)
    {
        listStart = f;
    }

    void onListEnd(std::function<void(const char c)> f)
    {
        listEnd = f;
    }

    void onLiteralStart(std::function<void(int size)> f)
    {
        literalStart = f;
    }

    void onLiteralPart(std::function<void(const char *data, const int size)> f)
    {
        literalPart = f;
    }

    void onLiteralEnd(std::function<void()> f)
    {
        literalEnd = f;
    }

    void onLineEnd(std::function<void()> f)
    {
        lineEnd = f;
    }

    std::function<void(const Message &)> responseReceived;

    void setupCallbacks();

    QList<QByteArray> *m_list;
    QScopedPointer<QByteArray> m_literalData;
};

}

#endif
