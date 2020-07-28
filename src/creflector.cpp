//
//  creflector.cpp
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

#include "main.h"
#include <string.h>
#include "creflector.h"
#include "cgatekeeper.h"
#include "cdmriddirfile.h"
#include "cdmriddirhttp.h"
#include "ctranscoder.h"
#include "cysfnodedirfile.h"
#include "cysfnodedirhttp.h"

////////////////////////////////////////////////////////////////////////////////////////
// constructor

CReflector::CReflector()
{
	keep_running = true;
#ifdef DEBUG_DUMPFILE
	m_DebugFile.open("/Users/jeanluc/Desktop/xlxdebug.txt");
#endif
}

CReflector::CReflector(const CCallsign &callsign)
{
#ifdef DEBUG_DUMPFILE
	m_DebugFile.close();
#endif
	keep_running = true;
	m_Callsign = callsign;
}

////////////////////////////////////////////////////////////////////////////////////////
// destructor

CReflector::~CReflector()
{
	keep_running = false;
	if ( m_XmlReportFuture.valid() )
	{
		m_XmlReportFuture.get();
	}
#ifdef JSON_MONITOR
	if ( m_JsonReportFuture.valid() )
	{
		m_JsonReportFuture.get();
	}
#endif
	for ( int i = 0; i < NB_OF_MODULES; i++ )
	{
		if ( m_RouterFuture[i].valid() )
		{
			m_RouterFuture[i].get();
		}
	}
}


////////////////////////////////////////////////////////////////////////////////////////
// operation

bool CReflector::Start(void)
{
	// let's go!
	keep_running = true;

	// init gate keeper. It can only return true!
	g_GateKeeper.Init();

#ifndef NO_XLX
	// init dmrid directory. No need to check the return value.
	g_DmridDir.Init();

	// init wiresx node directory. Likewise with the return vale.
	g_YsfNodeDir.Init();

#ifdef TRANSCODER_IP
	// init the transcoder
	if (! g_Transcoder.Init())
		return false;
#endif
#endif

	// create protocols
	if (! m_Protocols.Init())
	{
		m_Protocols.Close();
		return false;
	}

	// start one thread per reflector module
	for ( int i = 0; i < NB_OF_MODULES; i++ )
	{
		m_RouterFuture[i] = std::async(std::launch::async, &CReflector::RouterThread, this, &(m_Stream[i]));
	}

	// start the reporting threads
	m_XmlReportFuture = std::async(std::launch::async, &CReflector::XmlReportThread, this);
#ifdef JSON_MONITOR
	m_JsonReportFuture = std::async(std::launch::async, &CReflector::JsonReportThread, this);
#endif

	return true;
}

void CReflector::Stop(void)
{
	// stop & delete all threads
	keep_running = false;

	// stop & delete report threads
	if ( m_XmlReportFuture.valid() )
	{
		m_XmlReportFuture.get();
	}
#ifdef JSON_MONITOR
	if ( m_JsonReportFuture.valid() )
	{
		m_JsonReportFuture.get();
	}
#endif

	// stop & delete all router thread
	for ( int i = 0; i < NB_OF_MODULES; i++ )
	{
		if ( m_RouterFuture[i].valid() )
		{
			m_RouterFuture[i].get();
		}
	}

	// close protocols
	m_Protocols.Close();

	// close gatekeeper
	g_GateKeeper.Close();

#ifdef TRANSCODER_IP
	// close transcoder
	g_Transcoder.Close();
#endif

#ifndef NO_XLX
	// close databases
	g_DmridDir.Close();
	g_YsfNodeDir.Close();
#endif
}

////////////////////////////////////////////////////////////////////////////////////////
// stream opening & closing

bool CReflector::IsStreaming(char module)
{
	return false;
}

// clients MUST have bee locked by the caller so we can freely access it within the fuction
CPacketStream *CReflector::OpenStream(std::unique_ptr<CDvHeaderPacket> &DvHeader, std::shared_ptr<CClient>client)
{
	// check sid is not zero
	if ( 0U == DvHeader->GetStreamId() )
		return nullptr;

	// check if client is valid candidate
	if ( ! m_Clients.IsClient(client) || client->IsAMaster() )
		return nullptr;

	// check if no stream with same streamid already open
	// to prevent loops
	if ( IsStreamOpen(DvHeader) )
	{
		std::cerr << "Detected stream loop on module " << DvHeader->GetRpt2Module() << " for client " << client->GetCallsign() << " with sid " << DvHeader->GetStreamId() << std::endl;
		return nullptr;
	}

	// get the module's queue
	char module = DvHeader->GetRpt2Module();
	CPacketStream *stream = GetStream(module);
	if ( stream == nullptr )
		return nullptr;

	stream->Lock();
	// is it available ?
	if ( stream->Open(*DvHeader, client) )
	{
		// stream open, mark client as master
		// so that it can't be deleted
		client->SetMasterOfModule(module);

		// update last heard time
		client->Heard();

		// and push header packet
		stream->Push(std::move(DvHeader));

		// report
		std::cout << "Opening stream on module " << module << " for client " << client->GetCallsign() << " with sid " << DvHeader->GetStreamId() << " by user " << DvHeader->GetMyCallsign() << std::endl;

		// notify
		g_Reflector.OnStreamOpen(stream->GetUserCallsign());

	}
	stream->Unlock();
	return stream;
}

void CReflector::CloseStream(CPacketStream *stream)
{
	if ( stream != nullptr )
	{
		// wait queue is empty. this waits forever
		bool bEmpty = false;
		do
		{
			stream->Lock();
			// do not use stream->IsEmpty() has this "may" never succeed
			// and anyway, the DvLastFramPacket short-circuit the transcoder
			// loop queues
			bEmpty = stream->empty();
			stream->Unlock();
			if ( !bEmpty )
				CTimePoint::TaskSleepFor(10);
		}
		while (!bEmpty);

		GetClients();	// lock clients
		stream->Lock();	// lock stream

		// get and check the master
		std::shared_ptr<CClient>client = stream->GetOwnerClient();
		if ( client != nullptr )
		{
			// client no longer a master
			client->NotAMaster();

			// notify
			OnStreamClose(stream->GetUserCallsign());

			std::cout << "Closing stream of module " << GetStreamModule(stream) << std::endl;
		}

		// release clients
		ReleaseClients();

		// unlock before closing
		// to avoid double lock in assiociated
		// codecstream close/thread-join
		stream->Unlock();

		// and stop the queue
		stream->Close();
	}
}

////////////////////////////////////////////////////////////////////////////////////////
// router threads

void CReflector::RouterThread(CPacketStream *streamIn)
{
	// get our module
	uint8 uiModuleId = GetStreamModule(streamIn);

	// get on input queue
	std::unique_ptr<CPacket> packet;

	while (keep_running)
	{
		// any packet in our input queue ?
		streamIn->Lock();
		if ( !streamIn->empty() )
		{
			// get the packet
			packet = streamIn->front();
			streamIn->pop();
		}
		else
		{
			packet = nullptr;
		}
		streamIn->Unlock();

		// route it
		if ( packet != nullptr )
		{
			// set origin
			packet->SetModuleId(uiModuleId);

			// iterate on all protocols
			m_Protocols.Lock();
			for ( auto it=m_Protocols.begin(); it!=m_Protocols.end(); it++ )
			{
				// duplicate packet
				auto packetClone = packet->Duplicate();

				// if packet is header, update RPT2 according to protocol
				if ( packetClone->IsDvHeader() )
				{
					// get our callsign
					CCallsign csRPT = (*it)->GetReflectorCallsign();
					csRPT.SetModule(GetStreamModule(streamIn));
					(dynamic_cast<CDvHeaderPacket *>(packetClone.get()))->SetRpt2Callsign(csRPT);
				}

				// and push it
				CPacketQueue *queue = (*it)->GetQueue();
				queue->push(packetClone);
				(*it)->ReleaseQueue();
			}
			m_Protocols.Unlock();
		}
		else
		{
			CTimePoint::TaskSleepFor(10);
		}
	}
}

////////////////////////////////////////////////////////////////////////////////////////
// report threads

void CReflector::XmlReportThread()
{
	while (keep_running)
	{
		// report to xml file
		std::ofstream xmlFile;
		xmlFile.open(XML_PATH, std::ios::out | std::ios::trunc);
		if ( xmlFile.is_open() )
		{
			// write xml file
			WriteXmlFile(xmlFile);

			// and close file
			xmlFile.close();
		}
#ifndef DEBUG_NO_ERROR_ON_XML_OPEN_FAIL
		else
		{
			std::cout << "Failed to open " << XML_PATH  << std::endl;
		}
#endif

		// and wait a bit
		for (int i=0; i< XML_UPDATE_PERIOD && keep_running; i++)
			CTimePoint::TaskSleepFor(1000);
	}
}

#ifdef JSON_MONITOR
void CReflector::JsonReportThread()
{
	CUdpSocket Socket;
	CBuffer    Buffer;
	CIp        Ip;
	bool       bOn;

	// init variable
	bOn = false;

	// create listening socket
	if ( Socket.Open(JSON_PORT) )
	{
		// and loop
		while (keep_running)
		{
			// any command ?
			if ( Socket.Receive(Buffer, Ip, 50) )
			{
				// check verb
				if ( Buffer.Compare((uint8 *)"hello", 5) == 0 )
				{
					std::cout << "Monitor socket connected with " << Ip << std::endl;

					// connected
					bOn = true;

					// announce ourselves
					SendJsonReflectorObject(Socket, Ip);

					// dump tables
					SendJsonNodesObject(Socket, Ip);
					SendJsonStationsObject(Socket, Ip);
				}
				else if ( Buffer.Compare((uint8 *)"bye", 3) == 0 )
				{
					std::cout << "Monitor socket disconnected" << std::endl;

					// diconnected
					bOn = false;
				}
			}

			// any notifications ?
			CNotification notification;
			m_Notifications.Lock();
			if ( !m_Notifications.empty() )
			{
				// get the packet
				notification = m_Notifications.front();
				m_Notifications.pop();
			}
			m_Notifications.Unlock();

			// handle it
			if ( bOn )
			{
				switch ( notification.GetId() )
				{
				case NOTIFICATION_CLIENTS:
				case NOTIFICATION_PEERS:
					//std::cout << "Monitor updating nodes table" << std::endl;
					SendJsonNodesObject(Socket, Ip);
					break;
				case NOTIFICATION_USERS:
					//std::cout << "Monitor updating stations table" << std::endl;
					SendJsonStationsObject(Socket, Ip);
					break;
				case NOTIFICATION_STREAM_OPEN:
					//std::cout << "Monitor notify station " << notification.GetCallsign() << "going ON air" << std::endl;
					SendJsonStationsObject(Socket, Ip);
					SendJsonOnairObject(Socket, Ip, notification.GetCallsign());
					break;
				case NOTIFICATION_STREAM_CLOSE:
					//std::cout << "Monitor notify station " << notification.GetCallsign() << "going OFF air" << std::endl;
					SendJsonOffairObject(Socket, Ip, notification.GetCallsign());
					break;
				case NOTIFICATION_NONE:
				default:
					// nothing to do, just sleep a bit
					CTimePoint::TaskSleepFor(250);
					break;
				}
			}
		}
	}
	else
	{
		std::cout << "Error creating monitor socket" << std::endl;
	}
}
#endif

////////////////////////////////////////////////////////////////////////////////////////
// notifications

void CReflector::OnPeersChanged(void)
{
	CNotification notification(NOTIFICATION_PEERS);

	m_Notifications.Lock();
	m_Notifications.push(notification);
	m_Notifications.Unlock();
}

void CReflector::OnClientsChanged(void)
{
	CNotification notification(NOTIFICATION_CLIENTS);

	m_Notifications.Lock();
	m_Notifications.push(notification);
	m_Notifications.Unlock();
}

void CReflector::OnUsersChanged(void)
{
	CNotification notification(NOTIFICATION_USERS);

	m_Notifications.Lock();
	m_Notifications.push(notification);
	m_Notifications.Unlock();
}

void CReflector::OnStreamOpen(const CCallsign &callsign)
{
	CNotification notification(NOTIFICATION_STREAM_OPEN, callsign);

	m_Notifications.Lock();
	m_Notifications.push(notification);
	m_Notifications.Unlock();
}

void CReflector::OnStreamClose(const CCallsign &callsign)
{
	CNotification notification(NOTIFICATION_STREAM_CLOSE, callsign);

	m_Notifications.Lock();
	m_Notifications.push(notification);
	m_Notifications.Unlock();
}

////////////////////////////////////////////////////////////////////////////////////////
// modules & queues

int CReflector::GetModuleIndex(char module) const
{
	int i = (int)module - (int)'A';
	if ( (i < 0) || (i >= NB_OF_MODULES) )
	{
		i = -1;
	}
	return i;
}

CPacketStream *CReflector::GetStream(char module)
{
	int i = GetModuleIndex(module);
	if ( i >= 0 )
	{
		return &(m_Stream[i]);
	}
	return nullptr;
}

bool CReflector::IsStreamOpen(const std::unique_ptr<CDvHeaderPacket> &DvHeader)
{
	for ( unsigned i = 0; i < m_Stream.size(); i++  )
	{
		if ( (m_Stream[i].GetStreamId() == DvHeader->GetStreamId()) && (m_Stream[i].IsOpen()) )
			return true;
	}
	return false;
}

char CReflector::GetStreamModule(CPacketStream *stream)
{
	for ( unsigned i = 0; i < m_Stream.size(); i++ )
	{
		if ( &(m_Stream[i]) == stream )
			return GetModuleLetter(i);
	}
	return ' ';
}

////////////////////////////////////////////////////////////////////////////////////////
// xml helpers

void CReflector::WriteXmlFile(std::ofstream &xmlFile)
{
	// write header
	xmlFile << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>" << std::endl;

	// software version
	char sz[64];
	::sprintf(sz, "%d.%d.%d", VERSION_MAJOR, VERSION_MINOR, VERSION_REVISION);
	xmlFile << "<Version>" << sz << "</Version>" << std::endl;

	// linked peers
	xmlFile << "<" << m_Callsign << "linked peers>" << std::endl;
	// lock
	CPeers *peers = GetPeers();
	// iterate on peers
	for ( auto pit=peers->cbegin(); pit!=peers->cend(); pit++ )
	{
		(*pit)->WriteXml(xmlFile);
	}
	// unlock
	ReleasePeers();
	xmlFile << "</" << m_Callsign << "linked peers>" << std::endl;

	// linked nodes
	xmlFile << "<" << m_Callsign << "linked nodes>" << std::endl;
	// lock
	CClients *clients = GetClients();
	// iterate on clients
	for ( auto cit=clients->cbegin(); cit!=clients->cend(); cit++ )
	{
		if ( (*cit)->IsNode() )
		{
			(*cit)->WriteXml(xmlFile);
		}
	}
	// unlock
	ReleaseClients();
	xmlFile << "</" << m_Callsign << "linked nodes>" << std::endl;

	// last heard users
	xmlFile << "<" << m_Callsign << "heard users>" << std::endl;
	// lock
	CUsers *users = GetUsers();
	// iterate on users
	for ( auto it=users->begin(); it!=users->end(); it++ )
	{
		(*it).WriteXml(xmlFile);
	}
	// unlock
	ReleaseUsers();
	xmlFile << "</" << m_Callsign << "heard users>" << std::endl;
}


#ifdef JSON_MONITOR
////////////////////////////////////////////////////////////////////////////////////////
// json helpers

void CReflector::SendJsonReflectorObject(CUdpSocket &Socket, CIp &Ip)
{
	char Buffer[1024];
	char cs[CALLSIGN_LEN+1];
	char mod[8] = "\"A\"";

	// get reflector callsign
	m_Callsign.GetCallsign((uint8 *)cs);
	cs[CALLSIGN_LEN] = 0;

	// build string
	::sprintf(Buffer, "{\"reflector\":\"%s\",\"modules\":[", cs);
	for ( int i = 0; i < NB_OF_MODULES; i++ )
	{
		::strcat(Buffer, mod);
		mod[1]++;
		if ( i < NB_OF_MODULES-1 )
		{
			::strcat(Buffer, ",");
		}
	}
	::strcat(Buffer, "]}");

	// and send
	Socket.Send(Buffer, Ip);
}

#define JSON_NBMAX_NODES	250

void CReflector::SendJsonNodesObject(CUdpSocket &Socket, CIp &Ip)
{
	char Buffer[12+(JSON_NBMAX_NODES*94)];

	// nodes object table
	::sprintf(Buffer, "{\"nodes\":[");
	// lock
	CClients *clients = GetClients();
	// iterate on clients
	for ( auto it=clients->cbegin(); it!=clients->cend(); )
	{
		(*it++)->GetJsonObject(Buffer);
		if ( it != clients->cend() )
		{
			::strcat(Buffer, ",");
		}
	}
	// unlock
	ReleaseClients();
	::strcat(Buffer, "]}");

	// and send
	//std::cout << Buffer << std::endl;
	Socket.Send(Buffer, Ip);
}

void CReflector::SendJsonStationsObject(CUdpSocket &Socket, CIp &Ip)
{
	char Buffer[15+(LASTHEARD_USERS_MAX_SIZE*94)];

	// stations object table
	::sprintf(Buffer, "{\"stations\":[");

	// lock
	CUsers *users = GetUsers();
	// iterate on users
	for ( auto it=users->begin(); it!=users->end(); )
	{
		(*it++).GetJsonObject(Buffer);
		if ( it != users->end() )
		{
			::strcat(Buffer, ",");
		}
	}
	// unlock
	ReleaseUsers();

	::strcat(Buffer, "]}");

	// and send
	//std::cout << Buffer << std::endl;
	Socket.Send(Buffer, Ip);
}

void CReflector::SendJsonOnairObject(CUdpSocket &Socket, CIp &Ip, const CCallsign &Callsign)
{
	char Buffer[128];
	char sz[CALLSIGN_LEN+1];

	// onair object
	Callsign.GetCallsignString(sz);
	::sprintf(Buffer, "{\"onair\":\"%s\"}", sz);

	// and send
	//std::cout << Buffer << std::endl;
	Socket.Send(Buffer, Ip);
}

void CReflector::SendJsonOffairObject(CUdpSocket &Socket, CIp &Ip, const CCallsign &Callsign)
{
	char Buffer[128];
	char sz[CALLSIGN_LEN+1];

	// offair object
	Callsign.GetCallsignString(sz);
	::sprintf(Buffer, "{\"offair\":\"%s\"}", sz);

	// and send
	//std::cout << Buffer << std::endl;
	Socket.Send(Buffer, Ip);
}
#endif
