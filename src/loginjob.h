/*
    Copyright (c) 2009 Kevin Ottens <ervin@kde.org>
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

#ifndef KIMAP2_LOGINJOB_H
#define KIMAP2_LOGINJOB_H

#include "kimap2_export.h"

#include "job.h"
#include <QtNetwork/QSsl>

namespace KIMAP2
{

class Session;
struct Message;
class LoginJobPrivate;

class KIMAP2_EXPORT LoginJob : public Job
{
    Q_OBJECT
    Q_DECLARE_PRIVATE(LoginJob)

    friend class SessionPrivate;

public:

    enum AuthenticationMode {
        ClearText = 0,
        Login,
        Plain,
        CramMD5,
        DigestMD5,
        NTLM,
        GSSAPI,
        Anonymous,
        XOAuth2
    };

    explicit LoginJob(Session *session);
    virtual ~LoginJob();

    QString userName() const;
    void setUserName(const QString &userName);

    /**
     * Get the authorization identity.
     * @since 4.10
     */
    QString authorizationName() const;

    /**
     * Set the authorization identity.
     *
     * If set, proxy-authentication according to RFC4616 will be used.
     *
     * Note that this feature only works with the "PLAIN" AuthenticationMode.
     *
     * The @param authorizationName will be used together with the password() to get authenticated as userName() by the authorization of the provided credentials.
     * This allows to login as a user using the admin credentials and the users name.
     * @since 4.10
     */
    void setAuthorizationName(const QString &authorizationName);

    QString password() const;
    void setPassword(const QString &password);

    /**
     * Returns the server greeting, in case of a successful login.
     * If the login wasn't successful, this method returns an empty string. Use errorString() to
     * get the error message in this case.
     *
     * Note that the content of this response is not defined by the IMAP protocol and is
     * implementation-dependent.
     * @since 4.7
     */
    QString serverGreeting() const;

    /**
     * Set the encryption mode for the connection. In case an encryption mode is set, the caller
     * MUST check the encryptionMode() result after executing the job, to see if the connection is
     * encrypted or not (e.g handshaking failed).
     * @param mode the encryption mode, see EncryptionModes
     */
    void setEncryptionMode(QSsl::SslProtocol mode, bool startTls = false);

    /**
      Get the encryption mode.
      @return the currently active encryption mode
    */
    QSsl::SslProtocol encryptionMode();

    void setAuthenticationMode(AuthenticationMode mode);

protected:
    void doStart() Q_DECL_OVERRIDE;
    void handleResponse(const Message &response) Q_DECL_OVERRIDE;
    void connectionLost() Q_DECL_OVERRIDE;

private:
    Q_PRIVATE_SLOT(d_func(), void sslResponse(bool))
};

}

#endif
