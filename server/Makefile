export CFLAGS:=-Wall

all:	../bin/deros_server

../bin/deros_server:	deros_server.c ../common/deros_net.c ../common/deros_addrs.c ../common/deros_dbglog.c
	gcc -o ../bin/deros_server -Wall $(^) -Wall -g

clean:
	rm -f ../bin/deros_server
