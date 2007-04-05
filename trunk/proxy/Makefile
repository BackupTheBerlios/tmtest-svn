# proxy Makefile
# Scott Bronson
# 8 Mar 2007


VERSION=0.01

COPTS=-g -Wall -Werror -DUSE_MOCK -DUSE_EPOLL

# files with unit tests
USRC=mempool.c minidst-minibuf.c
UHDR=mempool.h minidst-minibuf.h
# files needed to run the unit tests
USRC+=unit-tests.c $(shell mutest/mutest-config -c assert --tests)
UHDR+=unit-tests.h $(shell mutest/mutest-config -h assert --tests)
USRC+=minibuf.c minio.c env.c
UHDR+=minibuf.h minio.h env.h list.h
# the IO Atom library
USRC+=io/atom.c io/poller.c io/socket.c io/pollers/select.c io/pollers/epoll.c io/pollers/mock.c
UHDR+=io/atom.h io/poller.h io/socket.h io/pollers/select.h io/pollers/epoll.h io/pollers/mock.h

# application files
CSRC=main.c log.c
CHDR=list.h log.h



all: proxy

proxy: $(CSRC) $(CHDR) Makefile
	$(CC) $(COPTS) $(CSRC) $(LIBS) -DVERSION=$(VERSION) -o proxy


# why compile a separate executable when you can just run the unit
# tests from proxy itself?  Because you'll likely want to run unit tests
# during development, even when proxy is a total uncompilable mess.
units: $(USRC) $(UHDR) mutest/main.c Makefile
	$(CC) $(COPTS) $(USRC) mutest/main.c -o units

run-units: units
	./units

clean:
	rm -f proxy units
	rm -rf docs

doc:
	doxygen
	# which pods2html > /dev/null || echo "You must install Pod::Tree off CPAN"
	# pods2html . docs/html
