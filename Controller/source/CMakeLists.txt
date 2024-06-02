
add_executable(${proj_name}
	CEnumerateSerial/enumser.cpp
	CEnumerateSerial/enumser.h

	alias.hpp
	audio.cpp
	audio.hpp
	client.hpp
	console.cpp
	console.hpp
	console_commands.hpp
	door_lock.cpp
	door_lock.hpp
	file_io.cpp
	file_io.hpp
	imgui_config.hpp
	main.cpp
	print.hpp
	random.hpp
	ring_dispenser.cpp
	ring_dispenser.hpp
	scope_exit.hpp
	serial_port.cpp
	serial_port.hpp
	server.cpp
	server.hpp
	settings.cpp
	settings.hpp
	targets.hpp
	targets.cpp
	time.hpp
	timer.cpp
	timer.hpp
 )

#Libs
find_package(glfw3 CONFIG REQUIRED)
target_link_libraries(${proj_name} PRIVATE glfw)

find_package(glad CONFIG REQUIRED)
target_link_libraries(${proj_name} PRIVATE glad::glad)

find_package(OpenGL REQUIRED)
target_link_libraries(${proj_name} PRIVATE OpenGL::GL)

find_package(fmt CONFIG REQUIRED)
target_link_libraries(${proj_name} PRIVATE fmt::fmt)

find_package(imgui CONFIG REQUIRED)
target_link_libraries(${proj_name} PRIVATE imgui::imgui)

find_package(implot CONFIG REQUIRED)
target_link_libraries(${proj_name} PRIVATE implot::implot)

# find_package(harfbuzz CONFIG REQUIRED)
# target_link_libraries(${proj_name} PRIVATE harfbuzz::harfbuzz)

find_package(asio CONFIG REQUIRED)
target_link_libraries(${proj_name} PRIVATE asio::asio)

find_package(OpenAL CONFIG REQUIRED)
target_link_libraries(${proj_name} PRIVATE OpenAL::OpenAL)

find_package(sndfile CONFIG REQUIRED)
target_link_libraries(${proj_name} PRIVATE SndFile::sndfile)

#Header only
find_package(glm CONFIG REQUIRED)
target_include_directories(${proj_name} PRIVATE glm::glm)

find_package(Boost REQUIRED)
target_include_directories(${proj_name} PRIVATE ${Boost_INCLUDE_DIRS})

target_compile_features(${proj_name} PRIVATE cxx_std_23)

if ("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
	target_compile_definitions(${proj_name} PRIVATE IS_DEBUG=1)
else()
	target_compile_definitions(${proj_name} PRIVATE IS_DEBUG=0)
endif()

target_compile_definitions(${proj_name} PRIVATE CONTROLLER=1)

target_compile_definitions(${proj_name} PRIVATE IMGUI_USER_CONFIG="imgui_config.hpp")
target_compile_definitions(${proj_name} PRIVATE _WIN32_WINNT=_WIN32_WINNT_WIN7)

