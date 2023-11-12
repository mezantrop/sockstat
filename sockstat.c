/*
* Copyright (c) 2023, Mikhail Zakharov <zmey20000@yahoo.com>
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
#include <unistd.h>
#include <stdlib.h>

#include <arpa/inet.h>

#include <libproc.h>

/* ------------------------------------------------------------------------------------------------------------------ */
#define PID_MAX 99999
#define FDS_MAX 200000

#define INET_ADDRPORTSTRLEN INET6_ADDRSTRLEN + 6    /* MAX: ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff + ':' + '65535' */

/* ------------------------------------------------------------------------------------------------------------------ */
int main(int argc, char *argv[]) {
    int *pids = NULL;                                                           /* PIDs array buffer */
    int npids = 0;                                                              /* Number of PIDs */
    static struct proc_fdinfo *fds = NULL;                                      /* FDs array */
    int mfds = 0, nfds = 0;                                                     /* Memory and number of FDS */
    struct proc_bsdinfo pinfo;
    char pname[2 * MAXCOMLEN];                                                  /* Process name */
    char ppath[PROC_PIDPATHINFO_MAXSIZE];                                       /* Process path */

    struct socket_fdinfo si;

    char lbuf[INET_ADDRPORTSTRLEN] = {0};                                       /* Buffers to store local & */
    char rbuf[INET_ADDRPORTSTRLEN] = {0};                                       /* remote IPv4/6 addresses */


    pids = (int *)malloc(sizeof(int) * PID_MAX);                                /* TODO: REDUCE MEMORY! */
    npids = proc_listpids(PROC_ALL_PIDS, 0, pids, sizeof(int) * PID_MAX);       /* PIDs */

    fds = (struct proc_fdinfo *)malloc(sizeof(struct proc_fdinfo) * FDS_MAX);   /* TODO: REDUCE MEMORY! */

    printf("%-5s %-31s %-3s %-5s %-40s %-40s\n",
        "PID", "COMMAND", "FD", "PROTO", "LOCAL ADDRESS", "REMOTE ADDRESS");

    for (int i = 0; i < npids; i++) {
        /* PID => FDs */
        if ((mfds = proc_pidinfo(pids[i], PROC_PIDLISTFDS, 0, fds, sizeof(struct proc_fdinfo) * FDS_MAX))) {
            proc_pidinfo(pids[i], PROC_PIDTBSDINFO, 0, &pinfo, sizeof(info));                    /* a PIDs => PID */

            nfds = (int)(mfds / sizeof(struct proc_fdinfo));
            for (int k = 0; k < nfds; k++) {
                if (proc_pidfdinfo(pids[i], fds[k].proc_fd, PROC_PIDFDSOCKETINFO, &si, sizeof(si)) >= sizeof(si)) {

                    /* NB! pbi_name could be forged: setprogname(3) */
                    printf("%-5d %-31s %-3d",
                        pinfo.pbi_pid, pinfo.pbi_name[0] ? pinfo.pbi_name : pinfo.pbi_comm, fds[k].proc_fd);

                    switch (si.psi.soi_family) {
                        case AF_INET:
                            if (si.psi.soi_kind == SOCKINFO_TCP) {                              /* IPv4 TCP */
                                printf(" %-5s %34s:%-5d", "tcp4",
                                    inet_ntop(AF_INET,
                                        &si.psi.soi_proto.pri_tcp.tcpsi_ini.insi_laddr.ina_46.i46a_addr4,
                                        lbuf, INET_ADDRSTRLEN),
                                    ntohs(si.psi.soi_proto.pri_tcp.tcpsi_ini.insi_lport));

                                printf(" %34s:%-5d",
                                    inet_ntop(AF_INET,
                                        &si.psi.soi_proto.pri_tcp.tcpsi_ini.insi_faddr.ina_46.i46a_addr4,
                                        rbuf, INET_ADDRSTRLEN),
                                    ntohs(si.psi.soi_proto.pri_tcp.tcpsi_ini.insi_fport));
                            } else {                                                            /* IPv4 UDP */
                                printf(" %-5s %34s:%-5d", "udp4",
                                    inet_ntop(AF_INET,
                                        &si.psi.soi_proto.pri_in.insi_laddr.ina_46.i46a_addr4,
                                        lbuf, INET_ADDRSTRLEN),
                                    ntohs(si.psi.soi_proto.pri_in.insi_lport));

                                printf(" %34s:%-5d",
                                    inet_ntop(AF_INET,
                                        &si.psi.soi_proto.pri_in.insi_faddr.ina_46.i46a_addr4,
                                        rbuf, INET_ADDRSTRLEN),
                                    ntohs(si.psi.soi_proto.pri_in.insi_fport));
                            }
                        break;

                        case AF_INET6:
                            if (si.psi.soi_kind == SOCKINFO_TCP) {                              /* IPv6 TCP */
                                printf(" %-5s %34s:%-5d", "tcp6",
                                    inet_ntop(AF_INET6,
                                        &si.psi.soi_proto.pri_tcp.tcpsi_ini.insi_laddr.ina_6,
                                        lbuf, INET6_ADDRSTRLEN),
                                    ntohs(si.psi.soi_proto.pri_tcp.tcpsi_ini.insi_lport));

                                printf(" %34s:%-5d",
                                    inet_ntop(AF_INET6,
                                        &si.psi.soi_proto.pri_tcp.tcpsi_ini.insi_faddr.ina_6,
                                        rbuf, INET6_ADDRSTRLEN),
                                    ntohs(si.psi.soi_proto.pri_tcp.tcpsi_ini.insi_fport));
                            } else {                                                            /* IPv6 UDP */
                                printf(" %-5s %34s:%-5d", "udp6",
                                    inet_ntop(AF_INET6,
                                        &si.psi.soi_proto.pri_tcp.tcpsi_ini.insi_fport,
                                        lbuf, INET6_ADDRSTRLEN),
                                    ntohs(si.psi.soi_proto.pri_in.insi_lport));

                                printf(" %34s:%-5d",
                                    inet_ntop(AF_INET6,
                                        &si.psi.soi_proto.pri_in.insi_faddr.ina_6,
                                        rbuf, INET6_ADDRSTRLEN),
                                    ntohs(si.psi.soi_proto.pri_in.insi_fport));
                            }
                        break;

                        case AF_UNIX:                                                           /* UNIX socket */
                            printf(" %-5s", "unix");
                            if (si.psi.soi_proto.pri_un.unsi_addr.ua_sun.sun_family != AF_UNIX &&
                                si.psi.soi_proto.pri_un.unsi_addr.ua_sun.sun_family == AF_UNSPEC)

                                printf(" 0x%16llx",
                                si.psi.soi_proto.pri_un.unsi_conn_pcb);
                            else
                                printf(" --");

                            if (si.psi.soi_proto.pri_un.unsi_addr.ua_sun.sun_path[0]) {
                                printf(" %s",
                                    si.psi.soi_proto.pri_un.unsi_addr.ua_sun.sun_path);
                            } /* else
                                printf(" No address"); */
                        break;

                        default:
                            printf(" %-5s", "unkn");
                        break;
                    }
                    printf("\n");
                }
            }
        }

    }

    return 0;
}
