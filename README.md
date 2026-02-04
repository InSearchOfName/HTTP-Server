
# HTTP-Server

Simple C HTTP server.

I built this server to learn more about C and to have a realistic project to work toward. This was my first time coding in C. It’s not perfect, and it doesn’t implement everything required by the HTTP/1.1 RFC.

## Build

```sh
make
```

## Run

```sh
./server
```

By default it serves [index.html](index.html).

## Files

- [main.c](main.c) - program entry point.
- [server.c](server.c) / [server.h](server.h) - server implementation.
- [index.html](index.html) - sample page.
- [Makefile](Makefile) - build script.
