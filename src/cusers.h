//
//  cusers.h
//  xlxd
//
//  Created by Jean-Luc Deltombe (LX3JL) on 13/11/2015.
//  Copyright © 2015 Jean-Luc Deltombe (LX3JL). All rights reserved.
//  Copyright © 2020 Thomas A. Early, N7TAE
//
// ----------------------------------------------------------------------------
//    This file is part of xlxd.
//
//    xlxd is free software: you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation, either version 3 of the License, or
//    (at your option) any later version.
//
//    xlxd is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    You should have received a copy of the GNU General Public License
//    along with Foobar.  If not, see <http://www.gnu.org/licenses/>.
// ----------------------------------------------------------------------------

#ifndef cusers_h
#define cusers_h

#include "cuser.h"

////////////////////////////////////////////////////////////////////////////////////////

class CUsers
{
public:
    // constructor
    CUsers();

    // destructor
    virtual ~CUsers() {}

    // locks
    void Lock(void)                     { m_Mutex.lock(); }
    void Unlock(void)                   { m_Mutex.unlock(); }

    // management
    int    GetSize(void) const          { return (int)m_Users.size(); }
    void   AddUser(const CUser &);

	// pass-thru
	std::list<CUser>::iterator begin()              { return m_Users.begin(); }
	std::list<CUser>::iterator end()                { return m_Users.end(); }

    // operation
    void   Hearing(const CCallsign &, const CCallsign &, const CCallsign &);
    void   Hearing(const CCallsign &, const CCallsign &, const CCallsign &, const CCallsign &);

protected:
    // data
    std::mutex        m_Mutex;
    std::list<CUser>  m_Users;
};

////////////////////////////////////////////////////////////////////////////////////////
#endif /* cusers_h */
