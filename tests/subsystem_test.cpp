#include "file.hpp"
#include "goldtest.hpp"
#include "module.hpp"
#include "promise.hpp"
#include "worker.hpp"

#include <atomic>
#include <thread>
#include <vector>

using namespace gold;

TEST(worker_drains_and_clears_jobs) {
	worker jobs;
	object target;
	method noOp = nullptr;

	// A worker without threads drains its queue synchronously. A null method
	// is a valid no-op job and should not crash the worker.
	auto first = jobs.add(noOp, target, list());
	EXPECT_TRUE((bool)first);
	jobs.wait();

	jobs.add(noOp, target, list());
	jobs.add(noOp, target, list());
	jobs.clear();
	jobs.wait();
}

TEST(promise_add_args_and_invalid_call) {
	auto callback = func([](list args) -> var {
		return var(args.getVar(1).getInt64() + args.getVar(2).getInt64());
	});
	promise task(object(), callback, list({int64_t(20), int64_t(22)}));

	EXPECT_EQ(task.call().getInt64(), 42);
	EXPECT_TRUE((bool)task);

	promise argsTask(object(), callback, list());
	auto added = argsTask.addArgs(list({int64_t(20), int64_t(22)}));
	EXPECT_EQ(added.getList().getVar(0).getInt64(), 20);

	promise invalid;
	EXPECT_TRUE(invalid.call().isEmpty());
	EXPECT_TRUE((bool)invalid);
}

TEST(promise_async_function_execution) {
	promise::useAllCores();
	auto callback = func([](list args) -> var {
		return var(args.getInt64(1) * 2);
	});
	promise task(object(), callback, list({int64_t(21)}));

	EXPECT_EQ(task.await().getInt64(), 42);
	EXPECT_TRUE((bool)task);
	promise::joinThreads();
}

TEST(module_loader_rejects_invalid_names_and_paths) {
	module::setLibraryPath("/tmp/gold-module-path-that-does-not-exist");
	EXPECT_FALSE(module::load(""));
	EXPECT_FALSE(module::load("../escape"));
	EXPECT_FALSE(module::isLoaded(""));
}

TEST(malformed_inputs_are_errors) {
	EXPECT_TRUE(file::parseJSON("").isError());
	EXPECT_TRUE(file::parseJSON("[").isError());

	binary truncated = {0x81};
	auto bytes = string_view((char*)truncated.data(), truncated.size());
	EXPECT_TRUE(file::parseBSON(bytes).isError());
	EXPECT_TRUE(file::parseCBOR(bytes).isError());
	EXPECT_TRUE(file::parseMsgPack(bytes).isError());
	EXPECT_TRUE(file::parseUBJSON(bytes).isError());
}

TEST(concurrent_object_reads_and_writes) {
	// Hammer a shared object from many threads: concurrent readers must not
	// block each other (shared_mutex), and writers must not corrupt state.
	object shared;
	shared.setInt64("counter", 0);
	shared.setString("name", "gold");

	const int kThreads = 8;
	const int kIters = 2000;
	std::atomic<int> errors{0};

	std::vector<std::thread> threads;
	for (int t = 0; t < kThreads; ++t) {
		threads.emplace_back([&, t]() {
			for (int i = 0; i < kIters; ++i) {
				// Concurrent reads.
				if (shared.getInt64("counter") < 0) ++errors;
				if (shared.getString("name") != "gold") ++errors;
				// Concurrent writes to distinct keys.
				shared.setInt64("t" + std::to_string(t), i);
				// Read back what we wrote.
				if (shared.getInt64("t" + std::to_string(t)) != i) ++errors;
			}
		});
	}
	for (auto& th : threads) th.join();

	EXPECT_EQ(errors.load(), 0);
	EXPECT_EQ(shared.getInt64("counter"), 0);
	EXPECT_EQ(shared.getString("name"), "gold");
}

TEST(concurrent_list_reads_and_writes) {
	// Concurrent push + indexed read on a shared list.
	list shared;
	const int kThreads = 8;
	const int kPerThread = 1000;
	std::atomic<int> errors{0};

	std::vector<std::thread> threads;
	for (int t = 0; t < kThreads; ++t) {
		threads.emplace_back([&, t]() {
			for (int i = 0; i < kPerThread; ++i) {
				shared.pushInt64(t * kPerThread + i);
			}
		});
	}
	for (auto& th : threads) th.join();

	EXPECT_EQ(shared.size(), (uint64_t)(kThreads * kPerThread));

	// Verify every element is present exactly once.
	std::vector<int> seen(kThreads * kPerThread, 0);
	for (uint64_t i = 0; i < shared.size(); ++i) {
		auto v = shared.getInt64(i);
		if (v < 0 || v >= (int64_t)seen.size()) { ++errors; continue; }
		seen[v]++;
	}
	for (auto c : seen)
		if (c != 1) ++errors;

	EXPECT_EQ(errors.load(), 0);
}

TEST(concurrent_object_erase_and_owns) {
	// Concurrent erase/owns on a shared object must not crash or corrupt.
	// Each thread uses its own key so there is no cross-thread ordering
	// dependency; we verify set->owns->erase->!owns is consistent per key.
	object shared;
	const int kThreads = 8;
	const int kIters = 2000;
	std::atomic<int> errors{0};

	std::vector<std::thread> threads;
	for (int t = 0; t < kThreads; ++t) {
		threads.emplace_back([&, t]() {
			auto key = "k" + std::to_string(t);
			for (int i = 0; i < kIters; ++i) {
				shared.setInt64(key, i);
				if (!shared.owns(key)) ++errors;
				shared.erase(key);
				if (shared.owns(key)) ++errors;
			}
		});
	}
	for (auto& th : threads) th.join();

	EXPECT_EQ(errors.load(), 0);
}

int main() {
	return goldtest::runAll();
}
