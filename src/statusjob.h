/*
    Copyright (c) 2016 Daniel Vrátil <dvratil@kde.org>

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

#ifndef KIMAP2_STATUSJOB_H
#define KIMAP2_STATUSJOB_H

#include "kimap2_export.h"

#include "job.h"
#include <QList>

namespace KIMAP2
{
class Session;
class StatusJobPrivate;

class KIMAP2_EXPORT StatusJob : public Job
{
    Q_OBJECT
    Q_DECLARE_PRIVATE(StatusJob)

    friend class StatusJobPrivate;

public:
    explicit StatusJob(Session *session);
    ~StatusJob();

    void setMailBox(const QString &mailBox);
    QString mailBox() const;

    void setDataItems(const QList<QByteArray> &dataItems);
    QList<QByteArray> dataItems() const;

    QList<QPair<QByteArray, qint64>> status() const;

protected:
    void doStart() Q_DECL_OVERRIDE;
    void handleResponse(const Message &response) Q_DECL_OVERRIDE;
};

}

#endif // KIMAP2_STATUSJOB_H
