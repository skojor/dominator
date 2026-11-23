cmake_minimum_required(VERSION 3.19)
project(Dominator LANGUAGES CXX)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Network Widgets)

qt_standard_project_setup()

qt_add_executable(Dominator
    WIN32 MACOSX_BUNDLE
    main.cpp
    mainwindow.cpp
    mainwindow.h
    localipcserver.cpp
    localipcserver.h
    positionreporter.cpp
    positionreporter.h
    scancontroller.cpp
    scancontroller.h
)

target_link_libraries(Dominator
    PRIVATE
        Qt::Core
        Qt::Network
        Qt::Widgets
)

install(TARGETS Dominator
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

qt_generate_deploy_app_script(
    TARGET Dominator
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT ${deploy_script})

include(CTest)
if(BUILD_TESTING)
    find_package(Qt6 6.5 REQUIRED COMPONENTS Test)

    qt_add_executable(DominatorTests
        tests/ipc_test.cpp
        localipcserver.cpp
        localipcserver.h
        scancontroller.cpp
        scancontroller.h
    )

    target_include_directories(DominatorTests
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}
    )

    target_link_libraries(DominatorTests
        PRIVATE
            Qt::Core
            Qt::Network
            Qt::Test
    )

    add_test(NAME DominatorTests COMMAND DominatorTests)
endif()
