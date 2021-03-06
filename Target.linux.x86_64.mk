###########################################################################
#
# Copyright (c) 2001, 2002 Swedish Institute of Computer Science.
# All rights reserved. 
# Copyright (c) 2013-2017, NEC Europe Ltd., NEC Corporation.
# All rights reserved. 
#
# Redistribution and use in source and binary forms, with or without modification, 
# are permitted provided that the following conditions are met:
#
# 1. Redistributions of source code must retain the above copyright notice,
#    this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright notice,
#    this list of conditions and the following disclaimer in the documentation
#    and/or other materials provided with the distribution.
# 3. The name of the author may not be used to endorse or promote products
#    derived from this software without specific prior written permission. 
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED 
# WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF 
# MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT 
# SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, 
# EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT 
# OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS 
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN 
# CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING 
# IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY 
# OF SUCH DAMAGE.
#
# This file is part of the lwIP TCP/IP stack.
# 
# Author: Adam Dunkels <adam@sics.se>
#         Simon Kuenzer <simon.kuenzer@neclab.eu>
#
###########################################################################

ifndef LWIP_ROOT
$(error "Please define LWIP_ROOT")
endif

ifeq ($(CONFIG_OSVAPP),y)
ifndef OSV_ROOT
$(error "Please define OSV_ROOT")
endif

BUILDSO=y
CONFIG_OSVNET=y
CONFIG_OSVBLK=y
CONFIG_PTH_THREADS=n
CINCLUDES+=-I$(OSV_ROOT)/arch/x64 -I$(OSV_ROOT) -I$(OSV_ROOT)/include
CINCLUDES+=-isystem $(OSV_ROOT)/include/glibc-compat
glibcbase     = $(OSV_ROOT)/external/x64/glibc.bin
gccbase       = $(OSV_ROOT)/external/x64/gcc.bin
miscbase      = $(OSV_ROOT)/external/x64/misc.bin
gcc-inc-base  = $(dir $(shell find $(gccbase)/ -name vector | grep -v -e debug/vector$$ -e profile/vector$$))
gcc-inc-base2 = $(dir $(shell find $(gccbase)/ -name unwind.h))
gcc-inc-base3 = $(dir $(shell dirname `find $(gccbase)/ -name c++config.h | grep -v /32/`))
CINCLUDES+=-isystem $(gcc-inc-base) \
           -isystem $(gcc-inc-base3)
CINCLUDES+=-isystem $(OSV_ROOT)/external/x64/acpica/source/include \
           -isystem $(OSV_ROOT)/external/x64/misc.bin/usr/include \
           -isystem $(OSV_ROOT)/include/api \
           -isystem $(OSV_ROOT)/include/api/x64
# must be after include/api, since it includes some libc-style headers:
CINCLUDES+=-isystem $(gcc-inc-base2) \
           -isystem gen/include \
           $(post-includes-bsd)
OSV_BUILD_MODE=${mode}
CINCLUDES+=-I$(OSV_ROOT)/build/$(OSV_BUILD_MODE)/gen/include/

post-includes-bsd += -isystem $(OSV_ROOT)/bsd/sys
# For acessing machine/ in cpp xen drivers
post-includes-bsd += -isystem $(OSV_ROOT)/bsd/
post-includes-bsd += -isystem $(OSV_ROOT)/bsd/x64
#autodepend = -MD -MT $@ -MP

common+=-nostdinc -D__BSD_VISIBLE=1 -D_KERNEL -g \
	-include $(OSV_ROOT)/compiler/include/intrinsics.hh -Wformat=0 \
	-Wno-format-security-DNDEBUG -DCONF_debug_memory=0 \
	-D__OSV__
ifndef CONFIG_BUILD_OPTIMISATION
common +=  -O3
else
common += $(CONFIG_BUILD_OPTIMISATION)
endif

CFLAGS+=$(common)
CXXFLAGS+=-std=gnu++11 $(common)
LDFLAGS+=$(autodepend)
else
CONFIG_PTH_THREADS?=n
CONFIG_SHELL?=n
CONFIG_NETMAP?=y

CONFIG_SHFS_CACHE_READAHEAD		?= 8
CONFIG_SHFS_CACHE_POOL_NB_BUFFERS	?= 8192
CONFIG_SHFS_CACHE_GROW			= n
endif

ifeq ($(CONFIG_SHELL),y)
ifneq ($(CONFIG_PTH_THREADS),y)
$(warning "Shell is not available without threads support")
CONFIG_SHELL:=n
endif
endif

ifeq ($(CONFIG_NETMAP),y)
ifndef NETMAP_INCLUDES
$(error "Please define NETMAP_INCLUDES")
endif
endif

###########################################################################

CONFIG_CTLDIR = n # ctldir is not supported on linuxapp
CONFIG_SHFS_STATS = n # no stats
CONFIG_TESTSUITE = n # no testuite

CONFIG_MINICACHE_MINDER_PRINT ?= n

CONFIG_LWIP_CHECKSUM_NOCHECK ?= y
CONFIG_LWIP_CHECKSUM_NOGEN ?= n

ifeq ($(CONFIG_LWIP_NUM_TCPCON),)
CONFIG_LWIP_NUM_TCPCON=512
endif
CFLAGS+= -DCONFIG_LWIP_NUM_TCPCON=$(CONFIG_LWIP_NUM_TCPCON)
CFLAGS-$(CONFIG_LWIP_CHECKSUM_NOCHECK)+=-DCONFIG_LWIP_CHECKSUM_NOCHECK
CFLAGS-$(CONFIG_LWIP_CHECKSUM_NOGEN)+=-DCONFIG_LWIP_CHECKSUM_NOGEN
CFLAGS-$(CONFIG_DEBUG_LWIP)+=-DCONFIG_DEBUG_LWIP
CFLAGS-$(CONFIG_DEBUG_LWIP_MAINLOOP)+=-DLWIP_MAINLOOP_DEBUG
CFLAGS-$(CONFIG_DEBUG_LWIP_IF)+=-DLWIP_IF_DEBUG
CFLAGS-$(CONFIG_DEBUG_LWIP_IP)+=-DLWIP_IP_DEBUG
CFLAGS-$(CONFIG_DEBUG_LWIP_UDP)+=-DLWIP_UDP_DEBUG
CFLAGS-$(CONFIG_DEBUG_LWIP_TCP)+=-DLWIP_TCP_DEBUG
CFLAGS-$(CONFIG_DEBUG_LWIP_SYS)+=-DLWIP_SYS_DEBUG
CFLAGS-$(CONFIG_DEBUG_LWIP_API)+=-DLWIP_API_DEBUG
CFLAGS-$(CONFIG_DEBUG_LWIP_SERVICE)+=-DLWIP_SERVICE_DEBUG
CFLAGS+= -DCONFIG_LWIP_NOTHREADS
CFLAGS+= -DHAVE_LWIP

include Minicache.mk

#################################

LN		 = ln -sf
MKDIR		 = mkdir -p
MV		 = mv -f
RM		 = rm -f
RMDIR		 = rm -rf
TOUCH		 = touch

CCDEP=gcc
CC=gcc
CXX=g++
LD=$(CC)

CFLAGS+=-g -D$(TARGET) $(MCCFLAGS) \
	-Wparentheses -Wsequence-point -Wswitch-default \
	-Wundef -Wpointer-arith -Wbad-function-cast \
	-Wwrite-strings -Wold-style-definition \
	-Wmissing-prototypes -Wredundant-decls -Wno-address
# -Wextra -Wnested-externs -Wall -Wshadow
# -pedantic \

# not used for now but interesting:
# -Wpacked
# -Wunreachable-code
# -ansi
# -std=c89
LDFLAGS+=-pthread #-lutil
ARFLAGS=rs

ifeq ($(CONFIG_PTH_THREADS),y)
LDFLAGS+=-lpth
CFLAGS+=-DCONFIG_PTH_THREADS
endif

ifeq ($(BUILDSO),y)
CFLAGS+=-fPIC -DLWIP_DNS_API_DECLARE_H_ERRNO=0
CXXFLAGS+=-fPIC -DLWIP_DNS_API_DECLARE_H_ERRNO=0
endif

CONTRIBDIR		 = ../lwip-contrib
LWIPDIR		?= $(LWIP_ROOT)/src
MINICACHE_ROOT		?= $(realpath .)
LWIPARCH		 = $(MINICACHE_ROOT)/target/$(TARGET)
BUILDDIR		?= $(MINICACHE_ROOT)/build
ifneq ($(BUILDSO),y)
MINICACHE_OUT		?= minicache_$(ARCH)
else
MINICACHE_OUT		?= minicache_$(ARCH).so
endif

CINCLUDES+=-I. \
	-Itarget/$(TARGET)/include \
	-I$(LWIPDIR)/include \
	-I$(LWIPARCH) \
	-I$(LWIPARCH)/include \
	-I$(LWIPDIR)
CFLAGS+=$(CINCLUDES)
CXXFLAGS+=$(CINCLUDES)

# COREFILES, CORE4FILES: The minimum set of files needed for lwIP.
COREDIRS=$(LWIPDIR)/core
COREFILES=$(LWIPDIR)/core/def.c $(LWIPDIR)/core/dhcp.c $(LWIPDIR)/core/dns.c \
	$(LWIPDIR)/core/inet_chksum.c $(LWIPDIR)/core/init.c $(LWIPDIR)/core/mem.c \
	$(LWIPDIR)/core/memp.c $(LWIPDIR)/core/netif.c $(LWIPDIR)/core/pbuf.c \
	$(LWIPDIR)/core/raw.c $(LWIPDIR)/core/stats.c $(LWIPDIR)/core/sys.c \
	$(LWIPDIR)/core/tcp.c $(LWIPDIR)/core/tcp_in.c $(LWIPDIR)/core/tcp_in.c \
	$(LWIPDIR)/core/tcp_out.c $(LWIPDIR)/core/timers.c $(LWIPDIR)/core/udp.c
CORE4DIRS=$(LWIPDIR)/core/ipv4
CORE4FILES=$(LWIPDIR)/core/ipv4/autoip.c $(LWIPDIR)/core/ipv4/icmp.c \
	$(LWIPDIR)/core/ipv4/igmp.c $(LWIPDIR)/core/ipv4/ip_frag.c \
	$(LWIPDIR)/core/ipv4/ip4.c $(LWIPDIR)/core/ipv4/ip4_addr.c
CORE6DIRS=$(LWIPDIR)/core/ipv6
CORE6FILES=$(LWIPDIR)/core/ipv6/dhcp6.c $(LWIPDIR)/core/ipv6/ethip6.c \
	$(LWIPDIR)/core/ipv6/icmp6.c $(LWIPDIR)/core/ipv6/ip6.c \
	$(LWIPDIR)/core/ipv6/ip6_addr.c $(LWIPDIR)/core/ipv6/ip6_frag.c \
	$(LWIPDIR)/core/ipv6/mld6.c $(LWIPDIR)/core/ipv6/nd6.c

# APIFILES: The files which implement the sequential and socket APIs.
APIDIRS=$(LWIPDIR)/api
APIFILES=$(LWIPDIR)/api/api_lib.c $(LWIPDIR)/api/api_msg.c $(LWIPDIR)/api/err.c \
	$(LWIPDIR)/api/netbuf.c $(LWIPDIR)/api/netdb.c $(LWIPDIR)/api/netifapi.c \
	$(LWIPDIR)/api/sockets.c $(LWIPDIR)/api/tcpip.c

# NETIFFILES: Files implementing various generic network interface functions.'
NETIFDIRS=$(LWIPDIR)/netif
NETIFFILES=$(LWIPDIR)/netif/etharp.c

# NETIFFILES: Add SLIP netif
NETIFFILES+=$(LWIPDIR)/netif/slipif.c

# SNMPFILES: Extra SNMPv1 agent
SNMPFILES=$(LWIPDIR)/core/snmp/asn1_dec.c $(LWIPDIR)/core/snmp/asn1_enc.c \
	$(LWIPDIR)/core/snmp/mib2.c $(LWIPDIR)/core/snmp/mib_structs.c \
	$(LWIPDIR)/core/snmp/msg_in.c $(LWIPDIR)/core/snmp/msg_out.c \
	$(LWIPARCH)/lwip_prvmib.c
SNMPDIRS=$(LWIPDIR)/core/snmp

ARCHDIRS=$(LWIPARCH)/netif
#ARCHFILES=$(wildcard $(LWIPARCH)/*.c) # $(LWIPARCH)/netif/sio.c $(LWIPARCH)/netif/fifo.c)
# lwIP device driver
ifeq ($(CONFIG_PCAPIF),y)
ARCHFILES+=$(wildcard $(LWIPARCH)/netif/pcapif.c)
CFLAGS+=-DCONFIG_PCAPIF
LDFLAGS+=-lpcap
else 
ifeq ($(CONFIG_OSVNET),y)
ARCHFILES+=$(wildcard $(LWIPARCH)/netif/osv-net.c)
ARCHFILESXX+=$(wildcard $(LWIPARCH)/netif/osv-net-io.cc)
CFLAGS+=-DCONFIG_OSVNET
else
ifeq ($(CONFIG_NETMAP),y)
ARCHFILES+=$(wildcard $(LWIPARCH)/netif/netmapif.c)
CFLAGS+=-DCONFIG_NETMAP -I$(NETMAP_INCLUDES)
else
ARCHFILES+=$(wildcard $(LWIPARCH)/netif/tapif.c)
CFLAGS+=-DCONFIG_TAPIF
endif
endif
endif

APPDIRS=target/$(TARGET)/blkdev
ifeq ($(CONFIG_OSVBLK),y)
APPFILES+=target/$(TARGET)/blkdev/osv-blk.c
APPFILESXX+=target/$(TARGET)/blkdev/osv-blk-bio.cc
CFLAGS+=-DCONFIG_OSVBLK
else
APPFILES+=target/$(TARGET)/blkdev/paio-blk.c
LDFLAGS+=-lrt
endif

# APPFILES: Applications.
APPDIRS+=:.:target/$(TARGET)
APPFILES+=$(MCOBJS)
APPFILESXX+=$(MCOBJSXX)
APPFILESW  =$(addprefix $(BUILDDIR)/,$(notdir $(APPFILES)))
APPFILESWXX=$(addprefix $(BUILDDIR)/,$(notdir $(APPFILESXX)))
APPOBJS =$(addprefix $(BUILDDIR)/,$(notdir $(APPFILESW:.c=.o)))
APPOBJS+=$(addprefix $(BUILDDIR)/,$(notdir $(APPFILESWXX:.cc=.o)))

# LWIPFILES: All the above.
LWIPFILES  =$(COREFILES) $(CORE4FILES) $(CORE6FILES) $(SNMPFILES) $(APIFILES) $(NETIFFILES) $(ARCHFILES) target/$(TARGET)/sys_arch.c
LWIPFILESXX=$(COREFILESXX) $(CORE4FILESXX) $(CORE6FILESXX) $(SNMPFILESXX) $(APIFILESXX) $(NETIFFILESXX) $(ARCHFILESXX)
LWIPFILESW=$(wildcard $(LWIPFILES))
LWIPFILESWXX=$(wildcard $(LWIPFILESXX))
LWIPOBJS =$(addprefix $(BUILDDIR)/,$(notdir $(LWIPFILES:.c=.o)))
LWIPOBJS+=$(addprefix $(BUILDDIR)/,$(notdir $(LWIPFILESXX:.cc=.o)))

ifneq ($(BUILDSO),y)
LWIPLIB=$(BUILDDIR)/liblwip.a
APPLIB=$(BUILDDIR)/minicache.a
else
LWIPLIB=$(BUILDDIR)/liblwip.so
endif

CFLAGS+=$(CFLAGS-y)

# set source search path
VPATH=$(BUILDDIR):$(LWIPARCH):$(COREDIRS):$(CORE4DIRS):$(CORE6DIRS):$(SNMPDIRS):$(APIDIRS):$(NETIFDIRS):$(ARCHDIRS):$(APPDIRS)

include $(BUILDDIR)/.depend

.PHONY: clean
clean:
	$(RM) $(BUILDDIR)/*.o $(BUILDDIR)/$(LWIPLIB) $(BUILDDIR)/$(APPLIB) $(BUILDDIR)/$(MINICACHE_OUT) $(BUILDDIR)/*.s $(BUILDDIR)/.depend* $(BUILDDIR)/*.core $(BUILDDIR)/core

.PHONY: distclean
distclean:
	$(RMDIR) $(BUILDDIR)

.PHONY: all
all: $(BUILDDIR)/$(MINICACHE_OUT)

.PHONY: build
build: $(BUILDDIR) $(BUILDDIR)/.depend $(BUILDDIR)/$(MINICACHE_OUT)

$(BUILDDIR):
	$(MKDIR) $@

$(BUILDDIR)/.depend: $(BUILDDIR) $(LWIPFILES) $(APPFILES) | $(BUILDDIR)
	$(CCDEP) $(CFLAGS) -MM $^ > $(BUILDDIR)/.depend || $(RM) $(BUILDDIR)/.depend

$(BUILDDIR)/%.o: %.cc | $(BUILDDIR)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

$(BUILDDIR)/%.o: %.c | $(BUILDDIR)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILDDIR)/%.a: %.o
	$(AR) $(ARFLAGS) -o $@ $^

.PHONY: $(BUILDDIR)/$(MINICACHE_OUT)
ifneq ($(BUILDSO),y)
$(LWIPLIB): $(LWIPOBJS)
	$(AR) $(ARFLAGS) $(LWIPLIB) $?

$(APPLIB): $(APPOBJS)
	$(AR) $(ARFLAGS) $(APPLIB) $?

$(BUILDDIR)/$(MINICACHE_OUT): $(BUILDDIR)/.depend $(LWIPLIB) $(APPLIB) $(APPFILES)
	$(LD) $(APPLIB) $(LWIPLIB) $(LDFLAGS) -o $(BUILDDIR)/$(MINICACHE_OUT)
else
$(LWIPLIB): $(LWIPOBJS)
	$(LD) $(LDFLAGS) $? -shared -o $(LWIPLIB)

$(BUILDDIR)/$(MINICACHE_OUT): $(BUILDDIR)/.depend $(LWIPLIB) $(APPOBJS) $(APPFILES)
	$(LD) $(APPOBJS) $(LDFLAGS) -l:$(LWIPLIB) -shared -o $(BUILDDIR)/$(MINICACHE_OUT)
endif
