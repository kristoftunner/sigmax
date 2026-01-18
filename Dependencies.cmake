include(cmake/CPM.cmake)

# Done as a function so that updates to variables like
# CMAKE_CXX_FLAGS don't propagate out to other
# targets
function(sigmax_setup_dependencies)

    # For each dependency, see if it's
    # already been provided to us by a parent project

    if(NOT TARGET fmtlib::fmtlib)
        cpmaddpackage("gh:fmtlib/fmt#11.1.4")
    endif()

    if(NOT TARGET spdlog::spdlog)
        cpmaddpackage(
            NAME
            spdlog
            VERSION
            1.15.2
            GITHUB_REPOSITORY
            "gabime/spdlog"
            OPTIONS
            "SPDLOG_FMT_EXTERNAL ON")
    endif()
    if(NOT TARGET GTest::gtest_main)
        cpmaddpackage(
            NAME
            googletest
            GITHUB_REPOSITORY
            google/googletest
            VERSION
            1.14.0
            OPTIONS
            "gtest_force_shared_crt ON")
    endif()

    if(NOT TARGET tools::tools)
        cpmaddpackage("gh:lefticus/tools#update_build_system")
    endif()

    # nlohmann/json - header-only JSON library
    if(NOT TARGET nlohmann_json::nlohmann_json)
        cpmaddpackage(
            NAME
            nlohmann_json
            VERSION
            3.12.0
            GITHUB_REPOSITORY
            "nlohmann/json"
            OPTIONS
            "JSON_BuildTests OFF"
            "JSON_Install OFF")
    endif()

    # Tracy profiler
    if(NOT TARGET Tracy::TracyClient)
        cpmaddpackage(
            NAME
            tracy
            GITHUB_REPOSITORY
            "wolfpld/tracy"
            VERSION
            0.13.1
            OPTIONS
            "TRACY_ENABLE ON"
            "TRACY_ON_DEMAND OFF")
    endif()

endfunction()
