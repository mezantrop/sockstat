# Sockstat for macOS

[![CodeQL](https://github.com/mezantrop/sockstat/actions/workflows/codeql.yml/badge.svg)](https://github.com/mezantrop/sockstat/actions/workflows/codeql.yml)
[![C/C++ CI](https://github.com/mezantrop/sockstat/actions/workflows/c-cpp-macos.yml/badge.svg)](https://github.com/mezantrop/sockstat/actions/workflows/c-cpp-macos.yml)

## FreeBSD-like sockstat for macOS using libproc

<a href="https://www.buymeacoffee.com/mezantrop" target="_blank"><img src="https://cdn.buymeacoffee.com/buttons/default-orange.png" alt="Buy Me A Coffee" height="41" width="174"></a>

![sockstat](sockstat.png)

### Installation

To install `sockstat` into `bin` directory under `/usr/local`, run:

```sh
make install clean
```

If a different installation path is required, define `PREFIX`:

```sh
make install clean PREFIX=/path/to/install
```

To deinstall:

```sh
make uninstall
```

### Usage

```
sockstat-1.0

Usage:
    sockstat [-46klNrquh]

    -4  Show AF_INET (IPv4) sockets
    -6  Show AF_INET (IPv6) sockets
    -k  Show AF_SYSTEM (Kernel) sockets
    -n  Show AF_NDRV sockets
    -r  Show AF_ROUTE sockets
    -u  Show AF_LOCAL (UNIX) sockets

    -l  Show only LISTENing sockets
    -q  Quiet mode - suppress header

    -h  This help message
```

If you have an idea, a question, or found a problem, do not hesitate to open an
[issue](https://github.com/mezantrop/sockstat/issues) or mail me: Mikhail Zakharov <zmey20000@yahoo.com>
