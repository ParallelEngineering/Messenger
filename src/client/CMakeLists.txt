find_package(Qt6 COMPONENTS Quick Qml Network REQUIRED)

qt_standard_project_setup(REQUIRES 6.5)

qt_add_executable(Messenger-Client
        WIN32
        MACOSX_BUNDLE
        client.cpp
)

set_target_properties(Messenger-Client PROPERTIES
        MACOSX_BUNDLE_BUNDLE_NAME Messenger
        MACOSX_BUNDLE_GUI_IDENTIFIER de.ParallelEngineering.Messenger
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
            Qt6::Network
)
