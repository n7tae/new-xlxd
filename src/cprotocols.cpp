//
//  cprotocols.cpp
//  xlxd
//
//  Created by Jean-Luc Deltombe (LX3JL) on 01/11/2015.
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

#include "main.h"
#include "cdextraprotocol.h"
#include "cdplusprotocol.h"
#include "cdcsprotocol.h"
#ifndef NO_XLX
#include "cxlxprotocol.h"
#include "cdmrplusprotocol.h"
#include "cdmrmmdvmprotocol.h"
#include "cysfprotocol.h"
#endif
#ifndef NO_G3
#include "cg3protocol.h"
#endif
#include "cprotocols.h"

////////////////////////////////////////////////////////////////////////////////////////
// destructor

CProtocols::~CProtocols()
{
    m_Mutex.lock();
    {
        for ( auto it=m_Protocols.begin(); it!=m_Protocols.end(); it++)
		{
			(*it)->Close();
			delete *it;
		}
		m_Protocols.clear();
    }
    m_Mutex.unlock();
}

////////////////////////////////////////////////////////////////////////////////////////
// initialization

bool CProtocols::Init(void)
{
    m_Mutex.lock();
    {
        auto dextra = new CDextraProtocol;
        if (dextra->Init())
			m_Protocols.push_back(dextra);
		else
		{
			delete dextra;
			return false;
		}


        // create and initialize DPLUS
        auto dplus = new CDplusProtocol;
        if (dplus->Init())
			m_Protocols.push_back(dplus);
		else
		{
			delete dplus;
			return false;
		}

        // create and initialize DCS
        auto dcs = new CDcsProtocol;
        if (dcs->Init())
			m_Protocols.push_back(dcs);
		else
		{
			delete dcs;
			return false;
		}

#ifndef NO_XLX
        // create and initialize XLX - interlink
        auto xlx = new CXlxProtocol;
        if (xlx->Init())
			m_Protocols.push_back(xlx);
		else
		{
			delete xlx;
			return false;
		}

        // create and initialize DMRPLUS
        auto dmrplus = new CDmrplusProtocol;
        if (dmrplus->Init())
			m_Protocols.push_back(dmrplus);
		else
		{
			delete dmrplus;
			return false;
		}

        // create and initialize DMRMMDVM
        auto dmrmmdvm = new CDmrmmdvmProtocol;
        if (dmrmmdvm->Init())
			m_Protocols.push_back(dmrmmdvm);
		else
		{
			delete dmrmmdvm;
			return false;
		}

        // create and initialize YSF
        auto ysf = new CYsfProtocol;
        if (ysf->Init())
			m_Protocols.push_back(ysf);
		else
		{
			delete ysf;
			return false;
		}
#endif

#ifndef NO_G3
        // create and initialize G3
        auto g3 = new CG3Protocol;
        if (g3->Init())
			m_Protocols.push_back(g3);
		else
		{
			delete g3;
			return true;
		}
#endif

    }
    m_Mutex.unlock();

    // done
    return true;
}

void CProtocols::Close(void)
{
    m_Mutex.lock();
    {
        for ( auto it=m_Protocols.begin(); it!=m_Protocols.end(); it++)
		{
			(*it)->Close();
			delete *it;
		}
		m_Protocols.clear();
    }
    m_Mutex.unlock();
}
