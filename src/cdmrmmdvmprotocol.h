//
//  cdmrmmdvmprotocol.h
//  xlxd
//
//  Created by Jean-Luc Deltombe (LX3JL) on 04/03/2017.
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

#ifndef cdmrmmdvmprotocol_h
#define cdmrmmdvmprotocol_h

#include "ctimepoint.h"
#include "cprotocol.h"
#include "cdvheaderpacket.h"
#include "cdvframepacket.h"
#include "cdvlastframepacket.h"

////////////////////////////////////////////////////////////////////////////////////////
// define

// frame type
#define DMRMMDVM_FRAMETYPE_VOICE     0
#define DMRMMDVM_FRAMETYPE_VOICESYNC 1
#define DMRMMDVM_FRAMETYPE_DATASYNC  2

// slot type
#define MMDVM_SLOTTYPE_HEADER        1
#define MMDVM_SLOTTYPE_TERMINATOR    2

// DMRMMDVM Module ID
#define MMDVM_MODULE_ID             'B'

////////////////////////////////////////////////////////////////////////////////////////
// class

class CDmrmmdvmStreamCacheItem
{
public:
	CDmrmmdvmStreamCacheItem()     {}
	~CDmrmmdvmStreamCacheItem()    {}

	CDvHeaderPacket m_dvHeader;
	CDvFramePacket  m_dvFrame0;
	CDvFramePacket  m_dvFrame1;

	uint8  m_uiSeqId;
};


class CDmrmmdvmProtocol : public CProtocol
{
public:
	// initialization
	bool Initialize(const char *type, const uint16 port, const bool has_ipv4, const bool has_ipv6);

	// task
	void Task(void);

protected:
	// queue helper
	void HandleQueue(void);

	// keepalive helpers
	void HandleKeepalives(void);

	// stream helpers
	void OnDvHeaderPacketIn(std::unique_ptr<CDvHeaderPacket> &, const CIp &, uint8, uint8);

	// packet decoding helpers
	bool IsValidConnectPacket(const CBuffer &, CCallsign *, const CIp &);
	bool IsValidAuthenticationPacket(const CBuffer &, CCallsign *, const CIp &);
	bool IsValidDisconnectPacket(const CBuffer &, CCallsign *);
	bool IsValidConfigPacket(const CBuffer &, CCallsign *, const CIp &);
	bool IsValidOptionPacket(const CBuffer &, CCallsign *);
	bool IsValidKeepAlivePacket(const CBuffer &, CCallsign *);
	bool IsValidRssiPacket(const CBuffer &, CCallsign *, int *);
	bool IsValidDvHeaderPacket(const CBuffer &, std::unique_ptr<CDvHeaderPacket> &, uint8 *, uint8 *);
	bool IsValidDvFramePacket(const CBuffer &, std::array<std::unique_ptr<CDvFramePacket>, 3> &);
	bool IsValidDvLastFramePacket(const CBuffer &, std::unique_ptr<CDvLastFramePacket> &);

	// packet encoding helpers
	void EncodeKeepAlivePacket(CBuffer *, std::shared_ptr<CClient>);
	void EncodeAckPacket(CBuffer *, const CCallsign &);
	void EncodeConnectAckPacket(CBuffer *, const CCallsign &, uint32);
	void EncodeNackPacket(CBuffer *, const CCallsign &);
	void EncodeClosePacket(CBuffer *, std::shared_ptr<CClient>);
	bool EncodeDvHeaderPacket(const CDvHeaderPacket &, uint8, CBuffer *) const;
	void EncodeDvPacket(const CDvHeaderPacket &, const CDvFramePacket &, const CDvFramePacket &, const CDvFramePacket &, uint8, CBuffer *) const;
	void EncodeDvLastPacket(const CDvHeaderPacket &, uint8, CBuffer *) const;

	// dmr DstId to Module helper
	char DmrDstIdToModule(uint32) const;
	uint32 ModuleToDmrDestId(char) const;

	// Buffer & LC helpers
	void AppendVoiceLCToBuffer(CBuffer *, uint32) const;
	void AppendTerminatorLCToBuffer(CBuffer *, uint32) const;
	void ReplaceEMBInBuffer(CBuffer *, uint8) const;
	void AppendDmrIdToBuffer(CBuffer *, uint32) const;
	void AppendDmrRptrIdToBuffer(CBuffer *, uint32) const;


protected:
	// for keep alive
	CTimePoint          m_LastKeepaliveTime;

	// for stream id
	uint16              m_uiStreamId;

	// for queue header caches
	std::array<CDmrmmdvmStreamCacheItem, NB_OF_MODULES>    m_StreamsCache;

	// for authentication
	uint32              m_uiAuthSeed;
};

////////////////////////////////////////////////////////////////////////////////////////

#endif /* cdmrmmdvmprotocol_h */
