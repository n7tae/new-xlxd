#pragma once

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

#include <unordered_map>
#include <string>

#include "main.h"

class CProtoAddress
{
public:
	CProtoAddress();
#ifdef LISTEN_IPV4
	std::string GetV4Address(int protocol);
#endif
#ifdef LISTEN_IPV6
	std::string GetV6Address(int protocol);
#endif

private:
#ifdef LISTEN_IPV4
	std::unordered_map<int, std::string> v4address;
#endif
#ifdef LISTEN_IPV6
	std::unordered_map<int, std::string> v6address;
#endif
};
