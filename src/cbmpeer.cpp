//
//  cbmpeer.cpp
//  xlxd
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

#include "main.h"
#include <string.h>
#include "creflector.h"
#include "cbmpeer.h"
#include "cbmclient.h"


////////////////////////////////////////////////////////////////////////////////////////
// constructor


CBmPeer::CBmPeer()
{
}

CBmPeer::CBmPeer(const CCallsign &callsign, const CIp &ip, const char *modules, const CVersion &version)
: CPeer(callsign, ip, modules, version)
{
    std::cout << "Adding BM peer" << std::endl;

    // and construct all xlx clients
    for ( int i = 0; i < ::strlen(modules); i++ )
    {
        // create
        CBmClient *client = new CBmClient(callsign, ip, modules[i]);
        // and append to vector
        m_Clients.push_back(client);
    }
}

CBmPeer::CBmPeer(const CBmPeer &peer)
: CPeer(peer)
{
    for ( auto it=peer.cbegin(); it!=peer.cend(); it++ )
    {
        CBmClient *client = new CBmClient((const CBmClient &)*(*it));
        m_Clients.push_back(client);

    }
}

////////////////////////////////////////////////////////////////////////////////////////
// destructors

CBmPeer::~CBmPeer()
{
}

////////////////////////////////////////////////////////////////////////////////////////
// status

bool CBmPeer::IsAlive(void) const
{
    return (m_LastKeepaliveTime.DurationSinceNow() < XLX_KEEPALIVE_TIMEOUT);
}

////////////////////////////////////////////////////////////////////////////////////////
// revision helper

int CBmPeer::GetProtocolRevision(const CVersion &version)
{
    return XLX_PROTOCOL_REVISION_2;
}
