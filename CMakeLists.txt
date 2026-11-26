cmake_minimum_required(VERSION 3.19)
project(Dominator LANGUAGES CXX)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Network Svg Widgets)

qt_standard_project_setup()
set(CMAKE_AUTORCC ON)

qt_add_executable(Dominator
    WIN32 MACOSX_BUNDLE
    main.cpp
    resources/icons.qrc
    dishcliclient.cpp
    dishcliclient.h
    mainwindow.cpp
    mainwindow.h
    dishdirectionindicator.cpp
    dishdirectionindicator.h
    elevationindicator.cpp
    elevationindicator.h
    dishsettings.cpp
    dishsettings.h
    dishsettingsdialog.cpp
    dishsettingsdialog.h
    localipcserver.cpp
    localipcserver.h
    positionreporter.cpp
    positionreporter.h
    n2yoclient.cpp
    n2yoclient.h
    satellitepreviewdialog.cpp
    satellitepreviewdialog.h
    scancontroller.cpp
    scancontroller.h
)

target_link_libraries(Dominator
    PRIVATE
        Qt::Core
        Qt::Network
        Qt::Svg
        Qt::Widgets
)

if(WIN32)
    target_sources(Dominator PRIVATE resources/dominator.rc)
    target_link_libraries(Dominator PRIVATE Crypt32)
endif()

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
    qt_add_executable(DataStreamTests tests/datastream_test.cpp datastream.cpp datastream.h)
    target_compile_features(DataStreamTests PRIVATE cxx_std_17)
    target_include_directories(DataStreamTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(DataStreamTests PRIVATE Qt::Test)
    add_test(NAME DataStreamTests COMMAND DataStreamTests)

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

    if(WIN32)
        target_link_libraries(DominatorTests PRIVATE Crypt32)
    endif()

    add_test(NAME DominatorTests COMMAND DominatorTests)

    qt_add_executable(DishSettingsTests
        tests/dishsettings_test.cpp
        dishsettings.cpp
        dishsettings.h
    )

    target_include_directories(DishSettingsTests
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}
    )

    target_link_libraries(DishSettingsTests
        PRIVATE
            Qt::Core
            Qt::Test
    )

    if(WIN32)
        target_link_libraries(DishSettingsTests PRIVATE Crypt32)
    endif()

    add_test(NAME DishSettingsTests COMMAND DishSettingsTests)

    qt_add_executable(DishDirectionIndicatorTests
        tests/dishdirectionindicator_test.cpp
        dishdirectionindicator.cpp
        dishdirectionindicator.h
        elevationindicator.cpp
        elevationindicator.h
    )

    target_include_directories(DishDirectionIndicatorTests
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}
    )

    target_link_libraries(DishDirectionIndicatorTests
        PRIVATE
            Qt::Widgets
            Qt::Test
    )

    add_test(NAME DishDirectionIndicatorTests COMMAND DishDirectionIndicatorTests)
    set_tests_properties(DishDirectionIndicatorTests PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    )

    qt_add_executable(DishCliClientTests
        tests/dishcliclient_test.cpp
        dishcliclient.cpp
        dishcliclient.h
    )

    target_include_directories(DishCliClientTests
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}
    )

    target_link_libraries(DishCliClientTests
        PRIVATE
            Qt::Core
            Qt::Network
            Qt::Test
    )

    add_test(NAME DishCliClientTests COMMAND DishCliClientTests)

    qt_add_executable(N2yoClientTests tests/n2yoclient_test.cpp n2yoclient.cpp n2yoclient.h)
    target_include_directories(N2yoClientTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(N2yoClientTests PRIVATE Qt::Network Qt::Test)
    add_test(NAME N2yoClientTests COMMAND N2yoClientTests)
endif()
