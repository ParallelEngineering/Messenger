# Messenger

Messenger is a cross-platform client-server messenger application based on the RSA algorithm. The project is still in an early stage and currently provides the foundation for the client and server.

## Functionality

The project currently implements the core flow for a TCP-based group chat:

- **Client connection:** enter a server host, connect automatically to the server, disconnect again when terminating the program, and see the current connection status in the UI.
- **Group chat login:** each server represents one group chat; users join that group chat by signing in with a name and password.
- **Message sending:** compose text messages and send them to the connected server.
- **Message receiving:** display incoming messages in the client window as they arrive from the server.
- **Group chat forwarding:** the server keeps track of the clients connected to its group chat and broadcasts chat messages to every active session.
- **Chat history:** the server stores the group chat history and sends all previous messages to newly connected clients.
- **Protocol validation:** client and server exchange structured messages and reject unsupported protocol versions.
- **Error handling:** connection errors and invalid client-side input are surfaced through the status text and message log.

## Architecture

The repository contains two applications: `Messenger-Client` and `Messenger-Server`. The client is built with Qt/QML; the server is a terminal program.

Both applications use a singleton through `getInstance()`. The project is built with CMake and C++20, with the RSA library included as a submodule.

## Dependencies

```mermaid
flowchart TD
    Client["Messenger-Client"]
    Server["Messenger-Server"]
    RSA["RSA"]
    QtGUI["Qt QML"]
    QtNetwork["Qt Network"]

    Client --> QtGUI
    Client --> QtNetwork
    Server --> QtNetwork
    Client --> RSA
    Server --> RSA

    click QtGUI "https://doc.qt.io/qt-6/qtquick-index.html" "Qt Quick/QML documentation"
    click QtNetwork "https://doc.qt.io/qt-6/qtnetwork-index.html" "Qt Network documentation"
    click RSA "https://github.com/ParallelEngineering/RSA"
```

The application depends on the RSA library for encryption, Qt Quick/QML for the graphical client, and Qt Network for networking functionality.

## Setup

> [!NOTE]
> Qt must be installed locally for the client so CMake can resolve `find_package(Qt6 COMPONENTS Quick Qml REQUIRED)`. The official installation guide is available in the [Qt documentation](https://doc.qt.io/qt-6/get-and-install-qt.html).

Clone the repository including its submodules:

```bash
git clone --recurse-submodules https://github.com/ParallelEngineering/Messenger.git
```

If the repository has already been cloned without submodules, they can be initialized recursively with remote updates:

```bash
git submodule update --init --recursive --remote
```

## Build

The project can be configured and built with CMake:

```bash
cmake -S . -B cmake-build-debug
cmake --build cmake-build-debug
```

This creates the `Messenger-Client` and `Messenger-Server` targets.
