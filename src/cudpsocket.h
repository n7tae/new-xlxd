//
//  cudpsocket.h
//  xlxd
//
//  Created by Jean-Luc Deltombe (LX3JL) on 31/10/2015.
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

#ifndef cudpsocket_h
#define cudpsocket_h

#include <cstdint>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>

#include "cip.h"
#include "cbuffer.h"

////////////////////////////////////////////////////////////////////////////////////////
// define

#define UDP_BUFFER_LENMAX       1024


////////////////////////////////////////////////////////////////////////////////////////
// class

class CUdpSocket
{
public:
	// constructor
	CUdpSocket();

	// destructor
	~CUdpSocket();

	// open & close
	bool Open(const CIp &Ip);
	void Close(void);
	int  GetSocket(void)
	{
		return m_fd;
	}

	// read
	bool Receive(CBuffer &, CIp &, int);

	// write
	void Send(const CBuffer &, const CIp &) const;
	void Send(const char    *, const CIp &) const;
	void Send(const CBuffer &, const CIp &, uint16_t) const;
	void Send(const char    *, const CIp &, uint16_t) const;

protected:
	// data
	int m_fd;
	CIp m_addr;
};

////////////////////////////////////////////////////////////////////////////////////////
#endif /* cudpsocket_h */
