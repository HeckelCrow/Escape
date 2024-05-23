cmake_minimum_required (VERSION 3.25)

set(X_VCPKG_APPLOCAL_DEPS_INSTALL ON)

# vcpkg doesn't install the libs if 
# CMAKE_INSTALL_SYSTEM_RUNTIME_DESTINATION isn't set here.
# Everything vcpkg uses needs to be set before project()
set(CMAKE_INSTALL_SYSTEM_RUNTIME_DESTINATION 
	${CMAKE_INSTALL_PREFIX}/${proj_name})

set (proj_name EscapeGame)
project (${proj_name} C CXX)

include_directories(source)
include_directories("../Common")
add_subdirectory(source)

install(
	TARGETS ${proj_name} 
	DESTINATION ${CMAKE_INSTALL_PREFIX}/${proj_name})

install(
	DIRECTORY data
	DESTINATION ${CMAKE_INSTALL_PREFIX}/${proj_name})

# CMAKE_INSTALL_SYSTEM_RUNTIME_DESTINATION needs to be set again
# I don't know why.
set(CMAKE_INSTALL_SYSTEM_RUNTIME_DESTINATION 
	${CMAKE_INSTALL_PREFIX}/${proj_name})
include(InstallRequiredSystemLibraries)
