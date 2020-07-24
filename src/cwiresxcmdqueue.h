//
//  cwiresxcmdqueue.h
//  xlxd
//
//  Created by Jean-Luc Deltombe (LX3JL) on 09/10/2019.
//  Copyright © 2019 Jean-Luc Deltombe (LX3JL). All rights reserved.
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

#ifndef cwiresxcmdqueue_h
#define cwiresxcmdqueue_h


#include "cwiresxcmd.h"

////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////
// class

class CWiresxCmdQueue
{
public:
	// constructor
	CWiresxCmdQueue() {}

	// destructor
	~CWiresxCmdQueue() {}

	// lock
	void Lock()                 { m_Mutex.lock(); }
	void Unlock()               { m_Mutex.unlock(); }

	// pass thru
	CWiresxCmd front()        { return queue.front(); }
	void pop()                { queue.pop(); }
	void push(CWiresxCmd cmd) { queue.push(cmd); }
	bool empty() const        { return queue.empty(); }

protected:
	// status
	std::mutex  m_Mutex;
	std::queue<CWiresxCmd> queue;
};

////////////////////////////////////////////////////////////////////////////////////////
#endif /* cwiresxcmdqueue_h */
