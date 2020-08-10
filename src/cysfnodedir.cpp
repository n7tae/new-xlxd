//
//  cysfnodedir.cpp
//  xlxd
//
//  Created by Jean-Luc Deltombe (LX3JL) on 08/10/2019.
//  Copyright © 2019 Jean-Luc Deltombe (LX3JL). All rights reserved.
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

#include <string.h>
#include <mysql/mysql.h>
#include "main.h"
#include "creflector.h"
#include "cysfnodedir.h"


////////////////////////////////////////////////////////////////////////////////////////
// constructor & destructor

CYsfNodeDir::CYsfNodeDir()
{
	keep_running = true;
}

CYsfNodeDir::~CYsfNodeDir()
{
	// kill threads
	Close();
}


////////////////////////////////////////////////////////////////////////////////////////
// init & close

bool CYsfNodeDir::Init(void)
{
	// load content
	Reload();

	// reset run flag
	keep_running = true;

	// start  thread;
	m_Future = std::async(std::launch::async, &CYsfNodeDir::Thread, this);

	return true;
}

void CYsfNodeDir::Close(void)
{
	keep_running = false;
	if ( m_Future.valid() )
	{
		m_Future.get();
	}
}

////////////////////////////////////////////////////////////////////////////////////////
// thread

void CYsfNodeDir::Thread()
{
	while (keep_running)
	{
		// Wait YSFNODEDB_REFRESH_RATE minutes
		for (int i=0; i<30*YSFNODEDB_REFRESH_RATE && keep_running; i++)
			CTimePoint::TaskSleepFor(2000);

		// have lists files changed ?
		if ( NeedReload() )
		{
			Reload();
		}
	}
}

////////////////////////////////////////////////////////////////////////////////////////
// Reload

bool CYsfNodeDir::Reload(void)
{
	CBuffer buffer;
	bool ok = false;

	if ( LoadContent(&buffer) )
	{
		Lock();
		ok = RefreshContent(buffer) && ReadDb();
		Unlock();
	}
	return ok;
}

////////////////////////////////////////////////////////////////////////////////////////
// find

bool CYsfNodeDir::FindFrequencies(const CCallsign &callsign, uint32 *txfreq, uint32 *rxfreq)
{
	auto found = find(callsign);
	if ( found != end() )
	{
		*txfreq = found->second.GetTxFrequency();
		*rxfreq = found->second.GetRxFrequency();
		return true;
	}
	else
	{
		*txfreq = YSF_DEFAULT_NODE_TX_FREQ;
		*rxfreq = YSF_DEFAULT_NODE_RX_FREQ;
		return false;
	}
}

bool CYsfNodeDir::ReadDb()
{
#if YSF_DB_SUPPORT==true
	bool rval = false;
	MYSQL *con = mysql_init(NULL);
	if (con)
	{
		if (mysql_real_connect(con, "localhost", YSF_DB_USER, YSF_DB_PASSWORD, YSF_DB_NAME, 0, NULL, 0))
		{
			if (0 == mysql_query(con, "SELECT callsign,txfreq,rxfreq FROM ysfnodes"))
			{
				MYSQL_RES *result = mysql_store_result(con);
  				if (result)
				{
					rval = true;
					std::cout << "Adding " << mysql_num_rows(result) << " registered YSF stations from table " << YSF_DB_NAME << std::endl;
					MYSQL_ROW row;

					while ((row = mysql_fetch_row(result)))
					{
						CCallsign cs(row[0]);
						CYsfNode node(atoi(row[1]), atoi(row[2]));
						std::pair<CCallsign, CYsfNode> pair(cs, node);
						insert(pair);
					}

					mysql_free_result(result);
				}
				else
				{
					std::cerr << "Could not fetch MySQL rows" << std::endl;
				}
			}
			else
			{
				std::cerr << "MySQL query failed: " << mysql_error(con) << std::endl;
			}
		}
		else
		{
			std::cerr << "Could not connect to database " << YSF_DB_NAME << ": " << mysql_error(con) << std::endl;
		}
		mysql_close(con);
	}
	else
	{
		std::cerr << "Could not init mysql." << std::endl;
		return false;
	}
	return rval;
#else
	return true;
#endif
}
