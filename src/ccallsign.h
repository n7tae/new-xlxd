//
//  ccallsign.h
//  xlxd
//
//  Created by Jean-Luc Deltombe (LX3JL) on 31/10/2015.
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

#ifndef ccallsign_h
#define ccallsign_h

////////////////////////////////////////////////////////////////////////////////////////
// define

#define CALLSIGN_LEN        8
#define CALLSUFFIX_LEN      4


////////////////////////////////////////////////////////////////////////////////////////
// class

class CCallsign
{
public:
	// contructors
	CCallsign();
	CCallsign(const char *, uint32 = 0);

	// status
	bool IsValid(void) const;
	bool HasSuffix(void) const;
	bool HasModule(void) const                { return m_Module != ' '; }

	// set
	void SetCallsign(const char *, bool = true);
	void SetCallsign(const uint8 *, int, bool = true);
#ifndef NO_XLX
	void SetDmrid(uint32, bool = true);
	void SetDmrid(const uint8 *, bool = true);
#endif
	void SetModule(char);
	void SetSuffix(const char *);
	void SetSuffix(const uint8 *, int);

	// modify
	void PatchCallsign(int, const uint8 *, int);

	// get
	void GetCallsign(uint8 *) const;
	void GetCallsignString(char *) const;
#ifndef NO_XLX
	uint32 GetDmrid(void) const             { return m_uiDmrid; }
#endif
	void GetSuffix(uint8 *) const;
	char GetModule(void) const              { return m_Module; }

	// compare
	bool HasSameCallsign(const CCallsign &) const;
	bool HasSameCallsignWithWildcard(const CCallsign &) const;
	bool HasLowerCallsign(const CCallsign &) const;
	bool HasSameModule(const CCallsign &) const;

	// operators
	bool operator ==(const CCallsign &) const;
	operator const char *() const;

protected:
	// helper
	bool IsNumber(char) const;
	bool IsLetter(char) const;
	bool IsSpace(char) const;

protected:
	// data
	char    m_Callsign[CALLSIGN_LEN];
	char    m_Suffix[CALLSUFFIX_LEN];
	char    m_Module;
	mutable char	m_sz[CALLSIGN_LEN+CALLSUFFIX_LEN+5];
#ifndef NO_XLX
	uint32  m_uiDmrid;
#endif
};

////////////////////////////////////////////////////////////////////////////////////////
#endif /* ccallsign_h */
