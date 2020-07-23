//
//  cpeers.h
//  xlxd
//
//  Created by Jean-Luc Deltombe (LX3JL) on 10/12/2016.
//  Copyright © 2016 Jean-Luc Deltombe (LX3JL). All rights reserved.
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

#ifndef cpeers_h
#define cpeers_h

#include "cpeer.h"


////////////////////////////////////////////////////////////////////////////////////////
// define


////////////////////////////////////////////////////////////////////////////////////////
// class

class CPeers
{
public:
    // constructors
    CPeers();

    // destructors
    virtual ~CPeers();

    // locks
    void Lock(void)                     { m_Mutex.lock(); }
    void Unlock(void)                   { m_Mutex.unlock(); }

    // manage peers
    int     GetSize(void) const         { return (int)m_Peers.size(); }
    void    AddPeer(std::shared_ptr<CPeer>);
    void    RemovePeer(std::shared_ptr<CPeer>);

	// pass-thru
	std::list<std::shared_ptr<CPeer>>::iterator begin()              { return m_Peers.begin(); }
	std::list<std::shared_ptr<CPeer>>::iterator end()                { return m_Peers.end(); }
	std::list<std::shared_ptr<CPeer>>::const_iterator cbegin() const { return m_Peers.cbegin(); }
	std::list<std::shared_ptr<CPeer>>::const_iterator cend() const   { return m_Peers.cend(); }

    // find peers
    std::shared_ptr<CPeer> FindPeer(const CIp &, int);
    std::shared_ptr<CPeer> FindPeer(const CCallsign &, const CIp &, int);
    std::shared_ptr<CPeer> FindPeer(const CCallsign &, int);

    // iterate on peers
    std::shared_ptr<CPeer> FindNextPeer(int, std::list<std::shared_ptr<CPeer>>::iterator &);

protected:
    // data
    std::mutex         m_Mutex;
    std::list<std::shared_ptr<CPeer>> m_Peers;
};


////////////////////////////////////////////////////////////////////////////////////////
#endif /* cpeers_h */
