//
//  cvocodecinterface.cpp
//  ambed
//
//  Created by Jean-Luc Deltombe (LX3JL) on 23/04/2017.
//  Copyright © 2015 Jean-Luc Deltombe (LX3JL). All rights reserved.
//
// ----------------------------------------------------------------------------
//    This file is part of ambed.
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
#include "ctimepoint.h"
#include "cpacketqueue.h"
#include "cvocodecinterface.h"


////////////////////////////////////////////////////////////////////////////////////////
// constructor

CVocodecInterface::CVocodecInterface()
{
	m_Channels.reserve(5);
	keep_running = true;
}

////////////////////////////////////////////////////////////////////////////////////////
// destructor

CVocodecInterface::~CVocodecInterface()
{
	// empty channel array
	// chennels are deleted by their owner (CVocodecs)
	m_Channels.clear();

	// stop thread
	keep_running = false;
	if ( m_Future.valid() )
	{
		m_Future.get();
	}
}

////////////////////////////////////////////////////////////////////////////////////////
// initialization

bool CVocodecInterface::Init(void)
{
	// reset stop flag
	keep_running = true;

	// start  thread;
	m_Future = std::async(std::launch::async, &CVocodecInterface::Thread, this);

	// done
	return true;
}


////////////////////////////////////////////////////////////////////////////////////////
// thread

void CVocodecInterface::Thread()
{
	while ( keep_running )
	{
		Task();
	}
}


////////////////////////////////////////////////////////////////////////////////////////
// manage Channels

void CVocodecInterface::AddChannel(CVocodecChannel *Channel)
{
	m_Channels.push_back(Channel);
}
