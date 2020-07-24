//
//  cstream.cpp
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
#include <string.h>
#include "ctimepoint.h"
#include "cambeserver.h"
#include "cvocodecs.h"
#include "cambepacket.h"
#include "cstream.h"

////////////////////////////////////////////////////////////////////////////////////////
// define

#define AMBE_FRAME_SIZE         9

////////////////////////////////////////////////////////////////////////////////////////
// constructor

CStream::CStream()
{
	m_uiId = 0;
	m_uiPort = 0;
	keep_running = true;
	m_VocodecChannel = NULL;
	m_LastActivity.Now();
	m_iTotalPackets = 0;
	m_iLostPackets = 0;
}

CStream::CStream(uint16 uiId, const CCallsign &Callsign, const CIp &Ip, uint8 uiCodecIn, uint8 uiCodecOut)
{
	m_uiId = uiId;
	m_Callsign = Callsign;
	m_Ip = Ip;
	m_uiPort = 0;
	m_uiCodecIn = uiCodecIn;
	m_uiCodecOut = uiCodecOut;
	keep_running = true;
	m_VocodecChannel = NULL;
	m_LastActivity.Now();
	m_iTotalPackets = 0;
	m_iLostPackets = 0;
}

////////////////////////////////////////////////////////////////////////////////////////
// destructor

CStream::~CStream()
{
	// stop thread first
	keep_running = false;
	if ( m_Future.valid() )
	{
		m_Future.get();
	}

	// then close everything
	m_Socket.Close();
	if ( m_VocodecChannel != NULL )
	{
		m_VocodecChannel->Close();
	}
}

////////////////////////////////////////////////////////////////////////////////////////
// initialization

bool CStream::Init(uint16 uiPort)
{
	// reset stop flag
	keep_running = true;

	// create our socket
	auto s = g_AmbeServer.GetListenIp();
	CIp ip(strchr(s, ':') ? AF_INET6 : AF_INET, uiPort, s);
	if (! ip.IsSet())
	{
		std::cerr << "Could not initialize ip address " << s << std::endl;
		return false;
	}

	if (! m_Socket.Open(ip))
	{
		std::cout << "Error opening stream stream socket on " << ip << std::endl;
		return false;
	}

	// open the vocodecchannel
	m_VocodecChannel = g_Vocodecs.OpenChannel(m_uiCodecIn, m_uiCodecOut);
	if (NULL == m_VocodecChannel)
	{
		std::cerr << "Could not open Vocodec Channel" << std::endl;
		m_Socket.Close();
		return false;
	}

	// store port
	m_uiPort = uiPort;

	// start  thread;
	m_Future = std::async(std::launch::async, &CStream::Task, this);

	// init timeout system
	m_LastActivity.Now();
	m_iTotalPackets = 0;
	m_iLostPackets = 0;


	// done
	return true;

}

void CStream::Close(void)
{
	// stop thread first
	keep_running = false;
	if ( m_Future.valid() )
	{
		m_Future.get();
	}

	// then close everything
	m_Socket.Close();
	if ( m_VocodecChannel != NULL )
	{
		m_VocodecChannel->Close();
	}


	// report
	std::cout << m_iLostPackets << " of " << m_iTotalPackets << " packets lost" << std::endl;
}

////////////////////////////////////////////////////////////////////////////////////////
// task

void CStream::Task(void)
{
	while (keep_running) {
		CBuffer     Buffer;
		static CIp  Ip;
		uint8       uiPid;
		uint8       Ambe[AMBE_FRAME_SIZE];
		CAmbePacket *packet;
		CPacketQueue *queue;

		// anything coming in from codec client ?
		if ( m_Socket.Receive(Buffer, Ip, 1) )
		{
			// crack packet
			if ( IsValidDvFramePacket(Buffer, &uiPid, Ambe) )
			{
				// transcode AMBE here
				m_LastActivity.Now();
				m_iTotalPackets++;

				// post packet to VocoderChannel
				packet = new CAmbePacket(uiPid, m_uiCodecIn, Ambe);
				queue = m_VocodecChannel->GetPacketQueueIn();
				queue->push(packet);
				m_VocodecChannel->ReleasePacketQueueIn();
			}
		}

		// anything in our queue ?
		queue = m_VocodecChannel->GetPacketQueueOut();
		while ( !queue->empty() )
		{
			// get the packet
			packet = (CAmbePacket *)queue->front();
			queue->pop();
			// send it to client
			EncodeDvFramePacket(&Buffer, packet->GetPid(), packet->GetAmbe());
			m_Socket.Send(Buffer, Ip, m_uiPort);
			// and done
			delete packet;
		}
		m_VocodecChannel->ReleasePacketQueueOut();
	}
}

////////////////////////////////////////////////////////////////////////////////////////
// packet decoding helpers

bool CStream::IsValidDvFramePacket(const CBuffer &Buffer, uint8 *pid, uint8 *ambe)
{
	bool valid = false;

	if ( Buffer.size() == 11 )
	{
		uint8 codec = Buffer.data()[0];
		*pid = Buffer.data()[1];
		::memcpy(ambe, &(Buffer.data()[2]), 9);
		valid = (codec == GetCodecIn());
	}

	return valid;
}

////////////////////////////////////////////////////////////////////////////////////////
// packet encodeing helpers

void CStream::EncodeDvFramePacket(CBuffer *Buffer, uint8 Pid, uint8 *Ambe)
{
	Buffer->clear();
	Buffer->Append((uint8)GetCodecOut());
	Buffer->Append((uint8)Pid);
	Buffer->Append(Ambe, 9);
}
