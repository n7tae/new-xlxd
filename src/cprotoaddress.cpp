//
//  Copyright © 2020 Thomas A. Eary, N7TAE
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

#include "cprotoaddress.h"

CProtoAddress::CProtoAddress()
{
#ifdef LISTEN_IPV4
	v4address[PROTOCOL_ANY]    = LISTEN_IPV4;
#endif
#ifdef LISTEN_V4_DPLUS
	v4address[PROTOCOL_DPLUS]  = LISTEN_V4_DPLUS;
#endif
#ifdef LISTEN_V4_DCS
	v4address[PROTOCOL_DCS]    = LISTEN_V4_DCS;
#endif
#ifdef LISTEN_V4_DEXTRA
	v4address[PROTOCOL_DEXTRA] = LISTEN_V4_DEXTRA;
#endif
#ifdef LISTEN_V4_DMRMMDVM
	v4address[PROTOCOL_DMRMMDVM] = LISTEN_V4_DMRMMDVM;
#endif
#ifdef LISTEN_V4_DMRPLUS
	v4address[PROTOCOL_DMRPLUS]  = LISTEN_V4_DMRPLUS;
#endif
#ifdef LISTEN_V4_YSF
	v4address[PROTOCOL_YSF]      = LISTEN_V4_YSF;
#endif
#ifdef LISTEN_V4_XLX
	v4address[PROTOCOL_XLX]      = LISTEN_V4_XLX;
#endif
#ifdef LISTEN_V4_G3
	v4address[PROTOCOL_G3]       = LISTEN_V4_G3;
#endif

#ifdef LISTEN_IPV6
	v6address[PROTOCOL_ANY]    = LISTEN_IPV6;
#endif
#ifdef LISTEN_V6_DPLUS
	v6address[PROTOCOL_DPLUS]  = LISTEN_V6_DPLUS;
#endif
#ifdef LISTEN_V6_DCS
	v6address[PROTOCOL_DCS]    = LISTEN_V6_DCS;
#endif
#ifdef LISTEN_V6_DEXTRA
	v6address[PROTOCOL_DEXTRA] = LISTEN_V6_DEXTRA;
#endif
#ifdef LISTEN_V6_DMRMMDVM
	v6address[PROTOCOL_DMRMMDVM] = LISTEN_V6_DMRMMDVM;
#endif
#ifdef LISTEN_V6_DMRPLUS
	v6address[PROTOCOL_DMRPLUS]  = LISTEN_V6_DMRPLUS;
#endif
#ifdef LISTEN_V6_YSF
	v6address[PROTOCOL_YSF]      = LISTEN_V6_YSF;
#endif
#ifdef LISTEN_V6_XLX
	v6address[PROTOCOL_XLX]      = LISTEN_V6_XLX;
#endif
#ifdef LISTEN_V6_G3
	v6address[PROTOCOL_G3]       = LISTEN_V6_G3;
#endif
}

#ifdef LISTEN_IPV4
std::string CProtoAddress::GetV4Address(int protocol)
{
	if (v4address.end() == v4address.find(protocol))
		return v4address[PROTOCOL_ANY];
	else
		return v4address[protocol];
}
#endif

#ifdef LISTEN_IPV6
std::string CProtoAddress::GetV6Address(int protocol)
{
	if (v6address.end() == v6address.find(protocol))
		return v6address[PROTOCOL_ANY];
	else
		return v6address[protocol];
}
#endif
