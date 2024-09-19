all:
	make -C server
	make -C examples

clean:
	make -C server clean
	make -C examples clean
