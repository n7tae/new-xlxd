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


CProtocol::CProtocol()
{
    m_bStopThread = false;
    m_pThread = NULL;
}


////////////////////////////////////////////////////////////////////////////////////////
// destructor

CProtocol::~CProtocol()
{
    // kill threads
    m_bStopThread = true;
    if ( m_pThread != NULL )
    {
        m_pThread->join();
        delete m_pThread;
    }

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

bool CProtocol::Init(void)
{
    // init reflector apparent callsign
    m_ReflectorCallsign = g_Reflector.GetCallsign();

    // reset stop flag
    m_bStopThread = false;

    // start  thread;
    m_pThread = new std::thread(CProtocol::Thread, this);

    // done
    return true;
}

void CProtocol::Close(void)
{
    m_bStopThread = true;
    if ( m_pThread != NULL )
    {
        m_pThread->join();
        delete m_pThread;
        m_pThread = NULL;
    }
}


////////////////////////////////////////////////////////////////////////////////////////
// thread

void CProtocol::Thread(CProtocol *This)
{
    while ( !This->m_bStopThread )
    {
        This->Task();
    }
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
		std::cout << "Deleting orphaned Frame with ID " << Frame->GetStreamId() << " on " << Ip << std::endl;
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
		std::cout << "Deleting orphaned Last Frame with ID " << Frame->GetStreamId() << " on " << Ip << std::endl;
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
