//
//  main.cpp
//  xlxd
//
//  Created by Jean-Luc Deltombe (LX3JL) on 31/10/2015.
//  Copyright © 2015 Jean-Luc Deltombe (LX3JL). All rights reserved.
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
#include "creflector.h"

#include <sys/stat.h>


////////////////////////////////////////////////////////////////////////////////////////
// global objects

CReflector  g_Reflector;

////////////////////////////////////////////////////////////////////////////////////////
// function declaration

#include "cusers.h"

int main()
{
	const std::string cs(CALLSIGN);
	if ((cs.size() != 6) || (cs.compare(0, 3, "XLX") && cs.compare(0, 3, "XRF")))
	{
		std::cerr << "Malformed reflector callsign: '" << cs << "', aborting!" << std::endl;
		return EXIT_FAILURE;
	}

	// remove pidfile
	remove(PIDFILE_PATH);

	// splash
	std::cout << "Starting " << cs << " " << VERSION_MAJOR << "." << VERSION_MINOR << "." << VERSION_REVISION << std::endl << std::endl;

	// initialize reflector
	g_Reflector.SetCallsign(cs.c_str());

#ifdef LISTEN_IPV4
	g_Reflector.SetListenIPv4(LISTEN_IPV4, INET_ADDRSTRLEN);
#endif

#ifdef LISTEN_IPV6
	g_Reflector.SetListenIPv6(LISTEN_IPV6, INET6_ADDRSTRLEN);
#endif

#ifdef TRANSCODER_IP
	g_Reflector.SetTranscoderIp(TRANSCODER_IP, INET6_ADDRSTRLEN);
#endif


	// and let it run
	if ( !g_Reflector.Start() )
	{
		std::cout << "Error starting reflector" << std::endl;
		return EXIT_FAILURE;
	}

	std::cout << "Reflector " << g_Reflector.GetCallsign()  << "started and listening on ";
#if defined LISTEN_IPV4
	std::cout << g_Reflector.GetListenIPv4() << " for IPv4";
#if defined LISTEN_IPV6
	std::cout << " and " << g_Reflector.GetListenIPv6() << " for IPv6" << std::endl;
#else
	std::cout << std::endl;
#endif
#elif defined LISTEN_IPV6
	std::cout << g_Reflector.GetListenIPv6() << " for IPv6" << std::endl;
#else
	std::cout << "...ABORTING! No IP addresses defined!" << std::endl;
	return EXIT_FAILURE;
#endif

	// write new pid file
	std::ofstream ofs(PIDFILE_PATH, std::ofstream::out);
	ofs << getpid() << std::endl;
	ofs.close();

	pause(); // wait for any signal

	g_Reflector.Stop();
	std::cout << "Reflector stopped" << std::endl;

	// done
	return EXIT_SUCCESS;
}
