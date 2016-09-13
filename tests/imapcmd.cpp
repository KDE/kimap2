/**
  * This file is part of the KDE project
  * Copyright (C) 2009 Kevin Ottens <ervin@kde.org>
  * Copyright (C) 2009 Andras Mantia <amantia@kde.org>
  * Copyright (C) 2016 Christian Mollekopf <mollekopf@kolabsys.com>
  *
  * This library is free software; you can redistribute it and/or
  * modify it under the terms of the GNU Library General Public
  * License as published by the Free Software Foundation; either
  * version 2 of the License, or (at your option) any later version.
  *
  * This library is distributed in the hope that it will be useful,
  * but WITHOUT ANY WARRANTY; without even the implied warranty of
  * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  * Library General Public License for more details.
  *
  * You should have received a copy of the GNU Library General Public License
  * along with this library; see the file COPYING.LIB.  If not, write to
  * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
  * Boston, MA 02110-1301, USA.
  */

#include <qdebug.h>
#include <qtcpsocket.h>
#include <QCoreApplication>
#include <qsignalspy.h>

#include "acl.h"
#include "session.h"
#include "appendjob.h"
#include "capabilitiesjob.h"
#include "fetchjob.h"
#include "listjob.h"
#include "loginjob.h"
#include "logoutjob.h"
#include "selectjob.h"
#include "closejob.h"
#include "expungejob.h"
#include "createjob.h"
#include "deletejob.h"
#include "namespacejob.h"
#include "subscribejob.h"
#include "unsubscribejob.h"
#include "renamejob.h"
#include "storejob.h"
#include "setacljob.h"
#include "getacljob.h"
#include "deleteacljob.h"
#include "myrightsjob.h"
#include "listrightsjob.h"
#include "setmetadatajob.h"
#include "getmetadatajob.h"
#include "searchjob.h"

using namespace KIMAP2;
int main(int argc, char **argv)
{
    QCoreApplication::setApplicationName(QStringLiteral("ImapCmd"));

    if (argc < 4) {
        qCritical() << "Not enough parameters, expecting: <server> <user> <password>";
        return -1;
    }

    QString server = QString::fromLocal8Bit(argv[1]);
    int port = 993;
    if (server.count(QLatin1Char(':')) == 1) {
        port = server.split(QLatin1Char(':')).last().toInt();
        server = server.split(QLatin1Char(':')).first();
    }
    QString user = QString::fromLocal8Bit(argv[2]);
    QString password = QString::fromLocal8Bit(argv[3]);

    qInfo() << "Querying:" << server << port << user << password;
    qInfo();

    QCoreApplication app(argc, argv);
    Session session(server, port);

    QObject::connect(&session, &KIMAP2::Session::sslErrors, [&session](const QList<QSslError> &errors) {
        qWarning() << "Got ssl error: " << errors;
        session.ignoreErrors(errors);
    });

    qInfo() << "Logging in...";
    LoginJob *login = new LoginJob(&session);
    login->setEncryptionMode( QSsl::AnyProtocol, false );
    login->setAuthenticationMode( LoginJob::Plain );
    login->setUserName(user);
    login->setPassword(password);
    login->exec();
    qInfo();

    /*if (login->encryptionMode() == LoginJob::Unencrypted)
    {
      qInfo() << "Encrypted login not possible, try to log in without encryption";
      login = new LoginJob( &session );
      login->setUserName( user );
      login->setPassword( password );
      login->exec();
      Q_ASSERT_X( login->error() == 0, "LoginJob", login->errorString().toLocal8Bit().constData() );
      Q_ASSERT( session.state() == Session::Authenticated );
      qInfo();

    }*/

    qInfo() << "Server greeting:" << session.serverGreeting();

    qInfo() << "Asking for capabilities:";
    CapabilitiesJob *capabilities = new CapabilitiesJob(&session);
    capabilities->exec();
    Q_ASSERT_X(capabilities->error() == 0, "CapabilitiesJob", capabilities->errorString().toLocal8Bit().constData());
    Q_ASSERT(session.state() == Session::Authenticated);
    qInfo() << capabilities->capabilities();
    qInfo();

    qInfo() << "Asking for namespaces:";
    NamespaceJob *namespaces = new NamespaceJob(&session);
    namespaces->exec();
    Q_ASSERT_X(namespaces->error() == 0, "CapabilitiesJob", namespaces->errorString().toLocal8Bit().constData());
    Q_ASSERT(session.state() == Session::Authenticated);

    qInfo() << "Contains empty namespace:" << namespaces->containsEmptyNamespace();

    qInfo() << "Personal:";
    foreach (MailBoxDescriptor ns, namespaces->personalNamespaces()) {
        qInfo() << ns.separator << ns.name;
    }

    qInfo() << "User:    ";
    foreach (MailBoxDescriptor ns, namespaces->userNamespaces()) {
        qInfo() << ns.separator << ns.name;
    }

    qInfo() << "Shared:  ";
    foreach (MailBoxDescriptor ns, namespaces->sharedNamespaces()) {
        qInfo() << ns.separator << ns.name;
    }
    qInfo();

    // qInfo() << "Listing mailboxes:";
    // listFolders(&session);
    // Q_ASSERT(session.state() == Session::Authenticated);

    qInfo() << "Selecting INBOX:";
    SelectJob *select = new SelectJob(&session);
    select->setMailBox(QLatin1String("INBOX"));
    select->exec();
    Q_ASSERT_X(select->error() == 0, "SelectJob", select->errorString().toLocal8Bit().constData());
    Q_ASSERT(session.state() == Session::Selected);
    qInfo() << "Flags:" << select->flags();
    qInfo() << "Permanent flags:" << select->permanentFlags();
    qInfo() << "Total Number of Messages:" << select->messageCount();
    qInfo() << "Number of recent Messages:" << select->recentCount();
    qInfo() << "First Unseen Message Index:" << select->firstUnseenIndex();
    qInfo() << "UID validity:" << select->uidValidity();
    qInfo() << "Next UID:" << select->nextUid();
    qInfo();

    QString command = QString::fromLocal8Bit(argv[4]);
    if (command == "search") {
        auto searchJob = new SearchJob(&session);
        searchJob->setTerm(Term(Term::Uid, ImapSet::fromImapSequenceSet("1:*")));
        searchJob->setUidBased(true);
        searchJob->exec();
        qInfo() << "Search result: " << searchJob->results();
    }
    if (command == "flags") {
        qInfo() << "Fetching flags";
        auto job = new FetchJob(&session);
        FetchJob::FetchScope scope;
        scope.mode = FetchJob::FetchScope::Flags;
        job->setScope(scope);
        job->setUidBased(true);
        job->setSequenceSet(ImapSet::fromImapSequenceSet("1:*"));
        job->exec();
    }

    if (command == "fetch") {
        qInfo() << "Fetching content";
        auto job = new FetchJob(&session);
        FetchJob::FetchScope scope;
        // scope.mode = FetchJob::FetchScope::FullHeaders;
        scope.mode = FetchJob::FetchScope::Headers;
        job->setScope(scope);
        job->setUidBased(true);
        job->setSequenceSet(ImapSet::fromImapSequenceSet("1:*"));
        QObject::connect(job, &KIMAP2::FetchJob::resultReceived, [](const KIMAP2::FetchJob::Result &result) {
            qInfo() << "* " << result.sequenceNumber
                    << "uid " << result.uid
                    <<  "size " << result.size
                    <<  "message size " << result.message->encodedContent().size();
        });
        job->exec();
    }


    qInfo() << "Logging out...";
    LogoutJob *logout = new LogoutJob(&session);
    logout->exec();
    Q_ASSERT_X(logout->error() == 0, "LogoutJob", logout->errorString().toLocal8Bit().constData());
    // Q_ASSERT(session.state() == Session::Disconnected);

    return 0;
}
