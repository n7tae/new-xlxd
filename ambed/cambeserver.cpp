//
//  cambeserver.cpp
//  ambed
//
//  Created by Jean-Luc Deltombe (LX3JL) on 15/04/2017.
//  Copyright © 2015 Jean-Luc Deltombe (LX3JL). All rights reserved.
//  Copyright © 2020 Thomas A. Early, N7TAE
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
#include "ccontroller.h"
#include "cvocodecs.h"
#include "cambeserver.h"

////////////////////////////////////////////////////////////////////////////////////////
// globals

CAmbeServer g_AmbeServer;


////////////////////////////////////////////////////////////////////////////////////////
// operation

bool CAmbeServer::Start(void)
{
	// init interfaces & controller
	std::cout << "Initializing vocodecs:" << std::endl;
	if (! g_Vocodecs.Init())
		return false;

	std::cout << "Initializing controller" << std::endl;
	if (! m_Controller.Init())
		return false;

	return true;
}

void CAmbeServer::Stop(void)
{
	// stop controller
	m_Controller.Close();
}
