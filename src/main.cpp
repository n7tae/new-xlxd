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

int main(int argc, const char * argv[])
{

    // check arguments
    if ( argc != 5 )
    {
        std::cout << "Usage: " << argv[0] << " callsign ipv4 ipv6 ambedip" << std::endl;
        std::cout << "example: " << argv[0] << " XLX999 192.168.178.212 2001:400:534::675b 127.0.0.1" << std::endl;
        return EXIT_FAILURE;
    }

	bool is4none = 0 == strncasecmp(argv[2], "none", 4);
	bool is6none = 0 == strncasecmp(argv[3], "none", 4);
	if (is4none && is6none) {
		std::cerr << "Both IPv4 and 6 address can't both be 'none'" << std::endl;
		return EXIT_FAILURE;
	}

    // splash
    std::cout << "Starting " << argv[0] << " " << VERSION_MAJOR << "." << VERSION_MINOR << "." << VERSION_REVISION << std::endl << std::endl;

    // initialize reflector
    g_Reflector.SetCallsign(argv[1]);
    g_Reflector.SetListenIPv4(argv[2], INET_ADDRSTRLEN);
    g_Reflector.SetListenIPv6(argv[3], INET6_ADDRSTRLEN);
    g_Reflector.SetTranscoderIp(argv[4], INET6_ADDRSTRLEN);


    // and let it run
    if ( !g_Reflector.Start() )
    {
        std::cout << "Error starting reflector" << std::endl;
        return EXIT_FAILURE;
    }

    std::cout << "Reflector " << g_Reflector.GetCallsign()  << "started and listening on ";
	if (! is4none)
	{
		std::cout << g_Reflector.GetListenIPv4();
		if (! is6none)
		{
			std::cout << " and " << g_Reflector.GetListenIPv4() << std::endl;
		}
	}
	else
	{
		std::cout << g_Reflector.GetListenIPv6() << std::endl;
	}

	pause(); // wait for any signal

    g_Reflector.Stop();
    std::cout << "Reflector stopped" << std::endl;

    // done
    return EXIT_SUCCESS;
}
