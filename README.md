[![CMake](https://github.com/falk-werner/sockman/actions/workflows/cmake.yml/badge.svg)](https://github.com/falk-werner/sockman/actions/workflows/cmake.yml)

# sockman

sockman (**sock**et **man**ager) is a lightweight C++ wrapper around [epoll](https://man7.org/linux/man-pages/man7/epoll.7.html).

## Motivation

There are already a number of good, well documented and widely used `socket`
and / or `event` libraries in the wild: [asio](https://think-async.com/Asio/),
[libevent](https://libevent.org/), [lubuv](https://github.com/libuv/libuv) are
some well known examples. Why do we need yet another one?

Well, sockman does neither intends to provide a socket abstraction for multiple
platforms nor to be a full blown event library. In contrast sockman intends to
provide a very lightweight wrapper to [epoll](https://man7.org/linux/man-pages/man7/epoll.7.html) for C++ applications that handles sockets.

It is believed that the linux socket API is well documented and well understood,
so there is no need for any abstraction _(at least as long there are no 
requirements to support multiple platforms)_. Having less abstraction allows applications a fine grained control, e.g. managing buffers.

## Basic Usage

sockman introduces the `manager` class, which is actual a wrapper around
`epoll`. A `socket` and an associated `callback` are added to the `manager`.
The `manager` will be invoke the `callback` whenever an event for the socket
is detected.

````cpp
sockman::manager manager;
manager.add(some_socket, sockman::readable, [](int the_sock, auto events){
    if (events.error())
    {
        // handle socket error
    }
    else if (events.hungup())
    {
        // remove connection closed
    }
    else if (events.readable())
    {
        // data available, read(the_sock, ...)
    }
    else if (events.writable())
    {
        // data can be written, write(the_sock, ...)
    }
});
````

There are 4 different events for a socket:

* `error` : a socket error occurred
* `hungup`: the remote connection is closed
* `readable`: there are data to `read` (or to `accept` for a listening socket)
* `writable`: it is possible to `write`data (or a connection is established)

During an applications lifetime it might be interested in different events.
Therefore, the interesting events can be modified via the `manager`.

````cpp
manager.notify_on_writable(some_socket, true);  // enable  callback on writable
manager.notify_on_writable(some_socket, false); // disable callback on writable
````

Note that only `readable` and `writable`can be configured by the user,
`error` and `hungup` will always be detected for all manages sockets.

### Multi-Threading

sockman does not handle threads by itself. All thread handling is up to the
user. It is **not thread safe** to call any method of a `manager` instance
unsynchronized from multiple threads.

### Buffer handling

sockman does not handle buffers at all. It only informs an application
about event of managed sockets.

### Socket lifetime

sockman does not manage the lifetime of sockets. It does not takes the
ownership of the sockets. It is up to the user to remove sockets from
the `manager` when they are no longer needed.

## Build and Install

````
cmake -B build
cmake --build build
sudo cmake --install build
````

_Note:_ One might use [checkinstall](https://en.wikipedia.org/wiki/CheckInstall) to create an installable (and removable) package.

### Options

| Option           | Default | Description |
| ---------------- | ------- | ----------- |
| WITHOUT_TESTS    | OFF     | disables build of unit tests |
| WITHOUT_EXAMPLES | OFF     | disables build of examples |

### Targets

* **test**: runs unit tests  
  `cmake --build build -t test`
* **memcheck**: runs unit tests with valgrind/memcheck _(requires valgrind)_  
  `cmake --build build -t memcheck`

### Dependecies

* [Google Test](https://github.com/google/googletest) _(unit test only)_


## References

* [asio](https://think-async.com/Asio/)
* [checkinstall](https://en.wikipedia.org/wiki/CheckInstall)
* [epoll](https://man7.org/linux/man-pages/man7/epoll.7.html)
* [Google Test](https://github.com/google/googletest)
* [libevent](https://libevent.org/)
* [lubuv](https://github.com/libuv/libuv)