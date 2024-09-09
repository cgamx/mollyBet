# MollyBet API test

## Instructions for the test

This is an open-ended question that we encourage you to answer using the libraries and style that you feel most comfortable with. We only ask that you use modern C++ and show off your best code.

The requirements are as follows:
  1. Make an HTTP request to login to [Molly API](https://api.mollybet.com/docs/). You can use the test credentials username=devinterview password=OwAb6wrocirEv to get back a session token.
  2. Connect a websocket to the Molly API stream.
  3. Read messages up until the "sync" message.
  4. Disconnect the websocket, and print out the distinct "competition_name" values seen in "event" messages your received.

Please include a self-contained Dockerfile that fetches libraries and compiles your code so we can test it, and also a readme file that explains the structure and decisions in your code.

# Introduction to the Code

This project is written in standard C++17, using CMake as the build system and vcpkg for dependency management.

## Current Dependencies

As listed in the `vcpkg.json` file, the following libraries are used:

- **OpenSSL**: for SSL/TLS support
- **Boost Beast**: for WebSocket communication
- **nlohmann-json**: for JSON parsing
- **fmt**: for formatted output and logging

## Supported Platforms and Compilers

The code has been tested on the following platforms and compilers:

- **Windows**:
  - MSVC 2022 (version 17.11.2)
  - MinGW GCC 13.2.0
  - WSL (Ubuntu) GCC 11.4.0
- **Linux (Fedora 36)**:
  - GCC 12.2.1
- **macOS Monterey**:
  - Apple Clang 14.0.0 (x86_64)

## Code Structure

The project is based on the asynchronous example from Boost Beast, and its structure is simple.

**src**:

 - **Session**:
   This class serves as a state machine for both the HTTPS token query and the WebSocket read operations. The same session instance is reused for both purposes. It allows users to set credentials and define a function to parse JSON data packets from the WebSocket.

 - **Logger**:
   A useful logging utility that I frequently use in my projects.

 - **Main**:
   To manage the `competition_name`, an `std::unordered_set` is used. Initially, I considered alternatives like `std::set`, `std::vector` (with sorting and removing duplicates), but ultimately I chose `std::unordered_set` for its simplicity and lower insertion cost. If performance had been critical, I would have considered using Martinus' Robin Hood Hashing implementation, as it is the fastest I know: [robin-hood-hashing](https://github.com/martinus/robin-hood-hashing).

**dependencies/vcpkg**:

vcpkg project as a git submodule.

## Building the code

```sh
# Git
git clone xxx
git submodule update --init

# Get dependencies and generate project
mkdir build
cd build
cmake ..

# Compile
cmake --build .
```

All the dependencies declared in **vcpkg.json** will be downloaded and compiled when we write: ```cmake ..```

**Note**: Building depencies is very slow specially for boost and openssl.


The executable MollyBet will be placed in the ../bin directory.

## Run the demo

```sh
../bin/MollyBet api.mollybet.com 443
```

## Dockerfile

I don't think a Dockerfile is needed to build this project but I have provide one.

Follow these steps to build and run it.

```sh
sudo docker build --no-cache -t mollybet .

sudo docker run -it mollybet

./MollyBet api.mollybet.com 443
```

## Notes

This is my first time using WebSockets.

The code is based on the asynchronous WebSocket client example from Boost Beast:
[Boost Beast WebSocket Client Async Example](https://www.boost.org/doc/libs/1_86_0/libs/beast/example/websocket/client/async/websocket_client_async.cpp).

As an initial exercise, I modified the example to connect to a WebSocket echo server:
[WebSocket Echo Server](https://websocket.org/tools/websocket-echo-server/).
Allowing the app user to send messages to the server, continuing until the message `quit()` is written by the user.

Once I gained some experience with this, I started implementing the test.
