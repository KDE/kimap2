/*
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

#ifndef KIMAP_NOOPJOB_H
#define KIMAP_NOOPJOB_H

#include "kimap_export.h"

#include "job.h"

namespace KIMAP {

class Session;
class Message;
class NoopJobPrivate;

//TODO: do we want to record the (possible) untagged RECENT, EXISTS, FETCH response?
class KIMAP_EXPORT NoopJob : public Job
{
  Q_OBJECT
  Q_DECLARE_PRIVATE(NoopJob)

  friend class SessionPrivate;

  public:
    NoopJob( Session *session );
    virtual ~NoopJob();

  protected:
    virtual void doStart();
};

}

#endif