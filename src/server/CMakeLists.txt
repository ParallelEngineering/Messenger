find_package(Qt6 REQUIRED COMPONENTS Core Network Sql)

set(CMAKE_AUTOMOC ON)

add_executable(Messenger-Server
        Storage/message_store.cpp
        Storage/message_store.h
        session.cpp
        session.h
        server.cpp
        server.h
)

add_custom_command(TARGET Messenger-Server POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/Migrations"
            "$<TARGET_FILE_DIR:Messenger-Server>/Migrations"
)

target_link_libraries(Messenger-Server
        PRIVATE
            Qt6::Core
            Qt6::Network
            Qt6::Sql
            Messenger-Shared
            RSA
)
