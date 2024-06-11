/* ------------------------------------------------------------------------------------------------------------------ */
/* FreeBSD-like sockstat for macOS using libproc                                                                      */
/* ------------------------------------------------------------------------------------------------------------------ */

/*
* Copyright (c) 2023-2024, Mikhail Zakharov <zmey20000@yahoo.com>
*
* Redistribution and use in source and binary forms, with or without modification, are permitted provided that the
* following conditions are met:
*
* 1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following
*    disclaimer.
*
* 2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and
*    the following disclaimer in the documentation and/or other materials provided with the distribution.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,
* INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
* DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
* SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
* SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
* WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
* OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

/* ------------------------------------------------------------------------------------------------------------------ */
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <limits.h>

#include <sys/types.h>
#include <sys/sysctl.h>
#include <pwd.h>
#include <uuid/uuid.h>

#include <arpa/inet.h>

#include <libproc.h>

/* ------------------------------------------------------------------------------------------------------------------ */
void usage(int ecode);

/* ------------------------------------------------------------------------------------------------------------------ */
#define PROG_NAME       "sockstat"
#define PROG_VERSION    "1.0.2"

#define MAXPROC         16384;
/* ------------------------------------------------------------------------------------------------------------------ */
int main(int argc, char* argv[]) {
    int mproc = MAXPROC;                                                    /* Max number of concurrent processes */
    size_t mproc_len = sizeof(mproc);                                       /* Set it enormously big to tune later*/
    int *pids = NULL;                                                       /* PIDs array buffer */
    int npids = 0;                                                          /* Number of PIDs */
    static struct proc_fdinfo *fds = NULL;                                  /* FDs array */
    int mfds = 0, nfds = 0;                                                 /* Memory and number of FDS */
    struct proc_bsdinfo pinfo;
    struct socket_fdinfo si;
    struct passwd *pwd;
    char buf[INET6_ADDRSTRLEN] = {0};                                       /* Local/Remote IPv4/6 addresses bufs */
    char outstr[160] = {0};
    int ready = 0;                                                          /* Socket info collected - ready to print */
    int listen = 1;                                                         /* Socket is listening for flg_l output */

    int flg = 0;                                                            /* CLI flags, see below */
    int flg_i4 = 0;                                                         /* IPv4 */
    int flg_i6 = 0;                                                         /* IPv6 */
    int flg_k = 0;                                                          /* Kern/system sockets */
    int flg_l = 0;                                                          /* TCP LISTENing sockets */
    int flg_n = 0;                                                          /* NDRV sockets */
    int flg_q = 0;                                                          /* Quiet - hide header */
    int flg_r = 0;                                                          /* ROUTE sockets */
    int flg_u = 0;                                                          /* UNIX aka LOCAL sockets */
    int flg_a = 0;                                                          /* pseudo-flag ALL socket flags are on */

    while ((flg = getopt(argc, argv, "46klnrquhv")) != -1)
        switch(flg) {
            case '4': flg_i4 = 1; break;
            case '6': flg_i6 = 1; break;
            case 'k': flg_k = 1; break;
            case 'l': flg_l = 1; break;
            case 'n': flg_n = 1; break;
            case 'q': flg_q = 1; break;
            case 'r': flg_r = 1; break;
            case 'u': flg_u = 1; break;
            case 'h': (void)usage(0); break;
            case 'v': printf("%s %s\n", PROG_NAME, PROG_VERSION); exit(0); break;
            default: (void)usage(1);
        }

    if (!flg_i4 && !flg_i6 && !flg_k && !flg_n && !flg_r && !flg_u) flg_a = 1;

    if (sysctlbyname("kern.maxproc", &mproc, &mproc_len, NULL, 0) == -1) {
        perror("Unable to get the maximum allowed number of processes");
        printf("Assuming it to be lower than: %d\n", mproc);
    }

    pids = (int *)malloc(sizeof(int) * mproc);
    /* PIDs */
    /* NB! proc_listpids() returns bytes (!), not count of pids (!) */
    npids = proc_listpids(PROC_ALL_PIDS, 0, pids, sizeof(int) * mproc);

    fds = (struct proc_fdinfo *)malloc(sizeof(struct proc_fdinfo) * OPEN_MAX);

    if (!flg_q)
        printf("%-23s\t%-5s\t%-31s\t%-3s\t%-5s\t%-19s\t%s\n",
            "USER", "PID", "COMMAND", "FD", "PROTO", "LOCAL ADDRESS", "REMOTE ADDRESS");

    for (int i = 0; i < npids/sizeof(int); i++) {
        /* PID => FDs */
        if ((mfds = proc_pidinfo(pids[i], PROC_PIDLISTFDS, 0, fds, sizeof(struct proc_fdinfo) * OPEN_MAX))) {
            /* a PIDs => PID */
            proc_pidinfo(pids[i], PROC_PIDTBSDINFO, 0, &pinfo, sizeof(pinfo));

            nfds = (int)(mfds / sizeof(struct proc_fdinfo));
            for (int k = 0; k < nfds; k++) {
                if (proc_pidfdinfo(pids[i], fds[k].proc_fd, PROC_PIDFDSOCKETINFO, &si, sizeof(si)) >= sizeof(si)) {
                    pwd = getpwuid(pinfo.pbi_uid);

                    sprintf(outstr, "%-23s\t%-5d\t%-31s\t%-3d",
                        pwd->pw_name, pinfo.pbi_pid, pinfo.pbi_name[0] ? pinfo.pbi_name : pinfo.pbi_comm,
                        fds[k].proc_fd);

                    ready = 0;
                    listen = 0;

                    switch (si.psi.soi_family) {
                        case AF_INET:
                            if (flg_i4 || flg_a) {
                                if (si.psi.soi_kind == SOCKINFO_TCP) { /* IPv4 TCP */
                                    /* Local address and port */
                                    if (si.psi.soi_proto.pri_tcp.tcpsi_ini.insi_lport) {
                                        sprintf(outstr + strlen(outstr), "\ttcp4\t%s:%d",
                                            si.psi.soi_proto.pri_tcp.tcpsi_ini.insi_laddr.ina_46.i46a_addr4.s_addr ==
                                                INADDR_ANY ? "*" : inet_ntop(AF_INET,
                                                    &si.psi.soi_proto.pri_tcp.tcpsi_ini.insi_laddr.ina_46.i46a_addr4,
                                                    buf, INET_ADDRSTRLEN),
                                            ntohs(si.psi.soi_proto.pri_tcp.tcpsi_ini.insi_lport));
                                        if (si.psi.soi_proto.pri_tcp.tcpsi_state == TSI_S_LISTEN) listen = 1;
                                        ready = 1;
                                    }
                                    /* Remote address and port */
                                    if (si.psi.soi_proto.pri_tcp.tcpsi_ini.insi_fport) {
                                        sprintf(outstr + strlen(outstr), "\t%s:%d",
                                            inet_ntop(AF_INET,
                                                &si.psi.soi_proto.pri_tcp.tcpsi_ini.insi_faddr.ina_46.i46a_addr4,
                                                buf, INET_ADDRSTRLEN),
                                            ntohs(si.psi.soi_proto.pri_tcp.tcpsi_ini.insi_fport));
                                        ready = 1;
                                    } else {
                                        /* No port - no address */
                                        sprintf(outstr + strlen(outstr), "\t*:*");
                                        ready = 1;
                                    }
                                } else { /* IPv4 UDP */
                                    /* Local address and port */
                                    if (si.psi.soi_proto.pri_in.insi_lport) {
                                        sprintf(outstr + strlen(outstr), "\tudp4\t%s:%d",
                                            si.psi.soi_proto.pri_in.insi_laddr.ina_46.i46a_addr4.s_addr ==
                                                INADDR_ANY ? "*" : inet_ntop(AF_INET,
                                                    &si.psi.soi_proto.pri_in.insi_laddr.ina_46.i46a_addr4,
                                                    buf, INET_ADDRSTRLEN),
                                            ntohs(si.psi.soi_proto.pri_in.insi_lport));
                                    } else {
                                        sprintf(outstr+strlen(outstr), "\tudp4\t%s:*",
                                            si.psi.soi_proto.pri_in.insi_laddr.ina_46.i46a_addr4.s_addr ==
                                                INADDR_ANY ? "*" : inet_ntop(AF_INET,
                                                    &si.psi.soi_proto.pri_in.insi_laddr.ina_46.i46a_addr4,
                                                    buf, INET_ADDRSTRLEN));
                                    }
                                    /* Remote address and port */
                                    /* UDP is stateless protocol, marking it LISTEN */
                                    listen = 1;
                                    sprintf(outstr + strlen(outstr), "\t*:*");
                                    ready = 1;
                                }
                            }
                        break;

                        case AF_INET6:
                            if (flg_i6 || flg_a) {
                                if (si.psi.soi_kind == SOCKINFO_TCP) { /* IPv6 TCP */
                                    /* Local address and port */
                                    if (si.psi.soi_proto.pri_tcp.tcpsi_ini.insi_lport) {
                                        sprintf(outstr + strlen(outstr), "\ttcp6\t%s:%d",
                                            IN6_IS_ADDR_UNSPECIFIED(&si.psi.soi_proto.pri_tcp.tcpsi_ini.insi_laddr.ina_6) ?
                                                "*" : inet_ntop(AF_INET6,
                                                    &si.psi.soi_proto.pri_tcp.tcpsi_ini.insi_laddr.ina_6,
                                                    buf, INET6_ADDRSTRLEN),
                                            ntohs(si.psi.soi_proto.pri_tcp.tcpsi_ini.insi_lport));
                                            if (si.psi.soi_proto.pri_tcp.tcpsi_state == TSI_S_LISTEN) listen = 1;
                                        ready = 1;
                                    }
                                    /* Remote address and port */
                                    if (si.psi.soi_proto.pri_tcp.tcpsi_ini.insi_fport) {
                                        sprintf(outstr + strlen(outstr), "\t%s:%d",
                                            inet_ntop(AF_INET6,
                                                &si.psi.soi_proto.pri_tcp.tcpsi_ini.insi_faddr.ina_6,
                                                buf, INET6_ADDRSTRLEN),
                                            ntohs(si.psi.soi_proto.pri_tcp.tcpsi_ini.insi_fport));
                                            ready = 1;
                                    } else {
                                        /* No port - no address */
                                        sprintf(outstr + strlen(outstr), "\t*:*");
                                        ready = 1;
                                    }
                                } else { /* IPv6 UDP */
                                    listen = 1;
                                    /* Local address and port */
                                    if (si.psi.soi_proto.pri_in.insi_lport) {
                                        sprintf(outstr + strlen(outstr), "\tudp6\t%s:%d",
                                            IN6_IS_ADDR_UNSPECIFIED(&si.psi.soi_proto.pri_in.insi_laddr.ina_6) ?
                                                "*" : inet_ntop(AF_INET6,
                                                    &si.psi.soi_proto.pri_in.insi_laddr.ina_6,
                                                    buf, INET6_ADDRSTRLEN),
                                            ntohs(si.psi.soi_proto.pri_in.insi_lport));
                                        ready = 1;
                                    } else {
                                        sprintf(outstr + strlen(outstr), "\tudp6\t%s:*",
                                            IN6_IS_ADDR_UNSPECIFIED(&si.psi.soi_proto.pri_in.insi_laddr.ina_6) ?
                                                "*" : inet_ntop(AF_INET6,
                                                    &si.psi.soi_proto.pri_in.insi_laddr.ina_6,
                                                    buf, INET6_ADDRSTRLEN));
                                        ready = 1;
                                    }
                                    /* Remote address and port */
                                    /* UDP is stateless protocol, marking it LISTEN */
                                    listen = 1;
                                    sprintf(outstr + strlen(outstr), "\t*:*");
                                    ready = 1;
                                }
                            }
                        break;

                        case AF_UNIX: /* aka LOCAL socket */
                            if (flg_u || flg_a) {
                                sprintf(outstr + strlen(outstr), "\tunix");
                                /* Bound address */
                                if (si.psi.soi_proto.pri_un.unsi_addr.ua_sun.sun_path[0])
                                    sprintf(outstr + strlen(outstr), "\t%s",
                                        si.psi.soi_proto.pri_un.unsi_addr.ua_sun.sun_path);
                                else
                                    sprintf(outstr + strlen(outstr), "\t0x%llx",
                                        si.psi.soi_proto.pri_un.unsi_conn_pcb);
                                /* Address of socket connected to */
                                if (si.psi.soi_proto.pri_un.unsi_caddr.ua_sun.sun_path[0])
                                    sprintf(outstr + strlen(outstr), "\t%s",
                                        si.psi.soi_proto.pri_un.unsi_caddr.ua_sun.sun_path);
                                else {
                                    sprintf(outstr + strlen(outstr), "\t->??");
                                    listen = 1;
                                }
                                ready = 1;
                            }
                        break;

                        case AF_ROUTE: /* Not much info, do we need more? */
                            if (flg_r || flg_a) {
                                sprintf(outstr + strlen(outstr), "\troute\t0x%llx", si.psi.soi_pcb);
                                ready = 1;
                            }
                        break;

                        case AF_NDRV:
                            if (flg_n || flg_a) {
                                if (si.psi.soi_kind == SOCKINFO_NDRV) { /* is this check useless? */
                                    sprintf(outstr + strlen(outstr), "\tndrv\tunit: %d name: %s",
                                        si.psi.soi_proto.pri_ndrv.ndrvsi_if_unit,
                                        si.psi.soi_proto.pri_ndrv.ndrvsi_if_name);
                                    ready = 1;
                                }
                            }
                        break;

                        case AF_SYSTEM:
                            if (flg_k || flg_a) {
                                switch (si.psi.soi_kind) {
                                    case SOCKINFO_KERN_EVENT:
                                        sprintf(outstr + strlen(outstr), "\tkevt\t0x%llx evt: 0x%x:0x%x:0x%x",
                                            si.psi.soi_pcb,
                                            si.psi.soi_proto.pri_kern_event.kesi_vendor_code_filter,
                                            si.psi.soi_proto.pri_kern_event.kesi_class_filter,
                                            si.psi.soi_proto.pri_kern_event.kesi_subclass_filter);
                                        ready = 1;
                                    break;

                                    case SOCKINFO_KERN_CTL:
                                        sprintf(outstr + strlen(outstr), "\tkctl\t0x%llx ctl: %s id: %d unit: %d",
                                            si.psi.soi_pcb,
                                            si.psi.soi_proto.pri_kern_ctl.kcsi_name,
                                            si.psi.soi_proto.pri_kern_ctl.kcsi_id,
                                            si.psi.soi_proto.pri_kern_ctl.kcsi_unit);
                                        ready = 1;
                                    break;

                                    default: /* May this ever happen? */
                                        sprintf(outstr + strlen(outstr), "\tsunkn");
                                        ready = 1;
                                    break;
                                }
                            }
                        break;


                        default:
                            if (flg_a) {
                                sprintf(outstr + strlen(outstr), "\tunk");
                                ready = 1;
                            }
                        break;
                    }
                    if (ready && ((flg_l && listen) || !flg_l)) {
                        puts(outstr);
                    }
                }
            }
        }

    }

    return 0;
}

/* ------------------------------------------------------------------------------------------------------------------ */
void usage(int ecode) {
    printf("Usage: sockstat [-46klNrquhv]\n\n\
    -4\tShow AF_INET (IPv4) sockets\n\
    -6\tShow AF_INET6 (IPv6) sockets\n\
    -k\tShow AF_SYSTEM (Kernel) sockets\n\
    -n\tShow AF_NDRV sockets\n\
    -r\tShow AF_ROUTE sockets\n\
    -u\tShow AF_LOCAL (UNIX) sockets\n\
    \n\
    -l\tShow only LISTENing sockets\n\
    -q\tQuiet mode - suppress header\n\
    \n\
    -h\tThis help message\n\
    -v\tShov program version\n\n");

    exit(ecode);
}
