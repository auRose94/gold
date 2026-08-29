#include "file.hpp"
#include "goldtest.hpp"
#include "module.hpp"
#include "promise.hpp"
#include "worker.hpp"

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

int main() {
	return goldtest::runAll();
}
