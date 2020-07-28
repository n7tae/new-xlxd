//
//  cg3protocol.cpp
//  xlxd
//
//  Created by Marius Petrescu (YO2LOJ) on 03/06/2019.
//  Copyright © 2019 Marius Petrescu (YO2LOJ). All rights reserved.
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
#include <sys/stat.h>
#include "cg3client.h"
#include "cg3protocol.h"
#include "creflector.h"
#include "cgatekeeper.h"

#include <netinet/ip.h>
#include <netinet/ip_icmp.h>


////////////////////////////////////////////////////////////////////////////////////////
// operation

bool CG3Protocol::Initalize(const char */*type*/, const uint16 /*port*/, const bool /*has_ipv4*/, const bool /*has_ipv6*/)
{
	ReadOptions();

	// init reflector apparent callsign
	m_ReflectorCallsign = g_Reflector.GetCallsign();

	// reset stop flag
	keep_running = true;

	// update the reflector callsign
	m_ReflectorCallsign.PatchCallsign(0, (const uint8 *)"XLX", 3);

	// create our sockets
	CIp ip(AF_INET, G3_DV_PORT, g_Reflector.GetListenIPv4());
	if ( ip.IsSet() )
	{
		if (! m_Socket4.Open(ip))
			return false;
	}
	else
		return false;

	//create helper socket
	ip.SetPort(G3_PRESENCE_PORT);
	if (! m_PresenceSocket.Open(ip))
	{
		std::cout << "Error opening socket on port UDP" << G3_PRESENCE_PORT << " on ip " << ip << std::endl;
		return false;
	}

	ip.SetPort(G3_CONFIG_PORT);
	if (! m_ConfigSocket.Open(ip))
	{
		std::cout << "Error opening G3 config socket on port UDP" << G3_CONFIG_PORT << " on ip " << ip << std::endl;
		return false;
	}

	if (! m_IcmpRawSocket.Open(IPPROTO_ICMP))
	{
		std::cout << "Error opening raw socket for ICMP" << std::endl;
		return false;
	}

	// start helper threads
	m_Future         = std::async(std::launch::async, &CProtocol::Thread, this);
	m_PresenceFuture = std::async(std::launch::async, &CG3Protocol::PresenceThread, this);
	m_PresenceFuture = std::async(std::launch::async, &CG3Protocol::ConfigThread, this);
	m_PresenceFuture = std::async(std::launch::async, &CG3Protocol::IcmpThread, this);

	// update time
	m_LastKeepaliveTime.Now();

	// done
	return true;
}

void CG3Protocol::Close(void)
{
	if (m_PresenceFuture.valid())
	{
		m_PresenceFuture.get();
	}

	if (m_ConfigFuture.valid())
	{
		m_ConfigFuture.get();
	}

	if (m_IcmpFuture.valid())
	{
		m_IcmpFuture.get();
	}
}


////////////////////////////////////////////////////////////////////////////////////////
// private threads

void CG3Protocol::PresenceThread()
{
	while (keep_running)
	{
		PresenceTask();
	}
}

void CG3Protocol::ConfigThread()
{
	while (keep_running)
	{
		ConfigTask();
	}
}

void CG3Protocol::IcmpThread()
{
	while (keep_running)
	{
		IcmpTask();
	}
}


////////////////////////////////////////////////////////////////////////////////////////
// presence task

void CG3Protocol::PresenceTask(void)
{
	CBuffer             Buffer;
	CIp                 ReqIp;
	CCallsign           Callsign;
	CCallsign           Owner;
	CCallsign           Terminal;


	if ( m_PresenceSocket.Receive(Buffer, ReqIp, 20) )
	{

		CIp Ip(ReqIp);
		Ip.SetPort(G3_DV_PORT);

		if (Buffer.size() == 32)
		{
			Callsign.SetCallsign(&Buffer.data()[8], 8);
			Owner.SetCallsign(&Buffer.data()[16], 8);
			Terminal.SetCallsign(&Buffer.data()[24], 8);

			std::cout << "Presence from " << Ip << " as " << Callsign << " on terminal " << Terminal << std::endl;

			// accept
			Buffer.data()[2] = 0x80; // response
			Buffer.data()[3] = 0x00; // ok

			if (m_GwAddress == 0)
			{
				Buffer.Append(*(uint32 *)m_ConfigSocket.GetLocalAddr());
			}
			else
			{
				Buffer.Append(m_GwAddress);
			}

			CClients *clients = g_Reflector.GetClients();
			auto it = clients->begin();
			std::shared_ptr<CClient>extant = nullptr;
			while ( (extant = clients->FindNextClient(PROTOCOL_G3, it)) != nullptr )
			{
				CIp ClIp = extant->GetIp();
				if (ClIp.GetAddr() == Ip.GetAddr())
				{
					break;
				}
			}

			if (extant == nullptr)
			{
				it = clients->begin();

				// do we already have a client with the same call (IP changed)?
				while ( (extant = clients->FindNextClient(PROTOCOL_G3, it)) != nullptr )
				{
					{
						if (extant->GetCallsign().HasSameCallsign(Terminal))
						{
							//delete old client
							clients->RemoveClient(extant);
							break;
						}
					}
				}

				// create new client and append
				clients->AddClient(std::make_shared<CG3Client>(Terminal, Ip));
			}
			else
			{
				// client changed callsign
				if (!extant->GetCallsign().HasSameCallsign(Terminal))
				{
					//delete old client
					clients->RemoveClient(extant);

					// create new client and append
					clients->AddClient(std::make_shared<CG3Client>(Terminal, Ip));
				}
			}
			g_Reflector.ReleaseClients();

			m_PresenceSocket.Send(Buffer, ReqIp);
		}
	}
}


////////////////////////////////////////////////////////////////////////////////////////
// configuration task

void CG3Protocol::ConfigTask(void)
{
	CBuffer             Buffer;
	CIp                 Ip;
	CCallsign           Call;
	bool                isRepeaterCall;

	if ( m_ConfigSocket.Receive(&Buffer, &Ip, 20) != -1 )
	{

		if (Buffer.size() == 16)
		{
			if (memcmp(&Buffer.data()[8], "        ", 8) == 0)
			{
				Call.SetCallsign(GetReflectorCallsign(), 8);
			}
			else
			{
				Call.SetCallsign(&Buffer.data()[8], 8);
			}

			isRepeaterCall = ((Buffer.data()[2] & 0x10) == 0x10);

			std::cout << "Config request from " << Ip << " for " << Call << " (" << ((char *)(isRepeaterCall)?"repeater":"routed") << ")" << std::endl;

			//std::cout << "Local address: " << inet_ntoa(*m_ConfigSocket.GetLocalAddr()) << std::endl;

			Buffer.data()[2] |= 0x80; // response

			if (isRepeaterCall)
			{
				if ((Call.HasSameCallsign(GetReflectorCallsign())) && (g_Reflector.IsValidModule(Call.GetModule())))
				{
					Buffer.data()[3] = 0x00; // ok
				}
				else
				{
					std::cout << "Module " << Call << " invalid" << std::endl;
					Buffer.data()[3] = 0x01; // reject
				}
			}
			else
			{
				// reject routed calls for now
				Buffer.data()[3] = 0x01; // reject
			}

			char module = Call.GetModule();

			if (!strchr(m_Modules.c_str(), module) && !strchr(m_Modules.c_str(), '*'))
			{
				// restricted
				std::cout << "Module " << Call << " restricted by configuration" << std::endl;
				Buffer.data()[3] = 0x01; // reject
			}

			// UR
			Buffer.resize(8);
			Buffer.Append((uint8 *)(const char *)Call, CALLSIGN_LEN - 1);
			Buffer.Append((uint8)module);

			// RPT1
			Buffer.Append((uint8 *)(const char *)GetReflectorCallsign(), CALLSIGN_LEN - 1);
			Buffer.Append((uint8)'G');

			// RPT2
			Buffer.Append((uint8 *)(const char *)GetReflectorCallsign(), CALLSIGN_LEN - 1);

			if (isRepeaterCall)
			{
				Buffer.Append((uint8)Call.GetModule());
			}
			else
			{
				// routed - no module for now
				Buffer.Append((uint8)' ');
			}

			if (Buffer.data()[3] == 0x00)
			{
				std::cout << "External G3 gateway address " << inet_ntoa(*(in_addr *)&m_GwAddress) << std::endl;

				if (m_GwAddress == 0)
				{
					Buffer.Append(*(uint32 *)m_ConfigSocket.GetLocalAddr());
				}
				else
				{
					Buffer.Append(m_GwAddress);
				}
			}
			else
			{
				Buffer.Append(0u);
			}

			m_ConfigSocket.Send(Buffer, Ip);
		}
	}
}

////////////////////////////////////////////////////////////////////////////////////////
// icmp task

void CG3Protocol::IcmpTask(void)
{
	CBuffer Buffer;
	CIp Ip;
	int iIcmpType;

	if ((iIcmpType = m_IcmpRawSocket.IcmpReceive(&Buffer, &Ip, 20)) != -1)
	{
		if (iIcmpType == ICMP_DEST_UNREACH)
		{
			CClients *clients = g_Reflector.GetClients();
			auto it = clients->begin();
			std::shared_ptr<CClient>client = nullptr;
			while ( (client = clients->FindNextClient(PROTOCOL_G3, it)) != nullptr )
			{
				CIp ClientIp = client->GetIp();
				if (ClientIp.GetAddr() == Ip.GetAddr())
				{
					clients->RemoveClient(client);
				}
			}
			g_Reflector.ReleaseClients();
		}
	}
}


////////////////////////////////////////////////////////////////////////////////////////
// DV task

void CG3Protocol::Task(void)
{
	CBuffer   Buffer;
	CIp       Ip;
	CCallsign Callsign;
	char      ToLinkModule;
	int       ProtRev;
	std::unique_ptr<CDvHeaderPacket>    Header;
	std::unique_ptr<CDvFramePacket>     Frame;
	std::unique_ptr<CDvLastFramePacket> LastFrame;

	// any incoming packet ?
	if ( m_Socket4.Receive(Buffer, Ip, 20) )
	{
		CIp ClIp;
		CIp *BaseIp = nullptr;
		CClients *clients = g_Reflector.GetClients();
		auto it = clients->begin();
		std::shared_ptr<CClient>client = nullptr;
		while ( (client = clients->FindNextClient(PROTOCOL_G3, it)) != nullptr )
		{
			ClIp = client->GetIp();
			if (ClIp.GetAddr() == Ip.GetAddr())
			{
				BaseIp = &ClIp;
				client->Alive();
				// supress host checks - no ping needed to trigger potential ICMPs
				// the regular data flow will do it
				m_LastKeepaliveTime.Now();
				break;
			}
		}
		g_Reflector.ReleaseClients();

		if (BaseIp != nullptr)
		{
			// crack the packet
			if ( IsValidDvFramePacket(Buffer, Frame) )
			{
				OnDvFramePacketIn(Frame, BaseIp);
			}
			else if ( IsValidDvHeaderPacket(Buffer, Header) )
			{
				// callsign muted?
				if ( g_GateKeeper.MayTransmit(Header->GetMyCallsign(), Ip, PROTOCOL_G3, Header->GetRpt2Module()) )
				{
					// handle it
					OnDvHeaderPacketIn(Header, *BaseIp);
				}
			}
			else if ( IsValidDvLastFramePacket(Buffer, LastFrame) )
			{
				OnDvLastFramePacketIn(LastFrame, BaseIp);
			}
		}
	}

	// handle end of streaming timeout
	CheckStreamsTimeout();

	// handle queue from reflector
	HandleQueue();

	// keep alive during idle if needed
	if ( m_LastKeepaliveTime.DurationSinceNow() > G3_KEEPALIVE_PERIOD )
	{
		// handle keep alives
		HandleKeepalives();

		// update time
		m_LastKeepaliveTime.Now();

		// reload option if needed - called once every G3_KEEPALIVE_PERIOD
		NeedReload();
	}
}

////////////////////////////////////////////////////////////////////////////////////////
// queue helper

void CG3Protocol::HandleQueue(void)
{
	m_Queue.Lock();
	while ( !m_Queue.empty() )
	{
		// supress host checks
		m_LastKeepaliveTime.Now();

		// get the packet
		auto packet = m_Queue.front();
		m_Queue.pop();

		// encode it
		CBuffer buffer;
		if ( EncodeDvPacket(*packet, &buffer) )
		{
			// and push it to all our clients linked to the module and who are not streaming in
			CClients *clients = g_Reflector.GetClients();
			auto it = clients->begin();
			std::shared_ptr<CClient>client = nullptr;
			while ( (client = clients->FindNextClient(PROTOCOL_G3, it)) != nullptr )
			{
				// is this client busy ?
				if ( !client->IsAMaster() && (client->GetReflectorModule() == packet->GetModuleId()) )
				{
					// not busy, send the packet
					int n = packet->IsDvHeader() ? 5 : 1;
					for ( int i = 0; i < n; i++ )
					{
						Send(buffer, client->GetIp());
					}
				}
			}
			g_Reflector.ReleaseClients();
		}
	}
	m_Queue.Unlock();
}

////////////////////////////////////////////////////////////////////////////////////////
// keepalive helpers

void CG3Protocol::HandleKeepalives(void)
{
	// G3 Terminal mode does not support keepalive
	// We will send some short packed and expect
	// A ICMP unreachable on failure
	CBuffer keepalive((uint8 *)"PING", 4);

	// iterate on clients
	CClients *clients = g_Reflector.GetClients();
	auto it = clients->begin();
	std::shared_ptr<CClient>client = nullptr;
	while ( (client = clients->FindNextClient(PROTOCOL_G3, it)) != nullptr )
	{
		if (!client->IsAlive())
		{
			clients->RemoveClient(client);
		}
		else
		{
			// send keepalive packet
			Send(keepalive, client->GetIp());
		}
	}
	g_Reflector.ReleaseClients();
}

////////////////////////////////////////////////////////////////////////////////////////
// streams helpers

void CG3Protocol::OnDvHeaderPacketIn(std::unique_ptr<CDvHeaderPacket> &Header, const CIp &Ip)
{
	// find the stream
	CPacketStream *stream = GetStream(Header->GetStreamId(), &Ip);

	if ( stream == nullptr )
	{
		// no stream open yet, open a new one
		CCallsign via(Header->GetRpt1Callsign());

		// find this client
		CClients *clients = g_Reflector.GetClients();
		auto it = clients->begin();
		std::shared_ptr<CClient>client = nullptr;
		while ( (client = clients->FindNextClient(PROTOCOL_G3, it)) != nullptr )
		{
			CIp ClIp = client->GetIp();
			if (ClIp.GetAddr() == Ip.GetAddr())
			{
				break;
			}
		}

		if ( client != nullptr )
		{

			// move it to the proper module
			if (m_ReflectorCallsign.HasSameCallsign(Header->GetRpt2Callsign()))
			{
				if (client->GetReflectorModule() != Header->GetRpt2Callsign().GetModule())
				{
					char new_module = Header->GetRpt2Callsign().GetModule();
					if (strchr(m_Modules.c_str(), '*') || strchr(m_Modules.c_str(), new_module))
					{
						client->SetReflectorModule(new_module);
					}
					else
					{
						g_Reflector.ReleaseClients();
						return;
					}
				}

				// get client callsign
				via = client->GetCallsign();

				// and try to open the stream
				if ( (stream = g_Reflector.OpenStream(Header, client)) != nullptr )
				{
					// keep the handle
					m_Streams.push_back(stream);
				}

				// update last heard
				g_Reflector.GetUsers()->Hearing(Header->GetMyCallsign(), via, Header->GetRpt2Callsign());
				g_Reflector.ReleaseUsers();
			}
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
}


////////////////////////////////////////////////////////////////////////////////////////
// packet decoding helpers

bool CG3Protocol::IsValidDvHeaderPacket(const CBuffer &Buffer, std::unique_ptr<CDvHeaderPacket> &header)
{
	if ( 56==Buffer.size() && 0==Buffer.Compare((uint8 *)"DSVT", 4) && 0x10U==Buffer.data()[4] && 0x20U==Buffer.data()[8] )
	{
		// create packet
		header = std::unique_ptr<CDvHeaderPacket>(new CDvHeaderPacket((struct dstar_header *)&(Buffer.data()[15]), *((uint16 *)&(Buffer.data()[12])), 0x80));
		// check validity of packet
		if ( header && header->IsValid() )
			return true;
	}
	return false;
}

bool CG3Protocol::IsValidDvFramePacket(const CBuffer &Buffer, std::unique_ptr<CDvFramePacket> &dvframe)
{
	if ( 27==Buffer.size() && 0==Buffer.Compare((uint8 *)"DSVT", 4) && 0x20U==Buffer.data()[4] && 0x20U==Buffer.data()[8] && 0U==(Buffer.data()[14] & 0x40U) )
	{
		// create packet
		dvframe = std::unique_ptr<CDvFramePacket>(new CDvFramePacket((struct dstar_dvframe *)&(Buffer.data()[15]), *((uint16 *)&(Buffer.data()[12])), Buffer.data()[14]));
		// check validity of packet
		if ( dvframe && dvframe->IsValid() )
			return true;
	}
	return false;
}

bool CG3Protocol::IsValidDvLastFramePacket(const CBuffer &Buffer, std::unique_ptr<CDvLastFramePacket> &dvframe)
{
	if ( 27==Buffer.size() && 0==Buffer.Compare((uint8 *)"DSVT", 4) && 0x20U==Buffer.data()[4] && 0x20U==Buffer.data()[8] && (Buffer.data()[14] & 0x40U) )
	{
		// create packet
		dvframe = std::unique_ptr<CDvLastFramePacket>(new CDvLastFramePacket((struct dstar_dvframe *)&(Buffer.data()[15]), *((uint16 *)&(Buffer.data()[12])), Buffer.data()[14]));
		// check validity of packet
		if ( dvframe && dvframe->IsValid() )
			return true;
	}
	return false;
}

////////////////////////////////////////////////////////////////////////////////////////
// packet encoding helpers

bool CG3Protocol::EncodeDvHeaderPacket(const std::unique_ptr<CDvHeaderPacket> &Packet, CBuffer *Buffer) const
{
	uint8 tag[]	= { 'D','S','V','T',0x10,0x00,0x00,0x00,0x20,0x00,0x01,0x02 };
	struct dstar_header DstarHeader;

	Packet->ConvertToDstarStruct(&DstarHeader);

	Buffer->Set(tag, sizeof(tag));
	Buffer->Append(Packet->GetStreamId());
	Buffer->Append((uint8)0x80);
	Buffer->Append((uint8 *)&DstarHeader, sizeof(struct dstar_header));

	return true;
}

bool CG3Protocol::EncodeDvFramePacket(const std::unique_ptr<CDvFramePacket> &Packet, CBuffer *Buffer) const
{
	uint8 tag[] = { 'D','S','V','T',0x20,0x00,0x00,0x00,0x20,0x00,0x01,0x02 };

	Buffer->Set(tag, sizeof(tag));
	Buffer->Append(Packet->GetStreamId());
	Buffer->Append((uint8)(Packet->GetPacketId() % 21));
	Buffer->Append((uint8 *)Packet->GetAmbe(), AMBE_SIZE);
	Buffer->Append((uint8 *)Packet->GetDvData(), DVDATA_SIZE);

	return true;

}

bool CG3Protocol::EncodeDvLastFramePacket(const std::unique_ptr<CDvLastFramePacket> &Packet, CBuffer *Buffer) const
{
	uint8 tag1[] = { 'D','S','V','T',0x20,0x00,0x00,0x00,0x20,0x00,0x01,0x02 };
	uint8 tag2[] = { 0x55,0xC8,0x7A,0x00,0x00,0x00,0x00,0x00,0x00,0x25,0x1A,0xC6 };

	Buffer->Set(tag1, sizeof(tag1));
	Buffer->Append(Packet->GetStreamId());
	Buffer->Append((uint8)((Packet->GetPacketId() % 21) | 0x40));
	Buffer->Append(tag2, sizeof(tag2));

	return true;
}

////////////////////////////////////////////////////////////////////////////////////////
// option helpers

char *CG3Protocol::TrimWhiteSpaces(char *str)
{
	char *end;
	while ((*str == ' ') || (*str == '\t')) str++;
	if (*str == 0)
		return str;
	end = str + strlen(str) - 1;
	while ((end > str) && ((*end == ' ') || (*end == '\t') || (*end == '\r'))) end --;
	*(end + 1) = 0;
	return str;
}


void CG3Protocol::NeedReload(void)
{
	struct stat fileStat;

	if (::stat(TERMINALOPTIONS_PATH, &fileStat) != -1)
	{
		if (m_LastModTime != fileStat.st_mtime)
		{
			ReadOptions();

			// we have new options - iterate on clients for potential removal
			CClients *clients = g_Reflector.GetClients();
			auto it = clients->begin();
			std::shared_ptr<CClient>client = nullptr;
			while ( (client = clients->FindNextClient(PROTOCOL_G3, it)) != nullptr )
			{
				char module = client->GetReflectorModule();
				if (!strchr(m_Modules.c_str(), module) && !strchr(m_Modules.c_str(), '*'))
				{
					clients->RemoveClient(client);
				}
			}
			g_Reflector.ReleaseClients();
		}
	}
}

void CG3Protocol::ReadOptions(void)
{
	char sz[256];
	int opts = 0;


	std::ifstream file(TERMINALOPTIONS_PATH);
	if (file.is_open())
	{
		m_GwAddress = 0u;
		m_Modules = "*";

		while (file.getline(sz, sizeof(sz)).good())
		{
			char *szt = TrimWhiteSpaces(sz);
			char *szval;

			if ((::strlen(szt) > 0) && szt[0] != '#')
			{
				if ((szt = ::strtok(szt, " ,\t")) != nullptr)
				{
					if ((szval = ::strtok(nullptr, " ,\t")) != nullptr)
					{
						if (::strncmp(szt, "address", 7) == 0)
						{
							in_addr addr = { .s_addr = inet_addr(szval) };
							if (addr.s_addr)
							{
								std::cout << "G3 handler address set to " << inet_ntoa(addr) << std::endl;
								m_GwAddress = addr.s_addr;
								opts++;
							}
						}
						else if (strncmp(szt, "modules", 7) == 0)
						{
							std::cout << "G3 handler module list set to " << szval << std::endl;
							m_Modules = szval;
							opts++;
						}
						else
						{
							// unknown option - ignore
						}
					}
				}
			}
		}
		std::cout << "G3 handler loaded " << opts << " options from file " << TERMINALOPTIONS_PATH << std::endl;
		file.close();

		struct stat fileStat;

		if (::stat(TERMINALOPTIONS_PATH, &fileStat) != -1)
		{
			m_LastModTime = fileStat.st_mtime;
		}
	}
}
