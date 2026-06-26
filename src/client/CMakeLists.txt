find_package(Qt6 COMPONENTS Quick Qml REQUIRED)

qt_standard_project_setup(REQUIRES 6.5)

qt_add_executable(Messenger-Client
        client.cpp
)

qt_add_qml_module(Messenger-Client
        URI Messenger.Client
        VERSION 1.0
        QML_FILES
            Main.qml
)

target_link_libraries(Messenger-Client
        PRIVATE
            Qt6::Quick
            Qt6::Qml
)
