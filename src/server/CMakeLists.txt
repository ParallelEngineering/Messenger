find_package(Qt6 REQUIRED COMPONENTS Core Network)

set(CMAKE_AUTOMOC ON)

add_executable(Messenger-Server
        client_session.cpp
        client_session.h
        server.cpp
        server.h
)

target_link_libraries(Messenger-Server
        PRIVATE
            Qt6::Core
            Qt6::Network
            Messenger-Shared
)
