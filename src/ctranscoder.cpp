//
//  ctranscoder.cpp
//  xlxd
//
//  Created by Jean-Luc Deltombe (LX3JL) on 13/04/2017.
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
#include "ctranscoder.h"

////////////////////////////////////////////////////////////////////////////////////////
// define

// status
#define STATUS_IDLE                 0
#define STATUS_LOGGED               1

// timeout
#define AMBED_OPENSTREAM_TIMEOUT    200     // in ms

////////////////////////////////////////////////////////////////////////////////////////

CTranscoder g_Transcoder;


////////////////////////////////////////////////////////////////////////////////////////
// constructor

CTranscoder::CTranscoder()
{
    keep_running = true;
    m_pThread = NULL;
    m_bConnected = false;
    m_LastKeepaliveTime.Now();
    m_LastActivityTime.Now();
    m_bStreamOpened = false;
    m_StreamidOpenStream = 0;
    m_PortOpenStream = 0;
}

////////////////////////////////////////////////////////////////////////////////////////
// destructor

CTranscoder::~CTranscoder()
{
    Close();
}

////////////////////////////////////////////////////////////////////////////////////////
// initialization

bool CTranscoder::Init(void)
{
    // create port to the transcoder
    auto s = g_Reflector.GetTranscoderIp();
	m_Ip.Initialize(strchr(s, ':') ? AF_INET6 : AF_INET, TRANSCODER_PORT, s);

	// does the user not want to use a transcoder?
	if (0 == strncasecmp(s, "none", 4))
	{
		std::cout << "Transcoder will not be enabled because the transcoder IP addess is 'none'" << std::endl;
		return true;
	}

	// now open the transcoder port
	s = (AF_INET == m_Ip.GetFamily()) ? g_Reflector.GetListenIPv4() : g_Reflector.GetListenIPv6();
	CIp tc(m_Ip.GetFamily(), TRANSCODER_PORT, s);

    // create our socket
	if (tc.IsSet())
	{
    	if (! m_Socket.Open(tc)) {
			std::cerr << "Error opening socket on port UDP" << TRANSCODER_PORT << " on ip " << m_Ip << std::endl;
			return false;
		}
	}
	else
	{
		// something bad was specified for the transcoder IP?
		std::cerr << "Error initializing transcoder port using " << ((AF_INET == m_Ip.GetFamily()) ? "IPv4" : "IPv6") << " on " << s << std::endl;
		return false;
	}

    // start  thread
    keep_running = true;
    m_pThread = new std::thread(CTranscoder::Thread, this);

    return true;
}

void CTranscoder::Close(void)
{
    // close socket
    m_Socket.Close();

    // close all streams
    m_Mutex.lock();
    {
        for ( auto it=m_Streams.begin(); it!=m_Streams.end(); it++ )
        {
            delete *it;
        }
        m_Streams.clear();

    }
    m_Mutex.unlock();

    // kill threads
    keep_running = false;
    if ( m_pThread != NULL )
    {
        m_pThread->join();
        delete m_pThread;
        m_pThread = NULL;
    }
}

////////////////////////////////////////////////////////////////////////////////////////
// thread

void CTranscoder::Thread(CTranscoder *This)
{
    while (This->keep_running)
    {
        This->Task();
    }
}

void CTranscoder::Task(void)
{
    CBuffer     Buffer;
    CIp         Ip;
    uint16      StreamId;
    uint16      Port;

    // anything coming in from codec server ?
    //if ( (m_Socket.Receive(&Buffer, &Ip, 20) != -1) && (Ip == m_Ip) )
    if ( m_Socket.Receive(Buffer, Ip, 20) )
    {
        m_LastActivityTime.Now();

        // crack packet
        if ( IsValidStreamDescrPacket(Buffer, &StreamId, &Port) )
        {
            //std::cout << "Transcoder stream " << (int) StreamId << " descr packet " << std::endl;
            m_bStreamOpened = true;
            m_StreamidOpenStream = StreamId;
            m_PortOpenStream = Port;
            m_SemaphoreOpenStream.Notify();
        }
        else if ( IsValidNoStreamAvailablePacket(Buffer) )
        {
            m_bStreamOpened = false;
            m_SemaphoreOpenStream.Notify();
        }
        else if ( IsValidKeepAlivePacket(Buffer) )
        {
            if ( !m_bConnected )
            {
                std::cout << "Transcoder connected at " << Ip << std::endl;
            }
            m_bConnected = true;
        }

    }

    // keep client alive
    if ( m_LastKeepaliveTime.DurationSinceNow() > TRANSCODER_KEEPALIVE_PERIOD )
    {
        //
        HandleKeepalives();

        // update time
        m_LastKeepaliveTime.Now();
    }
 }

////////////////////////////////////////////////////////////////////////////////////////
// manage streams

CCodecStream *CTranscoder::GetStream(CPacketStream *PacketStream, uint8 uiCodecIn)
{
    CBuffer     Buffer;

    CCodecStream *stream = NULL;

    // do we need transcoding
    if ( uiCodecIn != CODEC_NONE )
    {
        // are we connected to server
        if ( m_bConnected )
        {
            // yes, post openstream request
            EncodeOpenstreamPacket(&Buffer, uiCodecIn, (uiCodecIn == CODEC_AMBEPLUS) ? CODEC_AMBE2PLUS : CODEC_AMBEPLUS);
            m_Socket.Send(Buffer, m_Ip, TRANSCODER_PORT);

            // wait relpy here
            if ( m_SemaphoreOpenStream.WaitFor(AMBED_OPENSTREAM_TIMEOUT) )
            {
                if ( m_bStreamOpened )
                {
                    std::cout << "ambed openstream ok" << std::endl;

                    // create stream object
                    stream = new CCodecStream(PacketStream, m_StreamidOpenStream, uiCodecIn, (uiCodecIn == CODEC_AMBEPLUS) ? CODEC_AMBE2PLUS : CODEC_AMBEPLUS);

                    // init it
                    if ( stream->Init(m_PortOpenStream) )
                    {
                        // and append to list
                        Lock();
                        m_Streams.push_back(stream);
                        Unlock();
                    }
                    else
                    {
                        // send close packet
                        EncodeClosestreamPacket(&Buffer, stream->GetStreamId());
                        m_Socket.Send(Buffer, m_Ip, TRANSCODER_PORT);
                        // and delete
                        delete stream;
                        stream = NULL;
                    }
                }
                else
                {
                    std::cout << "ambed openstream failed (no suitable channel available)" << std::endl;
                }
            }
            else
            {
                std::cout << "ambed openstream timeout" << std::endl;
            }

        }
    }
    return stream;
}

void CTranscoder::ReleaseStream(CCodecStream *stream)
{
    CBuffer Buffer;

    if ( stream != NULL )
    {
        // look for the stream
        bool found = false;
        Lock();
        {
            for ( auto it=m_Streams.begin(); it!=m_Streams.end(); it++ )
            {
                // compare object pointers
                if ( (*it) ==  stream )
                {
                    // send close packet
                    EncodeClosestreamPacket(&Buffer, (*it)->GetStreamId());
                    m_Socket.Send(Buffer, m_Ip, TRANSCODER_PORT);

                    // display stats
                    if ( (*it)->GetPingMin() >= 0.0 )
                    {
                        char sz[256];
                        sprintf(sz, "ambed stats (ms) : %.1f/%.1f/%.1f",
                                (*it)->GetPingMin() * 1000.0,
                                (*it)->GetPingAve() * 1000.0,
                                (*it)->GetPingMax() * 1000.0);
                        std::cout << sz << std::endl;
                    }
                    if ( (*it)->GetTimeoutPackets() > 0 )
                    {
                        char sz[256];
                        sprintf(sz, "ambed %d of %d packets timed out",
                                (*it)->GetTimeoutPackets(),
                                (*it)->GetTotalPackets());
                        std::cout << sz << std::endl;
                    }

                    // and close it
                    (*it)->Close();
                    delete (*it);
                    m_Streams.erase(it);
                    break;
                }
            }
        }
        Unlock();
    }
}

////////////////////////////////////////////////////////////////////////////////////////
// keepalive helpers

void CTranscoder::HandleKeepalives(void)
{
    CBuffer keepalive;

    // send keepalive
    EncodeKeepAlivePacket(&keepalive);
    m_Socket.Send(keepalive, m_Ip, TRANSCODER_PORT);

    // check if still with us
    if ( m_bConnected && (m_LastActivityTime.DurationSinceNow() >= TRANSCODER_KEEPALIVE_TIMEOUT) )
    {
        // no, disconnect
        m_bConnected = false;
        std::cout << "Transcoder keepalive timeout" << std::endl;
    }
}

////////////////////////////////////////////////////////////////////////////////////////
// packet decoding helpers

bool CTranscoder::IsValidKeepAlivePacket(const CBuffer &Buffer)
{
    uint8 tag[] = { 'A','M','B','E','D','P','O','N','G' };

    bool valid = false;
    if ( (Buffer.size() == 9) && (Buffer.Compare(tag, sizeof(tag)) == 0) )
    {
        valid = true;
    }
    return valid;
}

bool CTranscoder::IsValidStreamDescrPacket(const CBuffer &Buffer, uint16 *Id, uint16 *Port)
{
    uint8 tag[] = { 'A','M','B','E','D','S','T','D' };

    bool valid = false;
    if ( (Buffer.size() == 14) && (Buffer.Compare(tag, sizeof(tag)) == 0) )
    {
        *Id = *(uint16 *)(&Buffer.data()[8]);
        *Port = *(uint16 *)(&Buffer.data()[10]);
        // uint8 CodecIn = Buffer.data()[12];
        // uint8 CodecOut = Buffer.data()[13];
        valid = true;
    }
    return valid;
}

bool CTranscoder::IsValidNoStreamAvailablePacket(const CBuffer&Buffer)
{
    uint8 tag[] = { 'A','M','B','E','D','B','U','S','Y' };

    return  ( (Buffer.size() == 9) && (Buffer.Compare(tag, sizeof(tag)) == 0) );
}


////////////////////////////////////////////////////////////////////////////////////////
// packet encoding helpers

void CTranscoder::EncodeKeepAlivePacket(CBuffer *Buffer)
{
    uint8 tag[] = { 'A','M','B','E','D','P','I','N','G' };

    Buffer->Set(tag, sizeof(tag));
    Buffer->Append((uint8 *)(const char *)g_Reflector.GetCallsign(), CALLSIGN_LEN);
}

void CTranscoder::EncodeOpenstreamPacket(CBuffer *Buffer, uint8 uiCodecIn, uint8 uiCodecOut)
{
    uint8 tag[] = { 'A','M','B','E','D','O','S' };

    Buffer->Set(tag, sizeof(tag));
    Buffer->Append((uint8 *)(const char *)g_Reflector.GetCallsign(), CALLSIGN_LEN);
    Buffer->Append((uint8)uiCodecIn);
    Buffer->Append((uint8)uiCodecOut);
}

void CTranscoder::EncodeClosestreamPacket(CBuffer *Buffer, uint16 uiStreamId)
{
    uint8 tag[] = { 'A','M','B','E','D','C','S' };

    Buffer->Set(tag, sizeof(tag));
    Buffer->Append((uint16)uiStreamId);
}
