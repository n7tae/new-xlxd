//
//  cprotocol.cpp
//  xlxd
//
//  Created by Jean-Luc Deltombe (LX3JL) on 01/11/2015.
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

#include "main.h"
#include "cprotocol.h"
#include "cclients.h"
#include "creflector.h"


////////////////////////////////////////////////////////////////////////////////////////
// constructor


CProtocol::CProtocol() : keep_running(true), m_pThread(NULL) {}


////////////////////////////////////////////////////////////////////////////////////////
// destructor

CProtocol::~CProtocol()
{
    // kill threads
    keep_running = false;
    if ( m_pThread != NULL )
    {
        m_pThread->join();
        delete m_pThread;
		m_pThread = NULL;
    }

	// Close sockets
	m_Socket6.Close();
	m_Socket4.Close();

    // empty queue
    m_Queue.Lock();
    while ( !m_Queue.empty() )
    {
        m_Queue.pop();
    }
    m_Queue.Unlock();
}

////////////////////////////////////////////////////////////////////////////////////////
// initialization

bool CProtocol::Initialize(const char *type, const uint16 port, const bool has_ipv4, const bool has_ipv6)
{
    // init reflector apparent callsign
    m_ReflectorCallsign = g_Reflector.GetCallsign();

    // reset stop flag
    keep_running = true;

    // update the reflector callsign
	if (type)
	    m_ReflectorCallsign.PatchCallsign(0, (const uint8 *)type, 3);

    // create our sockets
#ifdef LISTEN_IPV4
	if (has_ipv4)
	{
		CIp ip4(AF_INET, port, g_Reflector.GetListenIPv4());
		if ( ip4.IsSet() )
		{
			if (! m_Socket4.Open(ip4))
				return false;
		}
	}
#endif

#ifdef LISTEN_IPV6
	if (has_ipv6)
	{
		CIp ip6(AF_INET6, port, g_Reflector.GetListenIPv6());
		if ( ip6.IsSet() )
		{
			if (! m_Socket6.Open(ip6))
			{
				m_Socket4.Close();
				return false;
			}
		}
	}
#endif

    // start  thread;
	m_pThread = new std::thread(CProtocol::Thread, this);
	if (m_pThread == NULL)
	{
		std::cerr << "Could not start DCS thread!" << std::endl;
		m_Socket4.Close();
		m_Socket6.Close();
		return false;
	}

    // done
    return true;
}

void CProtocol::Thread(CProtocol *This)
{
	while (This->keep_running)
	{
		This->Task();
	}
}

void CProtocol::Close(void)
{
    keep_running = false;
    if ( m_pThread != NULL )
    {
        m_pThread->join();
        delete m_pThread;
        m_pThread = NULL;
    }
	m_Socket4.Close();
	m_Socket6.Close();
}

////////////////////////////////////////////////////////////////////////////////////////
// packet encoding helpers

bool CProtocol::EncodeDvPacket(const CPacket &packet, CBuffer *buffer) const
{
    bool ok = false;
    if ( packet.IsDvFrame() )
    {
        if ( packet.IsLastPacket() )
        {
            ok = EncodeDvLastFramePacket((const CDvLastFramePacket &)packet, buffer);
        }
        else
        {
            ok = EncodeDvFramePacket((const CDvFramePacket &)packet, buffer);
        }
    }
    else if ( packet.IsDvHeader() )
    {
        ok = EncodeDvHeaderPacket((const CDvHeaderPacket &)packet, buffer);
    }
    else
    {
        buffer->clear();
    }
    return ok;
}

////////////////////////////////////////////////////////////////////////////////////////
// streams helpers

void CProtocol::OnDvFramePacketIn(CDvFramePacket *Frame, const CIp *Ip)
{
    // find the stream
    CPacketStream *stream = GetStream(Frame->GetStreamId(), Ip);
    if ( stream == NULL )
	{
		std::cout << "Deleting orphaned Frame with ID " << Frame->GetStreamId() << " on " << *Ip << std::endl;
		delete Frame;
	}
	else
    {
        //std::cout << "DV frame" << "from "  << *Ip << std::endl;
        // and push
        stream->Lock();
        stream->Push(Frame);
        stream->Unlock();
    }
}

void CProtocol::OnDvLastFramePacketIn(CDvLastFramePacket *Frame, const CIp *Ip)
{
    // find the stream
    CPacketStream *stream = GetStream(Frame->GetStreamId(), Ip);
    if ( stream == NULL )
	{
		std::cout << "Deleting orphaned Last Frame with ID " << Frame->GetStreamId() << " on " << *Ip << std::endl;
		delete Frame;
	}
	else
    {
        // push
        stream->Lock();
        stream->Push(Frame);
        stream->Unlock();

        // and close the stream
        g_Reflector.CloseStream(stream);
    }
}

////////////////////////////////////////////////////////////////////////////////////////
// stream handle helpers

CPacketStream *CProtocol::GetStream(uint16 uiStreamId, const CIp *Ip)
{
    for ( auto it=m_Streams.begin(); it!=m_Streams.end(); it++ )
    {
        if ( (*it)->GetStreamId() == uiStreamId )
        {
            // if Ip not NULL, also check if IP match
            if ( (Ip != NULL) && ((*it)->GetOwnerIp() != NULL) )
            {
                if ( *Ip == *((*it)->GetOwnerIp()) )
                {
                	return *it;
                }
            }
        }
    }
    // done
    return NULL;
}

void CProtocol::CheckStreamsTimeout(void)
{
    for ( auto it=m_Streams.begin(); it!=m_Streams.end(); )
    {
        // time out ?
        (*it)->Lock();
        if ( (*it)->IsExpired() )
        {
            // yes, close it
            (*it)->Unlock();
            g_Reflector.CloseStream(*it);
            // and remove it
            it = m_Streams.erase(it);
        }
        else
        {
            (*it++)->Unlock();
        }
    }
}

////////////////////////////////////////////////////////////////////////////////////////
// queue helper

void CProtocol::HandleQueue(void)
{
    // the default protocol just keep queue empty
    m_Queue.Lock();
    while ( !m_Queue.empty() )
    {
        delete m_Queue.front();
        m_Queue.pop();
    }
    m_Queue.Unlock();
}

////////////////////////////////////////////////////////////////////////////////////////
// syntax helper

bool CProtocol::IsNumber(char c) const
{
    return ((c >= '0') && (c <= '9'));
}

bool CProtocol::IsLetter(char c) const
{
    return ((c >= 'A') && (c <= 'Z'));
}

bool CProtocol::IsSpace(char c) const
{
    return (c == ' ');
}

////////////////////////////////////////////////////////////////////////////////////////
// DestId to Module helper

char CProtocol::DmrDstIdToModule(uint32 tg) const
{
    return ((char)((tg % 26)-1) + 'A');
}

uint32 CProtocol::ModuleToDmrDestId(char m) const
{
    return (uint32)(m - 'A')+1;
}

////////////////////////////////////////////////////////////////////////////////////////
// Receivers

bool CProtocol::Receive6(CBuffer &buf, CIp &ip, int time_ms)
{
	return m_Socket6.Receive(buf, ip, time_ms);
}

bool CProtocol::Receive4(CBuffer &buf, CIp &ip, int time_ms)
{
	return m_Socket4.Receive(buf, ip, time_ms);
}

bool CProtocol::ReceiveDS(CBuffer &buf, CIp &ip, int time_ms)
{
	auto fd4 = m_Socket4.GetSocket();
	auto fd6 = m_Socket6.GetSocket();

	if (fd4 < 0)
	{
		if (fd6 < 0)
			return false;
		return m_Socket6.Receive(buf, ip, time_ms);
	}
	else if (fd6 < 0)
		return m_Socket4.Receive(buf, ip, time_ms);

	fd_set fset;
	FD_ZERO(&fset);
	FD_SET(fd4, &fset);
	FD_SET(fd6, &fset);
	int max = (fd4 > fd6) ? fd4 : fd6;
	struct timeval tv;
	tv.tv_sec = time_ms / 1000;
	tv.tv_usec = (time_ms % 1000) * 1000;

	auto rval = select(max+1, &fset, 0, 0, &tv);
	if (rval <= 0)
	{
		if (rval < 0)
			std::cerr << "ReceiveDS select error: " << strerror(errno) << std::endl;
		return false;
	}

	if (FD_ISSET(fd4, &fset))
		return m_Socket4.ReceiveFrom(buf, ip);
	else
		return m_Socket6.ReceiveFrom(buf, ip);
}

////////////////////////////////////////////////////////////////////////////////////////
// dual stack senders

void CProtocol::Send(const CBuffer &buf, const CIp &Ip) const
{
	switch (Ip.GetFamily()) {
		case AF_INET:
			m_Socket4.Send(buf, Ip);
			break;
		case AF_INET6:
			m_Socket6.Send(buf, Ip);
			break;
		default:
			std::cerr << "Wrong family: " << Ip.GetFamily() << std::endl;
			break;
	}
}

void CProtocol::Send(const char *buf, const CIp &Ip) const
{
	switch (Ip.GetFamily()) {
		case AF_INET:
			m_Socket4.Send(buf, Ip);
			break;
		case AF_INET6:
			m_Socket6.Send(buf, Ip);
			break;
		default:
			std::cerr << "ERROR: wrong family: " << Ip.GetFamily() << std::endl;
			break;
	}
}

void CProtocol::Send(const CBuffer &buf, const CIp &Ip, uint16_t port) const
{
	switch (Ip.GetFamily()) {
		case AF_INET:
			m_Socket4.Send(buf, Ip, port);
			break;
		case AF_INET6:
			m_Socket6.Send(buf, Ip, port);
			break;
		default:
			std::cerr << "Wrong family: " << Ip.GetFamily() << " on port " << port << std::endl;
			break;
	}
}

void CProtocol::Send(const char *buf, const CIp &Ip, uint16_t port) const
{
	switch (Ip.GetFamily()) {
		case AF_INET:
			m_Socket4.Send(buf, Ip, port);
			break;
		case AF_INET6:
			m_Socket6.Send(buf, Ip, port);
			break;
		default:
			std::cerr << "Wrong family: " << Ip.GetFamily() << " on port " << port << std::endl;
			break;
	}
}
