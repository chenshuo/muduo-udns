#MUDUO_DIRECTORY ?= $(HOME)/build/debug-install
MUDUO_DIRECTORY ?= $(HOME)/build/release-install
MUDUO_INCLUDE = $(MUDUO_DIRECTORY)/include
MUDUO_LIBRARY = $(MUDUO_DIRECTORY)/lib
SRC = .

UDNSDIR = $(SRC)/udns-0.2
UDNSSRCS = udns_dn.c udns_dntosp.c udns_parse.c udns_resolver.c udns_init.c \
	   udns_misc.c udns_XtoX.c \
	   udns_rr_a.c udns_rr_ptr.c udns_rr_mx.c udns_rr_txt.c udns_bl.c \
	   udns_rr_srv.c udns_rr_naptr.c udns_codes.c udns_jran.c
UDNSHEADERS = config.h udns.h

BUILDDIR = build

CXXFLAGS = -g -O2 -Wall -Wextra -Werror \
	   -Wconversion -Wno-unused-parameter \
	   -Wold-style-cast -Woverloaded-virtual \
	   -Wpointer-arith -Wshadow -Wwrite-strings \
	   -march=native -rdynamic \
	   -I$(MUDUO_INCLUDE) -I$(UDNSDIR)

CFLAGS = -Wall -W -O2 -pipe -g

LDFLAGS = -L$(MUDUO_LIBRARY) -L$(UDNSDIR) -lmuduo_net -lmuduo_base -lpthread -ludns

all: libudns udns

clean:
	rm -f core $(UDNSDIR)/libudns.a
	cd $(UDNSDIR) && make clean

libudns: $(UDNSDIR)/libudns.a

$(UDNSDIR)/libudns.a:
	cd $(UDNSDIR) && CFLAGS='$(CFLAGS)' ./configure --disable-ipv6 && make

Resolver.o: Resolver.cc Resolver.h
	g++ $(CXXFLAGS) -c $<

udns: dns.cc Resolver.o $(UDNSDIR)/libudns.a
	g++ $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

.PHONY: all clean libudns
