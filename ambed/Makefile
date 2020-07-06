#copyright(C) 2020 by Thomas A. Early, N7TAE

# If you are going to change this path, you will
# need to update the systemd service script
BINDIR=/usr/local/bin

GCC=g++

# uncomment the next line to enable gdb debugging help
#DEBUG=true

CFLAGS=-W -MMD -MD -std=c++11
ifdef DEBUG
CFLAGS+=-ggdb3
endif

LDFLAGS=-pthread

SRCS=cagc.cpp cbuffer.cpp cfirfilter.cpp cip.cpp csignalprocessor.cpp cusb3003hrinterface.cpp  cvocodecchannel.cpp cvoicepacket.cpp cambepacket.cpp  ccallsign.cpp cfixedgain.cpp cpacket.cpp cstream.cpp cusb3000interface.cpp cusb3003interface.cpp cvocodecinterface.cpp main.cpp cambeserver.cpp ccontroller.cpp cftdidevicedescr.cpp cpacketqueue.cpp ctimepoint.cpp cusb3003df2etinterface.cpp cusb3xxxinterface.cpp cvocodecs.cpp cudpsocket.cpp

OBJS=$(SRCS:.cpp=.o)
DEPS=$(SRCS:.cpp=.d)
EXE=ambed

$(EXE) : $(OBJS)
	$(GCC) $(LDFLAGS) $(OBJS) -lftd2xx -Wl,-rpath,/usr/local/lib -o $@

%.o : %.cpp
	$(GCC) $(CFLAGS) -c $< -o $@

clean :
	$(RM) $(EXE) *.o *.d

-include $(DEPS)

# The install and uninstall targets need to be run by root
install : $(EXE) $(EXE).service
	cp $(EXE) $(BINDIR)
	cp $(EXE).service /etc/systemd/system/
	systemctl enable ambed
	systemctl daemon-reload
	systemctl start ambed

uninstall :
	systemctl stop ambed
	systemctl disable ambed
	systemctl daemon-reload
	rm -f /etc/systemd/system/ambed.service
	rm -f $(BINDIR)/ambed
	systemctl daemon-reload
