# -------------------------------------------------------------------------------------------------------------------- #
# FreeBSD-like sockstat for macOS using libproc                                                                        #
# -------------------------------------------------------------------------------------------------------------------- #

# Copyright (c) 2023, Mikhail Zakharov <zmey20000@yahoo.com>
#
# Redistribution and use in source and binary forms, with or without modification, are permitted provided that the
# following conditions are met:
#
# 1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following
#    disclaimer.
#
# 2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following
#    disclaimer in the documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,
# INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
# WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

# -------------------------------------------------------------------------------------------------------------------- #
PREFIX ?= /usr/local

CC = cc
CFLAGS += -O3 -Wall -DPREFIX='"$(PREFIX)"'
OBJS = sockstat.o

.PHONY:	all clean install uninstall

all: sockstat


sockstat: $(OBJS)
	$(CC) -o $@ $(OBJS)

install: sockstat
	install -d $(PREFIX)/bin/
	install -m 755 -s sockstat $(PREFIX)/bin/

uninstall:
	rm -f $(PREFIX)/bin/sockstat

clean:
	rm -rf  sockstat *.o *.dSYM *.core

# sockstat.o: sockstat.h
