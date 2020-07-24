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
	Close();
}

////////////////////////////////////////////////////////////////////////////////////////
// initialization

bool CProtocols::Init(void)
{
	m_Mutex.lock();
	{
		m_Protocols.emplace_back(std::unique_ptr<CDextraProtocol>(new CDextraProtocol));
		if (! m_Protocols.back()->Initialize("XRF", DEXTRA_PORT, DSTAR_IPV4, DSTAR_IPV6))
			return false;

		m_Protocols.emplace_back(std::unique_ptr<CDplusProtocol>(new CDplusProtocol));
		if (! m_Protocols.back()->Initialize("REF", DPLUS_PORT, DSTAR_IPV4, DSTAR_IPV6))
			return false;

		m_Protocols.emplace_back(std::unique_ptr<CDcsProtocol>(new CDcsProtocol));
		if (! m_Protocols.back()->Initialize("DCS", DCS_PORT, DSTAR_IPV4, DSTAR_IPV6))
			return false;

#ifndef NO_XLX
		m_Protocols.emplace_back(std::unique_ptr<CDmrmmdvmProtocol>(new CDmrmmdvmProtocol));
		if (! m_Protocols.back()->Initialize(nullptr, DMRMMDVM_PORT, DMR_IPV4, DMR_IPV6))
			return false;

		m_Protocols.emplace_back(std::unique_ptr<CDmrplusProtocol>(new CDmrplusProtocol));
		if (! m_Protocols.back()->Initialize(nullptr, DMRPLUS_PORT, DMR_IPV4, DMR_IPV6))
			return false;

		m_Protocols.emplace_back(std::unique_ptr<CYsfProtocol>(new CYsfProtocol));
		if (! m_Protocols.back()->Initialize("YSF", YSF_PORT, DMR_IPV4, DMR_IPV6))
			return false;

		m_Protocols.emplace_back(std::unique_ptr<CXlxProtocol>(new CXlxProtocol));
		if (! m_Protocols.back()->Initialize("XLX", XLX_PORT, DMR_IPV4, DMR_IPV6))
			return false;
#endif

#ifndef NO_G3
		m_Protocols.emplace_back(std::unique_ptr<CG3Protocol>(new CG3Protocol));
		if (! m_Protocols.back()->Initialize("XLX", G3_DV_PORT, DMR_IPV4, DMR_IPV6))
			return false;
#endif

	}
	m_Mutex.unlock();

	// done
	return true;
}

void CProtocols::Close(void)
{
	m_Mutex.lock();
	m_Protocols.clear();
	m_Mutex.unlock();
}
