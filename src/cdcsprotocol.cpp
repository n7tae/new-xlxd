//
//  cdcsprotocol.cpp
//  xlxd
//
//  Created by Jean-Luc Deltombe (LX3JL) on 07/11/2015.
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
#include <string.h>
#include "cdcsclient.h"
#include "cdcsprotocol.h"
#include "creflector.h"
#include "cgatekeeper.h"

////////////////////////////////////////////////////////////////////////////////////////
// operation

bool CDcsProtocol::Initialize(const char *type, const int ptype, const uint16 port, const bool has_ipv4, const bool has_ipv6)
{
	// base class
	if (! CProtocol::Initialize(type, ptype, port, has_ipv4, has_ipv6))
		return false;

	// update time
	m_LastKeepaliveTime.Now();

	// done
	return true;
}



////////////////////////////////////////////////////////////////////////////////////////
// task

void CDcsProtocol::Task(void)
{
	CBuffer   Buffer;
	CIp       Ip;
	CCallsign Callsign;
	char      ToLinkModule;
	std::unique_ptr<CDvHeaderPacket> Header;
	std::unique_ptr<CDvFramePacket>  Frame;

	// handle incoming packets
#if DSTAR_IPV6==true
#if DSTAR_IPV4==true
	if ( ReceiveDS(Buffer, Ip, 20) )
#else
	if ( Receive6(Buffer, Ip, 20) )
#endif
#else
	if ( Receive4(Buffer, Ip, 20) )
#endif
	{
		// crack the packet
		if ( IsValidDvPacket(Buffer, Header, Frame) )
		{
			// callsign muted?
			if ( g_GateKeeper.MayTransmit(Header->GetMyCallsign(), Ip, PROTOCOL_DCS, Header->GetRpt2Module()) )
			{
				OnDvHeaderPacketIn(Header, Ip);

				if ( !Frame->IsLastPacket() )
				{
					//std::cout << "DCS DV frame" << std::endl;
					OnDvFramePacketIn(Frame, &Ip);
				}
				else
				{
					//std::cout << "DCS DV last frame" << std::endl;
					OnDvLastFramePacketIn((std::unique_ptr<CDvLastFramePacket> &)Frame, &Ip);
				}
			}
		}
		else if ( IsValidConnectPacket(Buffer, &Callsign, &ToLinkModule) )
		{
			std::cout << "DCS connect packet for module " << ToLinkModule << " from " << Callsign << " at " << Ip << std::endl;

			// callsign authorized?
			if ( g_GateKeeper.MayLink(Callsign, Ip, PROTOCOL_DCS) && g_Reflector.IsValidModule(ToLinkModule) )
			{
				// valid module ?
				if ( g_Reflector.IsValidModule(ToLinkModule) )
				{
					// acknowledge the request
					EncodeConnectAckPacket(Callsign, ToLinkModule, &Buffer);
					Send(Buffer, Ip);

					// create the client and append
					g_Reflector.GetClients()->AddClient(std::make_shared<CDcsClient>(Callsign, Ip, ToLinkModule));
					g_Reflector.ReleaseClients();
				}
				else
				{
					std::cout << "DCS node " << Callsign << " connect attempt on non-existing module" << std::endl;

					// deny the request
					EncodeConnectNackPacket(Callsign, ToLinkModule, &Buffer);
					Send(Buffer, Ip);
				}
			}
			else
			{
				// deny the request
				EncodeConnectNackPacket(Callsign, ToLinkModule, &Buffer);
				Send(Buffer, Ip);
			}

		}
		else if ( IsValidDisconnectPacket(Buffer, &Callsign) )
		{
			std::cout << "DCS disconnect packet from " << Callsign << " at " << Ip << std::endl;

			// find client
			CClients *clients = g_Reflector.GetClients();
			std::shared_ptr<CClient>client = clients->FindClient(Ip, PROTOCOL_DCS);
			if ( client != nullptr )
			{
				// remove it
				clients->RemoveClient(client);
				// and acknowledge the disconnect
				EncodeConnectNackPacket(Callsign, ' ', &Buffer);
				Send(Buffer, Ip);
			}
			g_Reflector.ReleaseClients();
		}
		else if ( IsValidKeepAlivePacket(Buffer, &Callsign) )
		{
			//std::cout << "DCS keepalive packet from " << Callsign << " at " << Ip << std::endl;

			// find all clients with that callsign & ip and keep them alive
			CClients *clients = g_Reflector.GetClients();
			auto it = clients->begin();
			std::shared_ptr<CClient>client = nullptr;
			while ( (client = clients->FindNextClient(Callsign, Ip, PROTOCOL_DCS, it)) != nullptr )
			{
				client->Alive();
			}
			g_Reflector.ReleaseClients();
		}
		else if ( IsIgnorePacket(Buffer) )
		{
			// valid but ignore packet
			//std::cout << "DCS ignored packet from " << Ip << std::endl;
		}
		else
		{
			// invalid packet
			std::cout << "DCS packet (" << Buffer.size() << ") from " << Ip << std::endl;
		}
	}

	// handle end of streaming timeout
	CheckStreamsTimeout();

	// handle queue from reflector
	HandleQueue();

	// keep client alive
	if ( m_LastKeepaliveTime.DurationSinceNow() > DCS_KEEPALIVE_PERIOD )
	{
		//
		HandleKeepalives();

		// update time
		m_LastKeepaliveTime.Now();
	}
}

////////////////////////////////////////////////////////////////////////////////////////
// streams helpers

void CDcsProtocol::OnDvHeaderPacketIn(std::unique_ptr<CDvHeaderPacket> &Header, const CIp &Ip)
{
	// find the stream
	CPacketStream *stream = GetStream(Header->GetStreamId());
	if ( stream )
	{
		// stream already open
		// skip packet, but tickle the stream
		stream->Tickle();
	}
	else
	{
		// no stream open yet, open a new one
		CCallsign my(Header->GetMyCallsign());
		CCallsign rpt1(Header->GetRpt1Callsign());
		CCallsign rpt2(Header->GetRpt2Callsign());

		// find this client
		std::shared_ptr<CClient>client = g_Reflector.GetClients()->FindClient(Ip, PROTOCOL_DCS);
		if ( client )
		{
			// get client callsign
			rpt1 = client->GetCallsign();
			// and try to open the stream
			if ( (stream = g_Reflector.OpenStream(Header, client)) != nullptr )
			{
				// keep the handle
				m_Streams.push_back(stream);
			}
		}
		// release
		g_Reflector.ReleaseClients();

		// update last heard
		g_Reflector.GetUsers()->Hearing(my, rpt1, rpt2);
		g_Reflector.ReleaseUsers();
	}
}

////////////////////////////////////////////////////////////////////////////////////////
// queue helper

void CDcsProtocol::HandleQueue(void)
{
	m_Queue.Lock();
	while ( !m_Queue.empty() )
	{
		// get the packet
		auto packet = m_Queue.front();
		m_Queue.pop();

		// get our sender's id
		int iModId = g_Reflector.GetModuleIndex(packet->GetModuleId());

		// check if it's header and update cache
		if ( packet->IsDvHeader() )
		{
			// this relies on queue feeder setting valid module id
			m_StreamsCache[iModId].m_dvHeader = CDvHeaderPacket((const CDvHeaderPacket &)*packet);
			m_StreamsCache[iModId].m_iSeqCounter = 0;
		}
		else
		{
			// encode it
			CBuffer buffer;
			if ( packet->IsLastPacket() )
			{
				EncodeDvLastPacket(
					m_StreamsCache[iModId].m_dvHeader,
					(const CDvFramePacket &)*packet,
					m_StreamsCache[iModId].m_iSeqCounter++,
					&buffer);
			}
			else if ( packet->IsDvFrame() )
			{
				EncodeDvPacket(
					m_StreamsCache[iModId].m_dvHeader,
					(const CDvFramePacket &)*packet,
					m_StreamsCache[iModId].m_iSeqCounter++,
					&buffer);
			}

			// send it
			if ( buffer.size() > 0 )
			{
				// and push it to all our clients linked to the module and who are not streaming in
				CClients *clients = g_Reflector.GetClients();
				auto it = clients->begin();
				std::shared_ptr<CClient>client = nullptr;
				while ( (client = clients->FindNextClient(PROTOCOL_DCS, it)) != nullptr )
				{
					// is this client busy ?
					if ( !client->IsAMaster() && (client->GetReflectorModule() == packet->GetModuleId()) )
					{
						// no, send the packet
						Send(buffer, client->GetIp());

					}
				}
				g_Reflector.ReleaseClients();
			}
		}
	}
	m_Queue.Unlock();
}

////////////////////////////////////////////////////////////////////////////////////////
// keepalive helpers

void CDcsProtocol::HandleKeepalives(void)
{
	// DCS protocol sends and monitors keepalives packets
	// event if the client is currently streaming
	// so, send keepalives to all
	CBuffer keepalive1;
	EncodeKeepAlivePacket(&keepalive1);

	// iterate on clients
	CClients *clients = g_Reflector.GetClients();
	auto it = clients->begin();
	std::shared_ptr<CClient>client = nullptr;
	while ( (client = clients->FindNextClient(PROTOCOL_DCS, it)) != nullptr )
	{
		// encode client's specific keepalive packet
		CBuffer keepalive2;
		EncodeKeepAlivePacket(&keepalive2, client);

		// send keepalive
		Send(keepalive1, client->GetIp());
		Send(keepalive2, client->GetIp());

		// is this client busy ?
		if ( client->IsAMaster() )
		{
			// yes, just tickle it
			client->Alive();
		}
		// check it's still with us
		else if ( !client->IsAlive() )
		{
			// no, disconnect
			CBuffer disconnect;
			EncodeDisconnectPacket(&disconnect, client);
			Send(disconnect, client->GetIp());

			// remove it
			std::cout << "DCS client " << client->GetCallsign() << " keepalive timeout" << std::endl;
			clients->RemoveClient(client);
		}

	}
	g_Reflector.ReleaseClients();
}

////////////////////////////////////////////////////////////////////////////////////////
// packet decoding helpers

bool CDcsProtocol::IsValidConnectPacket(const CBuffer &Buffer, CCallsign *callsign, char *reflectormodule)
{
	bool valid = false;
	if ( Buffer.size() == 519 )
	{
		callsign->SetCallsign(Buffer.data(), 8);
		callsign->SetModule(Buffer.data()[8]);
		*reflectormodule = Buffer.data()[9];
		valid = (callsign->IsValid() && IsLetter(*reflectormodule));
	}
	return valid;
}

bool CDcsProtocol::IsValidDisconnectPacket(const CBuffer &Buffer, CCallsign *callsign)
{
	bool valid = false;
	if ((Buffer.size() == 11) && (Buffer.data()[9] == ' '))
	{
		callsign->SetCallsign(Buffer.data(), 8);
		callsign->SetModule(Buffer.data()[8]);
		valid = callsign->IsValid();
	}
	else if ((Buffer.size() == 19) && (Buffer.data()[9] == ' ') && (Buffer.data()[10] == 0x00))
	{
		callsign->SetCallsign(Buffer.data(), 8);
		callsign->SetModule(Buffer.data()[8]);
		valid = callsign->IsValid();
	}
	return valid;
}

bool CDcsProtocol::IsValidKeepAlivePacket(const CBuffer &Buffer, CCallsign *callsign)
{
	bool valid = false;
	if ( (Buffer.size() == 17) || (Buffer.size() == 15) || (Buffer.size() == 22) )
	{
		callsign->SetCallsign(Buffer.data(), 8);
		valid = callsign->IsValid();
	}
	return valid;
}

bool CDcsProtocol::IsValidDvPacket(const CBuffer &Buffer, std::unique_ptr<CDvHeaderPacket> &header, std::unique_ptr<CDvFramePacket> &frame)
{
	uint8 tag[] = { '0','0','0','1' };

	if ( (Buffer.size() >= 100) && (Buffer.Compare(tag, sizeof(tag)) == 0) )
	{
		// get the header
		header = std::unique_ptr<CDvHeaderPacket>(new CDvHeaderPacket((struct dstar_header *)&(Buffer.data()[4]), *((uint16 *)&(Buffer.data()[43])), 0x80));

		// get the frame
		if ( Buffer.data()[45] & 0x40U )
		{
			// it's the last frame
			frame = std::unique_ptr<CDvLastFramePacket>(new CDvLastFramePacket((struct dstar_dvframe *)&(Buffer.data()[46]), *((uint16 *)&(Buffer.data()[43])), Buffer.data()[45]));
		}
		else
		{
			// it's a regular DV frame
			frame = std::unique_ptr<CDvFramePacket>(new CDvFramePacket((struct dstar_dvframe *)&(Buffer.data()[46]), *((uint16 *)&(Buffer.data()[43])), Buffer.data()[45]));
		}

		// check validity of packets
		if ( header && header->IsValid() && frame && frame->IsValid() )
			return true;
	}
	return false;
}

bool CDcsProtocol::IsIgnorePacket(const CBuffer &Buffer)
{
	uint8 tag[] = { 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00, };

	if ( 15==Buffer.size() == 15 && 0==Buffer.Compare(tag, sizeof(tag)) )
		return true;
	return false;
}


////////////////////////////////////////////////////////////////////////////////////////
// packet encoding helpers

void CDcsProtocol::EncodeKeepAlivePacket(CBuffer *Buffer)
{
	Buffer->Set(GetReflectorCallsign());
}

void CDcsProtocol::EncodeKeepAlivePacket(CBuffer *Buffer, std::shared_ptr<CClient>Client)
{
	uint8 tag[] = { 0x0A,0x00,0x20,0x20 };

	Buffer->Set((uint8 *)(const char *)GetReflectorCallsign(), CALLSIGN_LEN-1);
	Buffer->Append((uint8)Client->GetReflectorModule());
	Buffer->Append((uint8)' ');
	Buffer->Append((uint8 *)(const char *)Client->GetCallsign(), CALLSIGN_LEN-1);
	Buffer->Append((uint8)Client->GetModule());
	Buffer->Append((uint8)Client->GetModule());
	Buffer->Append(tag, sizeof(tag));
}

void CDcsProtocol::EncodeConnectAckPacket(const CCallsign &Callsign, char ReflectorModule, CBuffer *Buffer)
{
	uint8 tag[] = { 'A','C','K',0x00 };
	uint8 cs[CALLSIGN_LEN];

	Callsign.GetCallsign(cs);
	Buffer->Set(cs, CALLSIGN_LEN-1);
	Buffer->Append((uint8)' ');
	Buffer->Append((uint8)Callsign.GetModule());
	Buffer->Append((uint8)ReflectorModule);
	Buffer->Append(tag, sizeof(tag));
}

void CDcsProtocol::EncodeConnectNackPacket(const CCallsign &Callsign, char ReflectorModule, CBuffer *Buffer)
{
	uint8 tag[] = { 'N','A','K',0x00 };
	uint8 cs[CALLSIGN_LEN];

	Callsign.GetCallsign(cs);
	Buffer->Set(cs, CALLSIGN_LEN-1);
	Buffer->Append((uint8)' ');
	Buffer->Append((uint8)Callsign.GetModule());
	Buffer->Append((uint8)ReflectorModule);
	Buffer->Append(tag, sizeof(tag));
}

void CDcsProtocol::EncodeDisconnectPacket(CBuffer *Buffer, std::shared_ptr<CClient>Client)
{
	Buffer->Set((uint8 *)(const char *)Client->GetCallsign(), CALLSIGN_LEN-1);
	Buffer->Append((uint8)' ');
	Buffer->Append((uint8)Client->GetModule());
	Buffer->Append((uint8)0x00);
	Buffer->Append((uint8 *)(const char *)GetReflectorCallsign(), CALLSIGN_LEN-1);
	Buffer->Append((uint8)' ');
	Buffer->Append((uint8)0x00);
}

void CDcsProtocol::EncodeDvPacket(const CDvHeaderPacket &Header, const CDvFramePacket &DvFrame, uint32 iSeq, CBuffer *Buffer) const
{
	uint8 tag[] = { '0','0','0','1' };
	struct dstar_header DstarHeader;

	Header.ConvertToDstarStruct(&DstarHeader);

	Buffer->Set(tag, sizeof(tag));
	Buffer->Append((uint8 *)&DstarHeader, sizeof(struct dstar_header) - sizeof(uint16));
	Buffer->Append(DvFrame.GetStreamId());
	Buffer->Append((uint8)(DvFrame.GetPacketId() % 21));
	Buffer->Append((uint8 *)DvFrame.GetAmbe(), AMBE_SIZE);
	Buffer->Append((uint8 *)DvFrame.GetDvData(), DVDATA_SIZE);
	Buffer->Append((uint8)((iSeq >> 0) & 0xFF));
	Buffer->Append((uint8)((iSeq >> 8) & 0xFF));
	Buffer->Append((uint8)((iSeq >> 16) & 0xFF));
	Buffer->Append((uint8)0x01);
	Buffer->Append((uint8)0x00, 38);
}

void CDcsProtocol::EncodeDvLastPacket(const CDvHeaderPacket &Header, const CDvFramePacket &DvFrame, uint32 iSeq, CBuffer *Buffer) const
{
	EncodeDvPacket(Header, DvFrame, iSeq, Buffer);
	(Buffer->data())[45] |= 0x40;
}
