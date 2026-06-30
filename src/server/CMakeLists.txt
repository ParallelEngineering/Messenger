find_package(Qt6 REQUIRED COMPONENTS Network)

add_executable(Messenger-Server server.cpp)

target_link_libraries(Messenger-Server PRIVATE Qt6::Network)
