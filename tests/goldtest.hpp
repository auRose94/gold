#pragma once

#include <cmath>
#include <cstdint>
#include <functional>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace goldtest {

	inline int& failures() {
		static int count = 0;
		return count;
	}

	inline int& checks() {
		static int count = 0;
		return count;
	}

	struct failure {
		std::string expr;
		std::string file;
		int line;
		std::string msg;
	};

	inline std::vector<failure>& failureList() {
		static std::vector<failure> list;
		return list;
	}

	inline void record(bool ok, const std::string& expr,
		const char* file, int line, const std::string& msg = "") {
		checks()++;
		if (!ok) {
			failures()++;
			failureList().push_back({expr, file, line, msg});
			std::cout << "FAIL " << file << ":" << line << "  " << expr;
			if (!msg.empty()) std::cout << "  [" << msg << "]";
			std::cout << "\n";
		}
	}

	template <typename A, typename B>
	bool eq(const A& a, const B& b) {
		return a == b;
	}

	template <typename A, typename B>
	bool neq(const A& a, const B& b) {
		return a != b;
	}

	inline bool nearlyEq(double a, double b, double eps = 1e-6) {
		return std::fabs(a - b) <= eps;
	}

	struct test_case {
		const char* name;
		std::function<void()> fn;
	};

	inline std::vector<test_case>& registry() {
		static std::vector<test_case> tests;
		return tests;
	}

	inline int registerTest(const char* name, std::function<void()> fn) {
		registry().push_back({name, fn});
		return 0;
	}

	inline int runAll() {
		int run = 0;
		for (auto& t : registry()) {
			int before = failures();
			std::cout << "== " << t.name << "\n";
			t.fn();
			if (failures() == before)
				std::cout << "   ok\n";
			run++;
		}
		std::cout << "\n" << run << " test groups, " << checks()
							<< " checks, " << failures() << " failures\n";
		return failures() == 0 ? 0 : 1;
	}

}  // namespace goldtest

#define TEST(name)                                            \
	static void __goldtest_run_##name();                      \
	static int goldtest_reg_##name =                          \
		goldtest::registerTest(#name, []() -> void {          \
			__goldtest_run_##name();                          \
		});                                                   \
	static void __goldtest_run_##name()

#define EXPECT(expr) \
	goldtest::record(static_cast<bool>(expr), #expr, __FILE__, __LINE__)

#define EXPECT_EQ(a, b) \
	goldtest::record(goldtest::eq((a), (b)), #a " == " #b, __FILE__, __LINE__)

#define EXPECT_NE(a, b) \
	goldtest::record(goldtest::neq((a), (b)), #a " != " #b, __FILE__, __LINE__)

#define EXPECT_NEAR(a, b, eps) \
	goldtest::record(goldtest::nearlyEq((a), (b), (eps)), \
		#a " ~= " #b, __FILE__, __LINE__)

#define EXPECT_TRUE(expr) EXPECT(expr)
#define EXPECT_FALSE(expr) EXPECT(!(expr))