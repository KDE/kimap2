/*
    Copyright (c) 2009 Kevin Ottens <ervin@kde.org>
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

#include "loginjob.h"

#include "kimap_debug.h"

#include "job_p.h"
#include "message_p.h"
#include "session_p.h"
#include "rfccodecs.h"

#include "common.h"

extern "C" {
#include <sasl/sasl.h>
}

static const sasl_callback_t callbacks[] = {
    { SASL_CB_ECHOPROMPT, Q_NULLPTR, nullptr },
    { SASL_CB_NOECHOPROMPT, Q_NULLPTR, nullptr },
    { SASL_CB_GETREALM, Q_NULLPTR, nullptr },
    { SASL_CB_USER, Q_NULLPTR, nullptr },
    { SASL_CB_AUTHNAME, Q_NULLPTR, nullptr },
    { SASL_CB_PASS, Q_NULLPTR, nullptr },
    { SASL_CB_CANON_USER, Q_NULLPTR, nullptr },
    { SASL_CB_LIST_END, Q_NULLPTR, nullptr }
};

namespace KIMAP2
{
class LoginJobPrivate : public JobPrivate
{
public:
    enum AuthState {
        StartTls = 0,
        Capability,
        Login,
        Authenticate
    };

    LoginJobPrivate(LoginJob *job, Session *session, const QString &name) : JobPrivate(session, name), q(job), encryptionMode(QSsl::UnknownProtocol), authState(Login), plainLoginDisabled(false)
    {
        conn = Q_NULLPTR;
        client_interact = Q_NULLPTR;
    }
    ~LoginJobPrivate() { }
    bool sasl_interact();

    bool startAuthentication();
    void sendPlainLogin();
    bool answerChallenge(const QByteArray &data);
    void sslResponse(bool response);
    void saveServerGreeting(const Message &response);

    LoginJob *q;

    QString userName;
    QString authorizationName;
    QString password;
    QString serverGreeting;

    QSsl::SslProtocol encryptionMode;
    bool startTls;
    QString authMode;
    AuthState authState;
    QStringList capabilities;
    bool plainLoginDisabled;

    sasl_conn_t *conn;
    sasl_interact_t *client_interact;
};
}

using namespace KIMAP2;

bool LoginJobPrivate::sasl_interact()
{
    qCDebug(KIMAP2_LOG) << "sasl_interact";
    sasl_interact_t *interact = client_interact;

    //some mechanisms do not require username && pass, so it doesn't need a popup
    //window for getting this info
    for (; interact->id != SASL_CB_LIST_END; interact++) {
        if (interact->id == SASL_CB_AUTHNAME ||
                interact->id == SASL_CB_PASS) {
            //TODO: dialog for use name??
            break;
        }
    }

    interact = client_interact;
    while (interact->id != SASL_CB_LIST_END) {
        qCDebug(KIMAP2_LOG) << "SASL_INTERACT id:" << interact->id;
        switch (interact->id) {
        case SASL_CB_AUTHNAME:
            if (!authorizationName.isEmpty()) {
                qCDebug(KIMAP2_LOG) << "SASL_CB_[AUTHNAME]: '" << authorizationName << "'";
                interact->result = strdup(authorizationName.toUtf8());
                interact->len = strlen((const char *) interact->result);
                break;
            }
        case SASL_CB_USER:
            qCDebug(KIMAP2_LOG) << "SASL_CB_[USER|AUTHNAME]: '" << userName << "'";
            interact->result = strdup(userName.toUtf8());
            interact->len = strlen((const char *) interact->result);
            break;
        case SASL_CB_PASS:
            qCDebug(KIMAP2_LOG) << "SASL_CB_PASS: [hidden]";
            interact->result = strdup(password.toUtf8());
            interact->len = strlen((const char *) interact->result);
            break;
        default:
            interact->result = Q_NULLPTR;
            interact->len = 0;
            break;
        }
        interact++;
    }
    return true;
}

LoginJob::LoginJob(Session *session)
    : Job(*new LoginJobPrivate(this, session, QString::fromUtf8("Login")))
{
    Q_D(LoginJob);
    connect(d->sessionInternal(), SIGNAL(encryptionNegotiationResult(bool)), this, SLOT(sslResponse(bool)));
    qCDebug(KIMAP2_LOG) << this;
}

LoginJob::~LoginJob()
{
    qCDebug(KIMAP2_LOG) << this;
}

QString LoginJob::userName() const
{
    Q_D(const LoginJob);
    return d->userName;
}

void LoginJob::setUserName(const QString &userName)
{
    Q_D(LoginJob);
    d->userName = userName;
}

QString LoginJob::authorizationName() const
{
    Q_D(const LoginJob);
    return d->authorizationName;
}

void LoginJob::setAuthorizationName(const QString &authorizationName)
{
    Q_D(LoginJob);
    d->authorizationName = authorizationName;
}

QString LoginJob::password() const
{
    Q_D(const LoginJob);
    return d->password;
}

void LoginJob::setPassword(const QString &password)
{
    Q_D(LoginJob);
    d->password = password;
}

void LoginJob::doStart()
{
    Q_D(LoginJob);

    qCDebug(KIMAP2_LOG) << "doStart" << this;
    // Don't authenticate on a session in the authenticated state
    if (session()->state() == Session::Authenticated || session()->state() == Session::Selected) {
        setError(UserDefinedError);
        setErrorText(QString::fromUtf8("IMAP session in the wrong state for authentication"));
        emitResult();
        return;
    }

    if (d->encryptionMode == QSsl::UnknownProtocol) {
        qCDebug(KIMAP2_LOG) << "Starting with plain";
        if (d->authMode.isEmpty()) {
            d->sendPlainLogin();
        } else {
            if (!d->startAuthentication()) {
                emitResult();
            }
        }
    } else {
        if (d->startTls) {
            qCDebug(KIMAP2_LOG) << "Starting with tls";
            d->authState = LoginJobPrivate::StartTls;
            d->tags << d->sessionInternal()->sendCommand("STARTTLS");
        } else {
            qCDebug(KIMAP2_LOG) << "Starting with Ssl";
            d->sessionInternal()->startSsl(d->encryptionMode);
        }
    }
}

void LoginJob::handleResponse(const Message &response)
{
    Q_D(LoginJob);

    if (response.content.isEmpty()) {
        return;
    }

    //set the actual command name for standard responses
    QString commandName = QStringLiteral("Login");
    if (d->authState == LoginJobPrivate::Capability) {
        commandName = QStringLiteral("Capability");
    } else if (d->authState == LoginJobPrivate::StartTls) {
        commandName = QStringLiteral("StartTls");
    }

    enum ResponseCode {
        OK,
        ERR,
        UNTAGGED,
        CONTINUATION,
        MALFORMED
    };

    QByteArray tag = response.content.first().toString();
    ResponseCode code = OK;

    qCDebug(KIMAP2_LOG) << commandName << tag;

    if (tag == "+") {
        code = CONTINUATION;
    } else if (tag == "*") {
        if (response.content.size() < 2) {
            code = MALFORMED; // Received empty untagged response
        } else {
            code = UNTAGGED;
        }
    } else if (d->tags.contains(tag)) {
        if (response.content.size() < 2) {
            code = MALFORMED;
        } else if (response.content[1].toString() == "OK") {
            code = OK;
        } else {
            code = ERR;
        }
    }

    switch (code) {
    case MALFORMED:
        // We'll handle it later
        break;

    case ERR:
        //server replied with NO or BAD for SASL authentication
        if (d->authState == LoginJobPrivate::Authenticate) {
            sasl_dispose(&d->conn);
        }

        setError(UserDefinedError);
        setErrorText(QString("%1 failed, server replied: %2").arg(commandName).arg(QLatin1String(response.toString().constData())));
        emitResult();
        return;

    case UNTAGGED:
        // The only untagged response interesting for us here is CAPABILITY
        if (response.content[1].toString() == "CAPABILITY") {
            QList<Message::Part>::const_iterator p = response.content.begin() + 2;
            while (p != response.content.end()) {
                QString capability = QLatin1String(p->toString());
                d->capabilities << capability;
                if (capability == QLatin1String("LOGINDISABLED")) {
                    d->plainLoginDisabled = true;
                }
                ++p;
            }
            qCDebug(KIMAP2_LOG) << "Capabilities updated: " << d->capabilities;
        }
        break;

    case CONTINUATION:
        if (d->authState != LoginJobPrivate::Authenticate) {
            // Received unexpected continuation response for something
            // other than AUTHENTICATE command
            code = MALFORMED;
            break;
        }

        if (d->authMode == QLatin1String("PLAIN")) {
            if (response.content.size() > 1 && response.content.at(1).toString() == "OK") {
                return;
            }
            QByteArray challengeResponse;
            if (!d->authorizationName.isEmpty()) {
                challengeResponse += d->authorizationName.toUtf8();
            }
            challengeResponse += '\0';
            challengeResponse += d->userName.toUtf8();
            challengeResponse += '\0';
            challengeResponse += d->password.toUtf8();
            challengeResponse = challengeResponse.toBase64();
            d->sessionInternal()->sendData(challengeResponse);
        } else if (response.content.size() >= 2) {
            if (!d->answerChallenge(QByteArray::fromBase64(response.content[1].toString()))) {
                emitResult(); //error, we're done
            }
        } else {
            // Received empty continuation for authMode other than PLAIN
            code = MALFORMED;
        }
        break;

    case OK:

        switch (d->authState) {
        case LoginJobPrivate::StartTls:
            d->sessionInternal()->startSsl(d->encryptionMode);
            break;
        case LoginJobPrivate::Capability:
            //cleartext login, if enabled
            if (d->authMode.isEmpty()) {
                if (d->plainLoginDisabled) {
                    setError(UserDefinedError);
                    setErrorText(QString("Login failed, plain login is disabled by the server."));
                    emitResult();
                } else {
                    d->sendPlainLogin();
                }
            } else {
                bool authModeSupported = false;
                //find the selected SASL authentication method
                Q_FOREACH (const QString &capability, d->capabilities) {
                    if (capability.startsWith(QLatin1String("AUTH="))) {
                        if (capability.mid(5) == d->authMode) {
                            authModeSupported = true;
                            break;
                        }
                    }
                }
                if (!authModeSupported) {
                    setError(UserDefinedError);
                    setErrorText(QString("Login failed, authentication mode %1 is not supported by the server.").arg(d->authMode));
                    emitResult();
                } else if (!d->startAuthentication()) {
                    emitResult(); //problem, we're done
                }
            }
            break;

        case LoginJobPrivate::Authenticate:
            sasl_dispose(&d->conn);   //SASL authentication done
        // Fall through
        case LoginJobPrivate::Login:
            d->saveServerGreeting(response);
            emitResult(); //got an OK, command done
            break;

        }

    }

    if (code == MALFORMED) {
        setErrorText(QString("%1 failed, malformed reply from the server.").arg(commandName));
        emitResult();
    }
}

bool LoginJobPrivate::startAuthentication()
{
    //SASL authentication
    if (!initSASL()) {
        q->setError(LoginJob::UserDefinedError);
        q->setErrorText(QString("Login failed, client cannot initialize the SASL library."));
        return false;
    }

    authState = LoginJobPrivate::Authenticate;
    const char *out = Q_NULLPTR;
    uint outlen = 0;
    const char *mechusing = Q_NULLPTR;

    int result = sasl_client_new("imap", m_session->hostName().toLatin1(), Q_NULLPTR, nullptr, callbacks, 0, &conn);
    if (result != SASL_OK) {
        qCDebug(KIMAP2_LOG) << "sasl_client_new failed with:" << result;
        q->setError(LoginJob::UserDefinedError);
        q->setErrorText(QString::fromUtf8(sasl_errdetail(conn)));
        return false;
    }

    do {
        result = sasl_client_start(conn, authMode.toLatin1(), &client_interact, capabilities.contains(QStringLiteral("SASL-IR")) ? &out : Q_NULLPTR, &outlen, &mechusing);

        if (result == SASL_INTERACT) {
            if (!sasl_interact()) {
                sasl_dispose(&conn);
                q->setError(LoginJob::UserDefinedError);   //TODO: check up the actual error
                return false;
            }
        }
    } while (result == SASL_INTERACT);

    if (result != SASL_CONTINUE && result != SASL_OK) {
        qCDebug(KIMAP2_LOG) << "sasl_client_start failed with:" << result;
        q->setError(LoginJob::UserDefinedError);
        q->setErrorText(QString::fromUtf8(sasl_errdetail(conn)));
        sasl_dispose(&conn);
        return false;
    }

    QByteArray tmp = QByteArray::fromRawData(out, outlen);
    QByteArray challenge = tmp.toBase64();

    if (challenge.isEmpty()) {
        tags << sessionInternal()->sendCommand("AUTHENTICATE", authMode.toLatin1());
    } else {
        tags << sessionInternal()->sendCommand("AUTHENTICATE", authMode.toLatin1() + ' ' + challenge);
    }

    return true;
}

void LoginJobPrivate::sendPlainLogin()
{
    authState = LoginJobPrivate::Login;
    qCDebug(KIMAP2_LOG) << "sending LOGIN";
    tags << sessionInternal()->sendCommand("LOGIN",
            '"' + quoteIMAP(userName).toUtf8() + '"' +
            ' ' +
            '"' + quoteIMAP(password).toUtf8() + '"');
}

bool LoginJobPrivate::answerChallenge(const QByteArray &data)
{
    QByteArray challenge = data;
    int result = -1;
    const char *out = Q_NULLPTR;
    uint outlen = 0;
    do {
        result = sasl_client_step(conn, challenge.isEmpty() ? Q_NULLPTR : challenge.data(),
                                  challenge.size(),
                                  &client_interact,
                                  &out, &outlen);

        if (result == SASL_INTERACT) {
            if (!sasl_interact()) {
                q->setError(LoginJob::UserDefinedError);   //TODO: check up the actual error
                sasl_dispose(&conn);
                return false;
            }
        }
    } while (result == SASL_INTERACT);

    if (result != SASL_CONTINUE && result != SASL_OK) {
        qCDebug(KIMAP2_LOG) << "sasl_client_step failed with:" << result;
        q->setError(LoginJob::UserDefinedError);   //TODO: check up the actual error
        q->setErrorText(QString::fromUtf8(sasl_errdetail(conn)));
        sasl_dispose(&conn);
        return false;
    }

    QByteArray tmp = QByteArray::fromRawData(out, outlen);
    challenge = tmp.toBase64();

    sessionInternal()->sendData(challenge);

    return true;
}

void LoginJobPrivate::sslResponse(bool response)
{
    if (response) {
        authState = LoginJobPrivate::Capability;
        tags << sessionInternal()->sendCommand("CAPABILITY");
    } else {
        q->setError(LoginJob::UserDefinedError);
        q->setErrorText(QString::fromUtf8("Login failed, TLS negotiation failed."));
        encryptionMode = QSsl::UnknownProtocol;
        q->emitResult();
    }
}

void LoginJob::setEncryptionMode(QSsl::SslProtocol mode, bool startTls)
{
    Q_D(LoginJob);
    d->encryptionMode = mode;
    d->startTls = startTls;
}

QSsl::SslProtocol LoginJob::encryptionMode()
{
    Q_D(LoginJob);
    return d->encryptionMode;
}

void LoginJob::setAuthenticationMode(AuthenticationMode mode)
{
    Q_D(LoginJob);
    switch (mode) {
    case ClearText: d->authMode = QLatin1String("");
        break;
    case Login: d->authMode = QStringLiteral("LOGIN");
        break;
    case Plain: d->authMode = QStringLiteral("PLAIN");
        break;
    case CramMD5: d->authMode = QStringLiteral("CRAM-MD5");
        break;
    case DigestMD5: d->authMode = QStringLiteral("DIGEST-MD5");
        break;
    case GSSAPI: d->authMode = QStringLiteral("GSSAPI");
        break;
    case Anonymous: d->authMode = QStringLiteral("ANONYMOUS");
        break;
    case XOAuth2: d->authMode = QStringLiteral("XOAUTH2");
        break;
    default:
        d->authMode = QStringLiteral("");
    }
}

void LoginJob::connectionLost()
{
    Q_D(LoginJob);

    qCWarning(KIMAP2_LOG) << "Connection to server lost " << d->m_socketError;
    if (d->m_socketError == QSslSocket::SslHandshakeFailedError) {
        setError(ERR_SSL_HANDSHAKE_FAILED);
        setErrorText(QString::fromUtf8("SSL handshake failed."));
        emitResult();
    } else {
        setError(ERR_COULD_NOT_CONNECT);
        setErrorText(QString::fromUtf8("Connection to server lost."));
        emitResult();
    }
}

void LoginJobPrivate::saveServerGreeting(const Message &response)
{
    // Concatenate the parts of the server response into a string, while dropping the first two parts
    // (the response tag and the "OK" code), and being careful not to add useless extra whitespace.

    for (int i = 2; i < response.content.size(); i++) {
        if (response.content.at(i).type() == Message::Part::List) {
            serverGreeting += QLatin1Char('(');
            foreach (const QByteArray &item, response.content.at(i).toList()) {
                serverGreeting += QLatin1String(item) + QLatin1Char(' ');
            }
            serverGreeting.chop(1);
            serverGreeting += QStringLiteral(") ");
        } else {
            serverGreeting += QLatin1String(response.content.at(i).toString()) + QLatin1Char(' ');
        }
    }
    serverGreeting.chop(1);
}

QString LoginJob::serverGreeting() const
{
    Q_D(const LoginJob);
    return d->serverGreeting;
}

#include "moc_loginjob.cpp"
