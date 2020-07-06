//
//  main.cpp
//  ambed
//
//  Created by Jean-Luc Deltombe (LX3JL) on 13/04/2017.
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
#include "cambeserver.h"

#include <sys/stat.h>

////////////////////////////////////////////////////////////////////////////////////////
// function declaration


int main(int argc, const char * argv[])
{

    // check arguments
    if ( argc != 2 )
    {
        std::cout << "Usage: ambed ip" << std::endl;
        std::cout << "example: ambed 192.168.178.212" << std::endl;
        return 1;
    }

    // initialize ambeserver
    g_AmbeServer.SetListenIp(argv[1]);

    // and let it run
    std::cout << "Starting AMBEd " << VERSION_MAJOR << "." << VERSION_MINOR << "." << VERSION_REVISION << std::endl << std::endl;
    if ( ! g_AmbeServer.Start() )
    {
        std::cout << "Error starting AMBEd" << std::endl;
        return EXIT_FAILURE;
    }
    std::cout << "AMBEd started and listening on " << g_AmbeServer.GetListenIp() << std::endl;

	pause(); // wait for any signal

    // and wait for end
    g_AmbeServer.Stop();
    std::cout << "AMBEd stopped" << std::endl;

    // done
    return EXIT_SUCCESS;
}
