all: ../../bin/A_test_deros ../../bin/B_test_deros

../../bin/A_test_deros: A_test_deros.c ../../common/deros_net.c ../../node/deros_core.c ../../common/deros_addrs.c ../../common/deros_msglog.c ../../common/deros_dbglog.c ../../node/deros_subscriber.c ../../node/deros_publisher.c
	gcc -o ../../bin/A_test_deros $(^) -pthread -Wall -g

../../bin/B_test_deros: B_test_deros.c ../../common/deros_net.c ../../node/deros_core.c ../../common/deros_addrs.c ../../common/deros_msglog.c ../../common/deros_dbglog.c ../../node/deros_subscriber.c ../../node/deros_publisher.c
	gcc -o ../../bin/B_test_deros $(^) -pthread -Wall -g

clean:
	rm -f ../../bin/A_test_deros ../../bin/B_test_deros
