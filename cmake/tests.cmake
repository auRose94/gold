enable_testing()

add_executable(
	goldTests
	tests/test.cpp
	tests/test_file_decode.cpp
)

add_executable(
	goldFileErrorTests
	tests/test_file_errors.cpp
)

if(MSVC)
	target_compile_options(goldTests PRIVATE /W4)
else()
	target_compile_options(goldTests PRIVATE -Wall -Wextra -pedantic)
endif()

target_include_directories(
	goldTests
	PUBLIC
		"include"
		"tests"
)

target_link_libraries(
	goldTests
	PRIVATE
		gold::shared
)

target_include_directories(goldFileErrorTests PRIVATE "include" "tests")
target_link_libraries(goldFileErrorTests PRIVATE gold::shared)

target_compile_features(
	goldTests
	PRIVATE
		cxx_variadic_templates
		cxx_nullptr
		cxx_generic_lambdas
		cxx_lambdas
		cxx_auto_type
		cxx_variable_templates
		cxx_variadic_macros
		cxx_template_template_parameters
		cxx_std_26
)

add_test(NAME goldTests COMMAND goldTests)

add_executable(
	goldSubsystemTests
	tests/subsystem_test.cpp
	src/worker.cpp
)
if(MSVC)
	target_compile_options(goldSubsystemTests PRIVATE /W4)
else()
	target_compile_options(goldSubsystemTests PRIVATE -Wall -Wextra -pedantic)
endif()
target_include_directories(goldSubsystemTests PRIVATE "include" "tests")
target_link_libraries(goldSubsystemTests PRIVATE gold::shared)
target_compile_features(goldSubsystemTests PRIVATE cxx_std_26)
add_test(NAME goldSubsystemTests COMMAND goldSubsystemTests)
add_test(NAME goldFileErrorTests COMMAND goldFileErrorTests)

if(GOLD_BUILD_LANG)
	add_executable(
		goldLangTests
		tests/langTest.cpp
	)
	if(MSVC)
		target_compile_options(goldLangTests PRIVATE /W4)
	else()
		target_compile_options(goldLangTests PRIVATE -Wall -Wextra -pedantic)
	endif()
	target_include_directories(
		goldLangTests
		PUBLIC
			"include"
			"include/lang"
			"tests"
	)
	target_link_libraries(
		goldLangTests
		PRIVATE
			gold::lang
	)
	target_compile_features(
		goldLangTests
		PRIVATE
			cxx_std_26
	)
	add_test(NAME goldLangTests COMMAND goldLangTests)
endif()

if(GOLD_BUILD_GAME)
	add_executable(
		goldGameTests
		tests/gameTest.cpp
		tests/gameMeshAssetTest.cpp
	)
	if(MSVC)
		target_compile_options(goldGameTests PRIVATE /W4)
	else()
		target_compile_options(goldGameTests PRIVATE -Wall -Wextra -pedantic)
	endif()
	target_include_directories(
		goldGameTests
		PUBLIC
			"include"
			"include/game"
			"tests"
	)
	target_link_libraries(
		goldGameTests
		PRIVATE
			gold::game
	)
	target_compile_features(
		goldGameTests
		PRIVATE
			cxx_std_26
	)
	add_test(NAME goldGameTests COMMAND goldGameTests)

	add_executable(
		goldRenderBackendTests
		tests/renderBackendTest.cpp
	)
	if(MSVC)
		target_compile_options(goldRenderBackendTests PRIVATE /W4)
	else()
		target_compile_options(goldRenderBackendTests PRIVATE -Wall -Wextra -pedantic)
	endif()
	target_include_directories(
		goldRenderBackendTests
		PUBLIC
			"include"
			"include/game"
			"tests"
	)
	target_link_libraries(
		goldRenderBackendTests
		PRIVATE
			gold::game
	)
	target_compile_features(
		goldRenderBackendTests
		PRIVATE
			cxx_std_26
	)
	add_test(NAME goldRenderBackendTests COMMAND goldRenderBackendTests)
endif()

if(GOLD_BUILD_WEB)
	add_executable(
		goldWebTests
		tests/webTest.cpp
	)
	target_include_directories(
		goldWebTests PRIVATE "include" "include/web" "tests")
	target_link_libraries(goldWebTests PRIVATE gold::web)
	target_compile_features(goldWebTests PRIVATE cxx_std_26)
	add_test(NAME goldWebTests COMMAND goldWebTests)

	add_executable(
		goldWebPersistenceTests
		tests/webPersistenceTest.cpp
	)
	target_include_directories(
		goldWebPersistenceTests PRIVATE "include" "include/web" "tests")
	target_link_libraries(goldWebPersistenceTests PRIVATE gold::web)
	target_compile_features(goldWebPersistenceTests PRIVATE cxx_std_26)
	add_test(NAME goldWebPersistenceTests COMMAND goldWebPersistenceTests)

	add_executable(
		goldServerTests
		tests/serverTest.cpp
	)
	if(MSVC)
		target_compile_options(goldServerTests PRIVATE /W4)
	else()
		target_compile_options(goldServerTests PRIVATE -Wall -Wextra -pedantic)
	endif()
	target_include_directories(goldServerTests PRIVATE "include" "include/web" "tests")
	target_link_libraries(goldServerTests PRIVATE gold::web)
	target_compile_features(goldServerTests PRIVATE cxx_std_26)
	add_test(NAME goldServerTests COMMAND goldServerTests)
endif()

if(GOLD_BUILD_UI AND GOLD_BUILD_WEB)
	add_executable(
		goldUITests
		tests/uiTest.cpp
	)
	if(MSVC)
		target_compile_options(goldUITests PRIVATE /W4 -Wno-unused-function -Wno-unused-variable)
	else()
		target_compile_options(goldUITests PRIVATE -Wall -Wextra -pedantic)
	endif()
	target_include_directories(goldUITests PRIVATE "include" "include/ui" "tests")
	# The ui module uses the web module's HTML/CSS parsers internally but
	# links them privately; tests that touch elements need web explicitly.
	target_link_libraries(goldUITests PRIVATE gold::ui gold::web)
	target_compile_features(goldUITests PRIVATE cxx_std_26)
	add_test(NAME goldUITests COMMAND goldUITests)
endif()
