//
//  ctranscoder.h
//  xlxd
//
//  Created by Jean-Luc Deltombe (LX3JL) on 13/04/2017.
//  Copyright © 2015 Jean-Luc Deltombe (LX3JL). All rights reserved.
//  Copyright © 2020 Thomas A. Early N7TAE
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

#ifndef ctranscoder_h
#define ctranscoder_h

#include "csemaphore.h"
#include "ccodecstream.h"
#include "cudpsocket.h"

////////////////////////////////////////////////////////////////////////////////////////
// define


////////////////////////////////////////////////////////////////////////////////////////
// class

class CPacketStream;

class CTranscoder
{
public:
	// constructor
	CTranscoder();

	// destructor
	~CTranscoder();

	// initialization
	bool Init(void);
	void Close(void);

	// locks
	void Lock(void)                     { m_Mutex.lock(); }
	void Unlock(void)                   { m_Mutex.unlock(); }

	// status
	bool IsConnected(void) const        { return m_bConnected; }

	// manage streams
	std::shared_ptr<CCodecStream> GetStream(CPacketStream *, uint8);
	void ReleaseStream(std::shared_ptr<CCodecStream>);

	// task
	void Thread(void);
	void Task(void);

protected:
	// keepalive helpers
	void HandleKeepalives(void);

	// packet decoding helpers
	bool IsValidKeepAlivePacket(const CBuffer &);
	bool IsValidStreamDescrPacket(const CBuffer &, uint16 *, uint16 *);
	bool IsValidNoStreamAvailablePacket(const CBuffer&);

	// packet encoding helpers
	void EncodeKeepAlivePacket(CBuffer *);
	void EncodeOpenstreamPacket(CBuffer *, uint8, uint8);
	void EncodeClosestreamPacket(CBuffer *, uint16);

protected:
	// streams
	std::mutex m_Mutex;
	std::list<std::shared_ptr<CCodecStream>> m_Streams;

	// sync objects for Openstream
	CSemaphore      m_SemaphoreOpenStream;
	bool            m_bStreamOpened;
	uint16          m_StreamidOpenStream;
	uint16          m_PortOpenStream;

	// thread
	std::atomic<bool> keep_running;
	std::future<void> m_Future;

	// socket
	CIp             m_Ip;
	CUdpSocket      m_Socket;
	bool            m_bConnected;

	// time
	CTimePoint      m_LastKeepaliveTime;
	CTimePoint      m_LastActivityTime;
};


////////////////////////////////////////////////////////////////////////////////////////
#endif /* ctranscoder_h */
