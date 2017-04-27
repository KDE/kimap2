# KIMAP2 #

This library provides a job-based API for interacting with an IMAP4rev1 server.
It manages connections, encryption and parameter quoting and encoding, but
otherwise provides quite a low-level interface to the protocol.  This library
does not implement an IMAP client; it merely makes it easier to do so.

Users should be familiar with [RFC 3501](http://www.apps.ietf.org/rfc/rfc3501.html "IMAP 4rev1")
as well as [other related RFCs](http://www.iana.org/assignments/imap4-capabilities)
although the library hides some of the nastier details like the encoding and quoting of
strings.

For development coordination see: https://phabricator.kde.org/tag/kimap2/

## Porting from KIMAP(1) ##
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

## Versioning and branching ##
Development happens in master. There is one release branch per minor version (if required).
Every release is tagged with a tag of the form "v.$MAJOR.$MINOR.$PATCH".

## Releases ##
KIMAP2 is released as required and does not follow a specific release schedule.
Releases *always* happen directly from a release tag.

A tarball can be created like so:
    git archive --format=tar.xz --prefix=kimap2-0.1.1/ v0.1.1 > kimap2-0.1.1.tar.xz

You may need to add the following to your ~/.gitconfig for the xz compression to be available:
    [tar "tar.xz"]
        command = xz -c

Generate a checksum to be included in the announcement:
    sha256sum kimap2-0.1.1.tar.xz

Tarballs should be uploaded to unstable/kimap2/$VERSION/src/kimap2-$version.tar.xz
    curl -T kimap2-0.1.0.tar.xz ftp://upload.kde.org/incoming/

Request the move to the target location via sysadmin ticket.

See ftp://upload.kde.org/README

For more information see techbase.kde.org/ReleasingExtragearSoftware

## Key changes from KIMAP(1) ##

These are some of larger changes that spawned KIMAP2:

* KIMAP2 is now threading free:
There no longer is a dedicated thread for each socket. The reasoning for
this is manyfold:
    * The threading strategy should be up to the application. The
    current design imposes additional threads in any case.
    * The threading resulted in lot's of subtle bugs and code-complexity
    overhead for the synchronization.
    * The whole design revolved around the idea that by running the
    parser in a thread the parser can be blocking and the network will
    always be the limiting factor. That assumption is not necessarily
    true and the parser starts to behave very badly if we get data
    faster than we can process it (The internal buffer will get large
    and a majority of time will be spent on memcopying during the trim()
    call).
    * This design doesn't allow the consumer to regulate the amount of
    data that enters the system. It reads as much into memory as it can,
    which again assumes the network is the slowest part.
    * During benchmarking of Sink the parser actually became the
    bottleneck of the imap resource (because of the pathological
    behaviour with memcopy in trim()).
All of this directly leads to the next point.

* We have now a non-blocking parser:
    * The parser operates with two buffers to emulate a ringbuffer =>
    fixed memory usage (except for some dynamic allocation for large
    literals).
    * The parser never reads more data than it can process => If we
    can't process fast enough the socket buffer will eventually fill up,
    resulting in the server eventually stopping to send more data. Which
    is how the network stack is supposed to work IMO.
    * We open up possibilities for new streaming API to directly stream
    i.e. attachments to disk.
    * This resulted in the parser mostly vanishing from benchmarks and
    memcpy to vanish entirely.

* We no longer depend on ki18n:
    * This is a protocol implementation and not a place for translated
    strings.

* We no longer depend on kio:
    * This was used for ktcpsocket only, and the only remaining benefit
    of that was the KSslCertificateManager integration. This can easily
    be readded externally by implementing the Session::sslErrors
    callback and the Session::ignoreErrors function, as well as
    QSslSOckets default ca certificate functions. (We may want to extend
    the API there a bit).
    * KIO has a whole slew of unrelated dependencies so this was an
    important step to make KIMAP2 more selfcontained.

* I got rid of a lot of the result batching code:
    * It complicated the code and was an optimization at the wrong level
    IMO to save a couple of function calls.

* Less overloaded result signals with lot's of parameters. One signal
with a result struct containing all data.

* The login job has received an overhaul.
    * Removed the slightly confused EncryptionMode names that sometime
    mixed the excryption and the use of starttls (which are two largely
    unrelated things).
    * Cleaned up the login logic which was a very complex statemachine
    scattered accross different classes and is now only a complex
    statemachine scattered over fewer classes =(
    * Fixed a potential race-condition where we would send a CAPABILITY
    request before receiving the greeting, which some servers seem to
    handle anyways but some don't.
    * Removed the encryption negotation which is handled by QSslSocket
    according to the provided settings, and otherwise fails as it
    should.

