add_executable(clivcut-render main.cpp)
target_link_libraries(clivcut-render PRIVATE clivcut_engine Qt6::Gui)
target_compile_definitions(clivcut-render PRIVATE CLIVCUT_VERSION="${PROJECT_VERSION}")
install(TARGETS clivcut-render RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
