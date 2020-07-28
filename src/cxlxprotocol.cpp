//
//  cxlxprotocol.cpp
//  xlxd
//
//  Created by Jean-Luc Deltombe (LX3JL) on 28/01/2016.
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
#include "cxlxpeer.h"
#include "cbmpeer.h"
#include "cxlxprotocol.h"
#include "creflector.h"
#include "cgatekeeper.h"


////////////////////////////////////////////////////////////////////////////////////////
// operation

bool CXlxProtocol::Initalize(const char *type, const uint16 port, const bool has_ipv4, const bool has_ipv6)
{
	if (! CProtocol::Initialize(type, port, has_ipv4, has_ipv6))
		return false;

	// update time
	m_LastKeepaliveTime.Now();
	m_LastPeersLinkTime.Now();

	// done
	return true;
}

////////////////////////////////////////////////////////////////////////////////////////
// task

void CXlxProtocol::Task(void)
{
	CBuffer   Buffer;
	CIp       Ip;
	CCallsign Callsign;
	char      Modules[27];
	CVersion  Version;
	std::unique_ptr<CDvHeaderPacket>    Header;
	std::unique_ptr<CDvFramePacket>     Frame;
	std::unique_ptr<CDvLastFramePacket> LastFrame;

	// any incoming packet ?
#if XLX_IPV6==true
#if XLX_IPV4==true
	if ( ReceiveDS(Buffer, Ip, 20) )
#else
	if ( Receive6(Buffer, Ip, 20) )
#endif
#else
	if ( Receive4(Buffer, Ip, 20) )
#endif
	{
		// crack the packet
		if ( IsValidDvFramePacket(Buffer, Frame) )
		{
			OnDvFramePacketIn(Frame, &Ip);
		}
		else if ( IsValidDvHeaderPacket(Buffer, Header) )
		{
			// callsign allowed?
			if ( g_GateKeeper.MayTransmit(Header->GetMyCallsign(), Ip) )
			{
				OnDvHeaderPacketIn(Header, Ip);
			}
		}
		else if ( IsValidDvLastFramePacket(Buffer, LastFrame) )
		{
			OnDvLastFramePacketIn(LastFrame, &Ip);
		}
		else if ( IsValidConnectPacket(Buffer, &Callsign, Modules, &Version) )
		{
			std::cout << "XLX (" << Version.GetMajor() << "." << Version.GetMinor() << "." << Version.GetRevision() << ") connect packet for modules " << Modules << " from " << Callsign <<  " at " << Ip << std::endl;

			// callsign authorized?
			if ( g_GateKeeper.MayLink(Callsign, Ip, PROTOCOL_XLX, Modules) )
			{
				// acknowledge connecting request
				// following is version dependent
				switch ( GetConnectingPeerProtocolRevision(Callsign, Version) )
				{
				case XLX_PROTOCOL_REVISION_0:
				{
					// already connected ?
					CPeers *peers = g_Reflector.GetPeers();
					if ( peers->FindPeer(Callsign, Ip, PROTOCOL_XLX) == nullptr )
					{
						// acknowledge the request
						EncodeConnectAckPacket(&Buffer, Modules);
						Send(Buffer, Ip);
					}
					g_Reflector.ReleasePeers();

				}
				break;
				case XLX_PROTOCOL_REVISION_1:
				case XLX_PROTOCOL_REVISION_2:
				default:
					// acknowledge the request
					EncodeConnectAckPacket(&Buffer, Modules);
					Send(Buffer, Ip);
					break;
				}
			}
			else
			{
				// deny the request
				EncodeConnectNackPacket(&Buffer);
				Send(Buffer, Ip);
			}
		}
		else if ( IsValidAckPacket(Buffer, &Callsign, Modules, &Version)  )
		{
			std::cout << "XLX ack packet for modules " << Modules << " from " << Callsign << " at " << Ip << std::endl;

			// callsign authorized?
			if ( g_GateKeeper.MayLink(Callsign, Ip, PROTOCOL_XLX, Modules) )
			{
				// already connected ?
				CPeers *peers = g_Reflector.GetPeers();
				if ( peers->FindPeer(Callsign, Ip, PROTOCOL_XLX) == nullptr )
				{
					// create the new peer
					// this also create one client per module
					std::shared_ptr<CPeer>peer = CreateNewPeer(Callsign, Ip, Modules, Version);

					// append the peer to reflector peer list
					// this also add all new clients to reflector client list
					peers->AddPeer(peer);
				}
				g_Reflector.ReleasePeers();
			}
		}
		else if ( IsValidDisconnectPacket(Buffer, &Callsign) )
		{
			std::cout << "XLX disconnect packet from " << Callsign << " at " << Ip << std::endl;

			// find peer
			CPeers *peers = g_Reflector.GetPeers();
			std::shared_ptr<CPeer>peer = peers->FindPeer(Ip, PROTOCOL_XLX);
			if ( peer != nullptr )
			{
				// remove it from reflector peer list
				// this also remove all concerned clients from reflector client list
				// and delete them
				peers->RemovePeer(peer);
			}
			g_Reflector.ReleasePeers();
		}
		else if ( IsValidNackPacket(Buffer, &Callsign) )
		{
			std::cout << "XLX nack packet from " << Callsign << " at " << Ip << std::endl;
		}
		else if ( IsValidKeepAlivePacket(Buffer, &Callsign) )
		{
			//std::cout << "XLX keepalive packet from " << Callsign << " at " << Ip << std::endl;

			// find peer
			CPeers *peers = g_Reflector.GetPeers();
			std::shared_ptr<CPeer>peer = peers->FindPeer(Ip, PROTOCOL_XLX);
			if ( peer != nullptr )
			{
				// keep it alive
				peer->Alive();
			}
			g_Reflector.ReleasePeers();
		}
		else
		{
			std::cout << "XLX packet (" << Buffer.size() << ")" << std::endl;
		}
	}

	// handle end of streaming timeout
	CheckStreamsTimeout();

	// handle queue from reflector
	HandleQueue();

	// keep alive
	if ( m_LastKeepaliveTime.DurationSinceNow() > XLX_KEEPALIVE_PERIOD )
	{
		// handle keep alives
		HandleKeepalives();

		// update time
		m_LastKeepaliveTime.Now();
	}

	// peer connections
	if ( m_LastPeersLinkTime.DurationSinceNow() > XLX_RECONNECT_PERIOD )
	{
		// handle remote peers connections
		HandlePeerLinks();

		// update time
		m_LastPeersLinkTime.Now();
	}
}

////////////////////////////////////////////////////////////////////////////////////////
// queue helper

void CXlxProtocol::HandleQueue(void)
{
	m_Queue.Lock();
	while ( !m_Queue.empty() )
	{
		// get the packet
		auto packet = m_Queue.front();
		m_Queue.pop();

		// check if origin of packet is local
		// if not, do not stream it out as it will cause
		// network loop between linked XLX peers
		if ( packet->IsLocalOrigin() )
		{
			// encode it
			CBuffer buffer;
			if ( EncodeDvPacket(*packet, &buffer) )
			{
				// encode revision dependent version
				CBuffer bufferLegacy = buffer;
				if ( packet->IsDvFrame() && (bufferLegacy.size() == 45) )
				{
					bufferLegacy.resize(27);
				}

				// and push it to all our clients linked to the module and who are not streaming in
				CClients *clients = g_Reflector.GetClients();
				auto it = clients->begin();
				std::shared_ptr<CClient>client = nullptr;
				while ( (client = clients->FindNextClient(PROTOCOL_XLX, it)) != nullptr )
				{
					// is this client busy ?
					if ( !client->IsAMaster() && (client->GetReflectorModule() == packet->GetModuleId()) )
					{
						// no, send the packet
						// this is protocol revision dependent
						switch ( client->GetProtocolRevision() )
						{
						case XLX_PROTOCOL_REVISION_0:
						case XLX_PROTOCOL_REVISION_1:
							Send(bufferLegacy, client->GetIp());
							break;
						case XLX_PROTOCOL_REVISION_2:
						default:
#ifdef TRANSCODER_IP
							if ( g_Transcoder.IsConnected() )
							{
								Send(buffer, client->GetIp());
							}
							else
#endif
							{
								Send(bufferLegacy, client->GetIp());
							}
							break;
						}
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

void CXlxProtocol::HandleKeepalives(void)
{
	// DExtra protocol sends and monitors keepalives packets
	// event if the client is currently streaming
	// so, send keepalives to all
	CBuffer keepalive;
	EncodeKeepAlivePacket(&keepalive);

	// iterate on peers
	CPeers *peers = g_Reflector.GetPeers();
	auto pit = peers->begin();
	std::shared_ptr<CPeer>peer = nullptr;
	while ( (peer = peers->FindNextPeer(PROTOCOL_XLX, pit)) != nullptr )
	{
		// send keepalive
		Send(keepalive, peer->GetIp());

		// client busy ?
		if ( peer->IsAMaster() )
		{
			// yes, just tickle it
			peer->Alive();
		}
		// otherwise check if still with us
		else if ( !peer->IsAlive() )
		{
			// no, disconnect
			CBuffer disconnect;
			EncodeDisconnectPacket(&disconnect);
			Send(disconnect, peer->GetIp());

			// remove it
			std::cout << "XLX peer " << peer->GetCallsign() << " keepalive timeout" << std::endl;
			peers->RemovePeer(peer);
		}
	}
	g_Reflector.ReleasePeers();
}

////////////////////////////////////////////////////////////////////////////////////////
// Peers helpers

void CXlxProtocol::HandlePeerLinks(void)
{
	CBuffer buffer;

	// get the list of peers
	CPeerCallsignList *list = g_GateKeeper.GetPeerList();
	CPeers *peers = g_Reflector.GetPeers();

	// check if all our connected peers are still listed by gatekeeper
	// if not, disconnect
	auto pit = peers->begin();
	std::shared_ptr<CPeer>peer = nullptr;
	while ( (peer = peers->FindNextPeer(PROTOCOL_XLX, pit)) != nullptr )
	{
		if ( list->FindListItem(peer->GetCallsign()) == nullptr )
		{
			// send disconnect packet
			EncodeDisconnectPacket(&buffer);
			Send(buffer, peer->GetIp());
			std::cout << "Sending disconnect packet to XLX peer " << peer->GetCallsign() << std::endl;
			// remove client
			peers->RemovePeer(peer);
		}
	}

	// check if all ours peers listed by gatekeeper are connected
	// if not, connect or reconnect
	for ( auto it=list->begin(); it!=list->end(); it++ )
	{
		if ( (*it).GetCallsign().HasSameCallsignWithWildcard(CCallsign("XRF*")) )
			continue;
		if ( peers->FindPeer((*it).GetCallsign(), PROTOCOL_XLX) == nullptr )
		{
			// resolve again peer's IP in case it's a dynamic IP
			(*it).ResolveIp();
			// send connect packet to re-initiate peer link
			EncodeConnectPacket(&buffer, (*it).GetModules());
			Send(buffer, (*it).GetIp(), XLX_PORT);
			std::cout << "Sending connect packet to XLX peer " << (*it).GetCallsign() << " @ " << (*it).GetIp() << " for modules " << (*it).GetModules() << std::endl;
		}
	}

	// done
	g_Reflector.ReleasePeers();
	g_GateKeeper.ReleasePeerList();
}


////////////////////////////////////////////////////////////////////////////////////////
// streams helpers

void CXlxProtocol::OnDvHeaderPacketIn(std::unique_ptr<CDvHeaderPacket> &Header, const CIp &Ip)
{
	CCallsign peer;

	// todo: verify Packet.GetModuleId() is in authorized list of XLX of origin
	// todo: do the same for DVFrame and DVLAstFrame packets

	// tag packet as remote peer origin
	Header->SetRemotePeerOrigin();

	// find the stream
	CPacketStream *stream = GetStream(Header->GetStreamId());
	if ( stream == nullptr )
	{
		// no stream open yet, open a new one
		// find this client
		std::shared_ptr<CClient>client = g_Reflector.GetClients()->FindClient(Ip, PROTOCOL_XLX, Header->GetRpt2Module());
		if ( client != nullptr )
		{
			// and try to open the stream
			if ( (stream = g_Reflector.OpenStream(Header, client)) != nullptr )
			{
				// keep the handle
				m_Streams.push_back(stream);
			}
			// get origin
			peer = client->GetCallsign();
		}
		// release
		g_Reflector.ReleaseClients();
	}
	else
	{
		// stream already open
		// skip packet, but tickle the stream
		stream->Tickle();
	}

	// update last heard
	g_Reflector.GetUsers()->Hearing(Header->GetMyCallsign(), Header->GetRpt1Callsign(), Header->GetRpt2Callsign(), peer);
	g_Reflector.ReleaseUsers();
}

void CXlxProtocol::OnDvFramePacketIn(std::unique_ptr<CDvFramePacket> &DvFrame, const CIp *Ip)
{
	// tag packet as remote peer origin
	DvFrame->SetRemotePeerOrigin();

	// anc call base class
	CDextraProtocol::OnDvFramePacketIn(DvFrame, Ip);
}

void CXlxProtocol::OnDvLastFramePacketIn(std::unique_ptr<CDvLastFramePacket> &DvFrame, const CIp *Ip)
{
	// tag packet as remote peer origin
	DvFrame->SetRemotePeerOrigin();

	// anc call base class
	CDextraProtocol::OnDvLastFramePacketIn(DvFrame, Ip);
}


////////////////////////////////////////////////////////////////////////////////////////
// packet decoding helpers

bool CXlxProtocol::IsValidKeepAlivePacket(const CBuffer &Buffer, CCallsign *callsign)
{
	bool valid = false;
	if (Buffer.size() == 9)
	{
		callsign->SetCallsign(Buffer.data(), 8);
		valid = callsign->IsValid();
	}
	return valid;
}


bool CXlxProtocol::IsValidConnectPacket(const CBuffer &Buffer, CCallsign *callsign, char *modules, CVersion *version)
{
	bool valid = false;
	if ((Buffer.size() == 39) && (Buffer.data()[0] == 'L') && (Buffer.data()[38] == 0))
	{
		callsign->SetCallsign((const uint8 *)&(Buffer.data()[1]), 8);
		::strcpy(modules, (const char *)&(Buffer.data()[12]));
		valid = callsign->IsValid();
		*version = CVersion(Buffer.data()[9], Buffer.data()[10], Buffer.data()[11]);
		for ( unsigned i = 0; i < ::strlen(modules); i++ )
		{
			valid &= IsLetter(modules[i]);
		}
	}
	return valid;
}

bool CXlxProtocol::IsValidDisconnectPacket(const CBuffer &Buffer, CCallsign *callsign)
{
	bool valid = false;
	if ((Buffer.size() == 10) && (Buffer.data()[0] == 'U') && (Buffer.data()[9] == 0))
	{
		callsign->SetCallsign((const uint8 *)&(Buffer.data()[1]), 8);
		valid = callsign->IsValid();
	}
	return valid;
}

bool CXlxProtocol::IsValidAckPacket(const CBuffer &Buffer, CCallsign *callsign, char *modules, CVersion *version)
{
	bool valid = false;
	if ((Buffer.size() == 39) && (Buffer.data()[0] == 'A') && (Buffer.data()[38] == 0))
	{
		callsign->SetCallsign((const uint8 *)&(Buffer.data()[1]), 8);
		::strcpy(modules, (const char *)&(Buffer.data()[12]));
		valid = callsign->IsValid();
		*version = CVersion(Buffer.data()[9], Buffer.data()[10], Buffer.data()[11]);
		for ( unsigned i = 0; i < ::strlen(modules); i++ )
		{
			valid &= IsLetter(modules[i]);
		}
	}
	return valid;
}

bool CXlxProtocol::IsValidNackPacket(const CBuffer &Buffer, CCallsign *callsign)
{
	bool valid = false;
	if ((Buffer.size() == 10) && (Buffer.data()[0] == 'N') && (Buffer.data()[9] == 0))
	{
		callsign->SetCallsign((const uint8 *)&(Buffer.data()[1]), 8);
		valid = callsign->IsValid();
	}
	return valid;
}

bool CXlxProtocol::IsValidDvFramePacket(const CBuffer &Buffer, std::unique_ptr<CDvFramePacket> &dvframe)
{
	// base class first (protocol revision 1 and lower)
	if (CDextraProtocol::IsValidDvFramePacket(Buffer, dvframe))
		return true;

	// otherwise try protocol revision 2
	if ( 45==Buffer.size() && 0==Buffer.Compare((uint8 *)"DSVT", 4) && 0x20U==Buffer.data()[4] && 0x20U==Buffer.data()[8] && 0==(Buffer.data()[14] & 0x40) )
	{
		// create packet
		dvframe = std::unique_ptr<CDvFramePacket>(new CDvFramePacket(
			// sid
			*((uint16 *)&(Buffer.data()[12])),
			// dstar
			Buffer.data()[14], &(Buffer.data()[15]), &(Buffer.data()[24]),
			// dmr
			Buffer.data()[27], Buffer.data()[28], &(Buffer.data()[29]), &(Buffer.data()[38])));

		// check validity of packet
		if ( dvframe && dvframe->IsValid() )
			return true;
	}
	return false;
}

bool CXlxProtocol::IsValidDvLastFramePacket(const CBuffer &Buffer, std::unique_ptr<CDvLastFramePacket> &dvframe)
{
	// base class first (protocol revision 1 and lower)
	if (CDextraProtocol::IsValidDvLastFramePacket(Buffer, dvframe))
		return true;

	// otherwise try protocol revision 2
	if ( 45==Buffer.size() && 0==Buffer.Compare((uint8 *)"DSVT", 4) && 0x20U==Buffer.data()[4] && 0x20U==Buffer.data()[8] && (Buffer.data()[14] & 0x40U) )
	{
		// create packet
		dvframe = std::unique_ptr<CDvLastFramePacket>(new CDvLastFramePacket(
			// sid
			*((uint16 *)&(Buffer.data()[12])),
			// dstar
			Buffer.data()[14], &(Buffer.data()[15]), &(Buffer.data()[24]),
			// dmr
			Buffer.data()[27], Buffer.data()[28], &(Buffer.data()[29]), &(Buffer.data()[38])));

		// check validity of packet
		if ( dvframe && dvframe->IsValid() )
			return true;
	}
	return false;
}

////////////////////////////////////////////////////////////////////////////////////////
// packet encoding helpers

void CXlxProtocol::EncodeKeepAlivePacket(CBuffer *Buffer)
{
	Buffer->Set(GetReflectorCallsign());
}

void CXlxProtocol::EncodeConnectPacket(CBuffer *Buffer, const char *Modules)
{
	uint8 tag[] = { 'L' };

	// tag
	Buffer->Set(tag, sizeof(tag));
	// our callsign
	Buffer->resize(Buffer->size()+8);
	g_Reflector.GetCallsign().GetCallsign(Buffer->data()+1);
	// our version
	Buffer->Append((uint8)VERSION_MAJOR);
	Buffer->Append((uint8)VERSION_MINOR);
	Buffer->Append((uint8)VERSION_REVISION);
	// the modules we share
	Buffer->Append(Modules);
	Buffer->resize(39);
}

void CXlxProtocol::EncodeDisconnectPacket(CBuffer *Buffer)
{
	uint8 tag[] = { 'U' };

	// tag
	Buffer->Set(tag, sizeof(tag));
	// our callsign
	Buffer->resize(Buffer->size()+8);
	g_Reflector.GetCallsign().GetCallsign(Buffer->data()+1);
	Buffer->Append((uint8)0);
}

void CXlxProtocol::EncodeConnectAckPacket(CBuffer *Buffer, const char *Modules)
{
	uint8 tag[] = { 'A' };

	// tag
	Buffer->Set(tag, sizeof(tag));
	// our callsign
	Buffer->resize(Buffer->size()+8);
	g_Reflector.GetCallsign().GetCallsign(Buffer->data()+1);
	// our version
	Buffer->Append((uint8)VERSION_MAJOR);
	Buffer->Append((uint8)VERSION_MINOR);
	Buffer->Append((uint8)VERSION_REVISION);
	// the modules we share
	Buffer->Append(Modules);
	Buffer->resize(39);
}

void CXlxProtocol::EncodeConnectNackPacket(CBuffer *Buffer)
{
	uint8 tag[] = { 'N' };

	// tag
	Buffer->Set(tag, sizeof(tag));
	// our callsign
	Buffer->resize(Buffer->size()+8);
	g_Reflector.GetCallsign().GetCallsign(Buffer->data()+1);
	Buffer->Append((uint8)0);
}

bool CXlxProtocol::EncodeDvFramePacket(const CDvFramePacket &Packet, CBuffer *Buffer) const
{
	uint8 tag[] = { 'D','S','V','T',0x20,0x00,0x00,0x00,0x20,0x00,0x01,0x02 };

	Buffer->Set(tag, sizeof(tag));
	Buffer->Append(Packet.GetStreamId());
	Buffer->Append((uint8)(Packet.GetDstarPacketId() % 21));
	Buffer->Append((uint8 *)Packet.GetAmbe(), AMBE_SIZE);
	Buffer->Append((uint8 *)Packet.GetDvData(), DVDATA_SIZE);

	Buffer->Append((uint8)Packet.GetDmrPacketId());
	Buffer->Append((uint8)Packet.GetDmrPacketSubid());
	Buffer->Append((uint8 *)Packet.GetAmbePlus(), AMBEPLUS_SIZE);
	Buffer->Append((uint8 *)Packet.GetDvSync(), DVSYNC_SIZE);

	return true;

}

bool CXlxProtocol::EncodeDvLastFramePacket(const CDvLastFramePacket &Packet, CBuffer *Buffer) const
{
	uint8 tag[]         = { 'D','S','V','T',0x20,0x00,0x00,0x00,0x20,0x00,0x01,0x02 };
	uint8 dstarambe[]   = { 0x55,0xC8,0x7A,0x00,0x00,0x00,0x00,0x00,0x00 };
	uint8 dstardvdata[] = { 0x25,0x1A,0xC6 };

	Buffer->Set(tag, sizeof(tag));
	Buffer->Append(Packet.GetStreamId());
	Buffer->Append((uint8)((Packet.GetPacketId() % 21) | 0x40));
	Buffer->Append(dstarambe, sizeof(dstarambe));
	Buffer->Append(dstardvdata, sizeof(dstardvdata));


	Buffer->Append((uint8)Packet.GetDmrPacketId());
	Buffer->Append((uint8)Packet.GetDmrPacketSubid());
	Buffer->Append((uint8 *)Packet.GetAmbePlus(), AMBEPLUS_SIZE);
	Buffer->Append((uint8 *)Packet.GetDvSync(), DVSYNC_SIZE);

	return true;
}

////////////////////////////////////////////////////////////////////////////////////////
// protocol revision helper

int CXlxProtocol::GetConnectingPeerProtocolRevision(const CCallsign &Callsign, const CVersion &Version)
{
	int protrev;

	// BM ?
	if ( Callsign.HasSameCallsignWithWildcard(CCallsign("BM*")) )
	{
		protrev = CBmPeer::GetProtocolRevision(Version);
	}
	// otherwise, assume native xlx
	else
	{
		protrev = CXlxPeer::GetProtocolRevision(Version);
	}

	// done
	return protrev;
}

std::shared_ptr<CPeer> CXlxProtocol::CreateNewPeer(const CCallsign &Callsign, const CIp &Ip, char *Modules, const CVersion &Version)
{
	// BM ?
	if ( Callsign.HasSameCallsignWithWildcard(CCallsign("BM*")) )
	{
		return std::make_shared<CBmPeer>(Callsign, Ip, Modules, Version);
	}
	else
	{
		return std::make_shared<CXlxPeer>(Callsign, Ip, Modules, Version);
	}
}
