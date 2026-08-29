enable_testing()

add_executable(
	goldTests
	tests/test.cpp
	tests/test_file_decode.cpp
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
endif()
