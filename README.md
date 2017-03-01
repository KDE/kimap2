# KIMAP #

This library provides a job-based API for interacting with an IMAP4rev1 server.
It manages connections, encryption and parameter quoting and encoding, but
otherwise provides quite a low-level interface to the protocol.  This library
does not implement an IMAP client; it merely makes it easier to do so.

Users should be familiar with [RFC 3501](http://www.apps.ietf.org/rfc/rfc3501.html "IMAP 4rev1")
as well as [other related RFCs](http://www.iana.org/assignments/imap4-capabilities)
although the library hides some of the nastier details like the encoding and quoting of
strings.

## Porting from KIMAP ##
The following changes can affect you when porting from KIMAP (this is probably an incomplete list, so beware)
* Reading from the socket and parsing no longer happens in a separate thread. If you don't want to block the main process execute the complete job in a thread. 
* Most deprecated API's have been removed. See especially SearchJob, ListJob and FetchJob.
* Due to the removal of KTcpSocket we no longer integrate with KSslCertificateManager. If that is wanted use Session::sslErrors signal to react to ssl problems:

    QObject::connect(mSession, &KIMAP2::Session::sslErrors, [this](const QList<QSslError> &errors) {
        SinkLog() << "Received ssl error: " << errors;
        mSession->ignoreErrors(errors);
    });

    QSslSocket::addDefaultCaCertificate can be used to add additional certificates to be used.

* The LoginJob API changed.
