# Copyright (c) 2020 by Thomas A. Early N7TAE
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

# locations for the executibles and other files are set here
# NOTE: IF YOU CHANGE THESE, YOU WILL NEED TO UPDATE THE service.* FILES AND
# if you change these locations, make sure the sgs.service file is updated!
# you will also break hard coded paths in the dashboard file, index.php.

# if you make changed in these two variable, you'll need to change things
# in the main.h file as well as the systemd service file.
BINDIR=/usr/local/bin
CFGDIR=/usr/local/etc
DATADIR=/var/lib/xlxd

################## This is the part you can safely edit #######################
# uncomment the next line for debugging support
#DEBUG=true
# uncomment the next line for a DStar only xrf reflector (instead of xlx)
#NOXLX=true
# uncomment the next line for no G3 support
#NOG3=true
###############################################################################

CC=g++

ifdef DEBUG
CFLAGS=-ggdb3 -c -std=c++11 -MMD -MD -c
else
CFLAGS=-c -std=c++11 -MMD -MD -c
endif
ifdef NOXLX
CFLAGS += -DNO_XLX
endif
ifdef NOG3
CFLAGS += -DNO_G3
endif

LDFLAGS=-pthread

XRFSRCS = cbuffer.cpp ccallsign.cpp ccallsignlist.cpp ccallsignlistitem.cpp cclient.cpp cclients.cpp cdcsclient.cpp cdcsprotocol.cpp cdextraclient.cpp cdextrapeer.cpp cdextraprotocol.cpp cdplusclient.cpp cdplusprotocol.cpp cdvframepacket.cpp cdvheaderpacket.cpp cdvlastframepacket.cpp cgatekeeper.cpp cip.cpp cnotification.cpp cpacket.cpp cpacketstream.cpp cpeercallsignlist.cpp cpeer.cpp cpeers.cpp cprotocol.cpp cprotocols.cpp creflector.cpp ctimepoint.cpp cudpsocket.cpp cuser.cpp cusers.cpp cversion.cpp main.cpp
XLXSRCS = cbmclient.cpp cbmpeer.cpp cbptc19696.cpp ccodecstream.cpp ccrc.cpp cdmriddir.cpp cdmriddirfile.cpp cdmriddirhttp.cpp cdmrmmdvmclient.cpp cdmrmmdvmprotocol.cpp cdmrplusclient.cpp cdmrplusprotocol.cpp cgolay2087.cpp cgolay24128.cpp chamming.cpp cqr1676.cpp crs129.cpp csemaphore.cpp ctranscoder.cpp cutils.cpp cwiresxcmd.cpp cwiresxcmdhandler.cpp cwiresxinfo.cpp cxlxclient.cpp cxlxprotocol.cpp cxlxpeer.cpp cysfclient.cpp cysfconvolution.cpp cysffich.cpp cysfnode.cpp cysfnodedir.cpp cysfnodedirfile.cpp cysfnodedirhttp.cpp cysfpayload.cpp cysfprotocol.cpp cysfutils.cpp
G3SRCS = cg3client.cpp cg3protocol.cpp crawsocket.cpp cudpmsgsocket.cpp

SRCS = $(XRFSRCS)
ifndef NOXLX
SRCS += $(XLXSRCS)
endif
ifndef NOG3
SRCS += $(G3SRCS)
endif

OBJS = $(SRCS:.cpp=.o)
DEPS = $(SRCS:.cpp=.d)

ifdef NOXLX
EXE=xrfd
else
EXE=xlxd
endif

all : $(EXE)

$(EXE) : $(OBJS)
	$(CC) $^ -o $@ $(LDFLAGS)

%.o : %.cpp
	g++ $(CFLAGS) $< -o $@

clean :
	$(RM) *.o *.d xlxd xrfd

-include $(DEPS)

install : $(EXE).blacklist $(EXE).whitelist $(EXE).interlink $(EXE).terminal $(EXE).service $(EXE)
	ln -s $(shell pwd)/$(EXE).blacklist $(CFGDIR)/$(EXE).blacklist
	ln -s $(shell pwd)/$(EXE).whitelist $(CFGDIR)/$(EXE).whitelist
	ln -s $(shell pwd)/$(EXE).interlink $(CFGDIR)/$(EXE).interlink
ifndef NOG3
	ln -s $(shell pwd)/$(EXE).terminal  $(CFGDIR)/$(EXE).terminal
endif
	cp -f $(EXE).service /etc/systemd/system/
	cp -f $(EXE) $(BINDIR)
	mkdir -p $(DATADIR)
	systemctl enable $(EXE).service
	systemctl daemon-reload
	systemctl start $(EXE)

uninstall :
	rm -f $(CFGDIR)/$(EXE).blacklist
	rm -f $(CFGDIR)/$(EXE).whitelist
	rm -f $(CFGDIR)/$(EXE).interlink
	rm -f $(CFGDIR)/$(EXE).terminal
	systemctl stop $(EXE).service
	rm -f $(CFGDIR)/dmrid.dat
	systemctl disable $(EXE).service
	rm -f /etc/systemd/system/xlxd.service
	systemctl daemon-reload
