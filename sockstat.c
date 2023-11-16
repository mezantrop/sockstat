/* ------------------------------------------------------------------------------------------------------------------ */
/* FreeBSD-like sockstat for macOS using libproc                                                                      */
/* ------------------------------------------------------------------------------------------------------------------ */

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
#include <limits.h>

#include <sys/types.h>
#include <sys/sysctl.h>
#include <pwd.h>
#include <uuid/uuid.h>

#include <arpa/inet.h>

#include <libproc.h>

/* ------------------------------------------------------------------------------------------------------------------ */
void usage(int ecode);
void prt_common(struct passwd *pwd, struct proc_bsdinfo pinfo, int32_t proc_fd);

/* ------------------------------------------------------------------------------------------------------------------ */
#define PROG_NAME       "sockstat"
#define PROG_VERSION    "0.3"

#define MAXPROC         16384;
/* ------------------------------------------------------------------------------------------------------------------ */
int main(int argc, char* argv[]) {
    int mproc = MAXPROC;                                                        /* Max number of concurrent processes */
    size_t mproc_len = sizeof(mproc);                                           /* Set it enormously big to tune later*/
    int *pids = NULL;                                                           /* PIDs array buffer */
    int npids = 0;                                                              /* Number of PIDs */
    static struct proc_fdinfo *fds = NULL;                                      /* FDs array */
    int mfds = 0, nfds = 0;                                                     /* Memory and number of FDS */
    struct proc_bsdinfo pinfo;
    struct socket_fdinfo si;
    struct passwd *pwd;
    char lbuf[INET6_ADDRSTRLEN] = {0};                                          /* Buffers to store local & */
    char rbuf[INET6_ADDRSTRLEN] = {0};                                          /* remote IPv4/6 addresses */
    int flg, flg_i4 = 0, flg_i6 = 0, flg_u = 0, flg_a = 0;


    while ((flg = getopt(argc, argv, "46uh")) != -1)
        switch(flg) {
            case '4':
                flg_i4 = 1;
            break;

            case '6':
                flg_i6 = 1;
            break;

            case 'u':
                flg_u = 1;
            break;

            case 'h':
            default:
                (void)usage(0);
        }

    if (!flg_i4 && !flg_i6 && !flg_u) flg_a = 1;

    if (sysctlbyname("kern.maxproc", &mproc, &mproc_len, NULL, 0) == -1) {
        perror("Unable to get the maximum allowed number of processes");
        printf("Assuming it to be lower than: %d\n", mproc);
    }

    pids = (int *)malloc(sizeof(int) * mproc);
    npids = proc_listpids(PROC_ALL_PIDS, 0, pids, sizeof(int) * mproc);                             /* PIDs */
    /* proc_listpids() returns bytes (!), not count of pids (!) */

    fds = (struct proc_fdinfo *)malloc(sizeof(struct proc_fdinfo) * OPEN_MAX);

    printf("%-20s %-5s %-31s %-3s %-5s %s\n",
        "USER", "PID", "COMMAND", "FD", "PROTO", "LOCAL ADDRESS / REMOTE ADDRESS");

    for (int i = 0; i < npids/sizeof(int); i++) {
        /* PID => FDs */
        if ((mfds = proc_pidinfo(pids[i], PROC_PIDLISTFDS, 0, fds, sizeof(struct proc_fdinfo) * OPEN_MAX))) {
            proc_pidinfo(pids[i], PROC_PIDTBSDINFO, 0, &pinfo, sizeof(pinfo));                    /* a PIDs => PID */

            nfds = (int)(mfds / sizeof(struct proc_fdinfo));
            for (int k = 0; k < nfds; k++) {
                if (proc_pidfdinfo(pids[i], fds[k].proc_fd, PROC_PIDFDSOCKETINFO, &si, sizeof(si)) >= sizeof(si)) {
                    pwd = getpwuid(pinfo.pbi_uid);
                    switch (si.psi.soi_family) {
                        case AF_INET:
                            if (flg_i4 || flg_a) {
                                prt_common(pwd, pinfo, fds[k].proc_fd);
                                if (si.psi.soi_kind == SOCKINFO_TCP) {                              /* IPv4 TCP */
                                    printf(" %-5s %s:%d", "tcp4",
                                        inet_ntop(AF_INET,
                                            &si.psi.soi_proto.pri_tcp.tcpsi_ini.insi_laddr.ina_46.i46a_addr4,
                                            lbuf, INET_ADDRSTRLEN),
                                        ntohs(si.psi.soi_proto.pri_tcp.tcpsi_ini.insi_lport));

                                    printf(" %s:%d",
                                        inet_ntop(AF_INET,
                                            &si.psi.soi_proto.pri_tcp.tcpsi_ini.insi_faddr.ina_46.i46a_addr4,
                                            rbuf, INET_ADDRSTRLEN),
                                        ntohs(si.psi.soi_proto.pri_tcp.tcpsi_ini.insi_fport));
                                } else {                                                            /* IPv4 UDP */
                                    printf(" %-5s %s:%d", "udp4",
                                        inet_ntop(AF_INET,
                                            &si.psi.soi_proto.pri_in.insi_laddr.ina_46.i46a_addr4,
                                            lbuf, INET_ADDRSTRLEN),
                                        ntohs(si.psi.soi_proto.pri_in.insi_lport));

                                    printf(" %s:%d",
                                        inet_ntop(AF_INET,
                                            &si.psi.soi_proto.pri_in.insi_faddr.ina_46.i46a_addr4,
                                            rbuf, INET_ADDRSTRLEN),
                                        ntohs(si.psi.soi_proto.pri_in.insi_fport));
                                }
                                printf("\n");
                            }
                        break;

                        case AF_INET6:
                            if (flg_i6 || flg_a) {
                                prt_common(pwd, pinfo, fds[k].proc_fd);
                                if (si.psi.soi_kind == SOCKINFO_TCP) {                              /* IPv6 TCP */
                                    printf(" %-5s %s:%d", "tcp6",
                                        inet_ntop(AF_INET6,
                                            &si.psi.soi_proto.pri_tcp.tcpsi_ini.insi_laddr.ina_6,
                                            lbuf, INET6_ADDRSTRLEN),
                                        ntohs(si.psi.soi_proto.pri_tcp.tcpsi_ini.insi_lport));

                                    printf(" %s:%d",
                                        inet_ntop(AF_INET6,
                                            &si.psi.soi_proto.pri_tcp.tcpsi_ini.insi_faddr.ina_6,
                                            rbuf, INET6_ADDRSTRLEN),
                                        ntohs(si.psi.soi_proto.pri_tcp.tcpsi_ini.insi_fport));
                                } else {                                                            /* IPv6 UDP */
                                    printf(" %-5s %s:%d", "udp6",
                                        inet_ntop(AF_INET6,
                                            &si.psi.soi_proto.pri_tcp.tcpsi_ini.insi_fport,
                                            lbuf, INET6_ADDRSTRLEN),
                                        ntohs(si.psi.soi_proto.pri_in.insi_lport));

                                    printf(" %s:%d",
                                        inet_ntop(AF_INET6,
                                            &si.psi.soi_proto.pri_in.insi_faddr.ina_6,
                                            rbuf, INET6_ADDRSTRLEN),
                                        ntohs(si.psi.soi_proto.pri_in.insi_fport));
                                }
                                printf("\n");
                            }
                        break;

                        case AF_UNIX:                                                           /* UNIX socket */
                            if (flg_u || flg_a) {
                                prt_common(pwd, pinfo, fds[k].proc_fd);
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
                                printf("\n");
                            }
                        break;

                        default:
                            if (flg_a) {
                                prt_common(pwd, pinfo, fds[k].proc_fd);
                                printf(" %-5s", "unkn");
                                printf("\n");
                            }
                        break;
                    }
                }
            }
        }

    }

    return 0;
}

/* ------------------------------------------------------------------------------------------------------------------ */
void usage(int ecode) {
    printf("Usage:\n\
    sockstat [-46uh]\n\n\
    -4\tShow AF_INET (IPv4) sockets\n\
    -6\tShow AF_INET (IPv6) sockets\n\
    -u\tShow AF_LOCAL (UNIX) sockets\n\
    -h\tThis help message\n\n");

    exit(ecode);
}

/* ------------------------------------------------------------------------------------------------------------------ */
void prt_common(struct passwd *pwd, struct proc_bsdinfo pinfo, int32_t proc_fd) {
    /* NB! pbi_name could be forged: setprogname(3) */
    printf("%-20s %-5d %-31s %-3d",
        pwd->pw_name, pinfo.pbi_pid, pinfo.pbi_name[0] ? pinfo.pbi_name : pinfo.pbi_comm, proc_fd);
}
