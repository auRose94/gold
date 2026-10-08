#include "promise.hpp"

#include <iostream>
#include <mutex>
#include <thread>

namespace gold {
	using namespace std;

	static const uint64_t threadSleep = 60;
	static mutex promisesMutex;
	static mutex shouldQuitMutex;
	static mutex printMutex;
	static bool shouldQuit = false;
	static vector<promise> promises;
	static vector<thread> threads;

	object& promise::getPrototype() {
		static auto proto = obj({
			{"self", var()},
			{"method", var()},
			{"response", var()},
			{"status", 0},
			{"addArgs", method(&promise::addArgs)},
			{"call", method(&promise::call)},
			{"await", method(&promise::await)},
		});
		return proto;
	}

	bool shouldContinue() {
		unique_lock<mutex> guard(shouldQuitMutex);
		auto keepWorking = !shouldQuit;
		return keepWorking;
	}

	bool getPromise(promise& job) {
		unique_lock<mutex> guard(promisesMutex);
		auto it = promises.begin();
		if (it != promises.end()) {
			job = *it;
			promises.erase(it);
			return true;
		}
		return false;
	}

	void pushPromise(promise& job) {
		unique_lock<mutex> guard(promisesMutex);
		promises.push_back(job);
	}

	void printError(string msg) {
		unique_lock<mutex> guard(printMutex);
		cerr << msg << endl;
	}

	bool singleThread() {
		auto size = threads.size();
		return size == 0;
	}

	int promise::worker() {
		auto job = promise();
		try {
			while (shouldContinue()) {
				try {
					if (getPromise(job)) job.call();
					job = promise();
					this_thread::sleep_for(
						std::chrono::microseconds(threadSleep)
																	 );
				} catch (const exception& e) {
					printError(e.what());
				} catch (...) {
					printError("unknown exception in promise worker");
				}
			}
		} catch (const exception& e) {
			printError(e.what());
			return -1;
		} catch (...) {
			printError("unknown exception in promise worker");
			return -1;
		}

		return 0;
	}

	void promise::useAllCores() {
		for (uint32_t i = 0; i < thread::hardware_concurrency();
				 ++i)
			threads.push_back(thread(worker));
	}

	void promise::joinThreads() {
		{
			lock_guard<mutex> guard(shouldQuitMutex);
			shouldQuit = true;
		}
		for (auto it = threads.begin(); it != threads.end(); ++it)
			it->join();
		threads.clear();
		lock_guard<mutex> guard(shouldQuitMutex);
		shouldQuit = false;
	}

	promise::promise() : object() {}

	promise::promise(const promise& copy) : object(copy) {
		setParent(getPrototype());
	}

	promise::promise(object self, method m, list args)
		: object() {
		setParent(getPrototype());
		setObject("self", self);
		setMethod("method", m);
		setList("args", args);
		if (!singleThread()) pushPromise(*this);
	}

	promise::promise(object self, func f, list args) : object() {
		setParent(getPrototype());
		setObject("self", self);
		setFunc("func", f);
		setList("args", args);
		if (!singleThread()) pushPromise(*this);
	}

	promise::~promise() {
		if (data && data.use_count() == 1) {
			empty();
		}
		data = nullptr;
	}

	promise& promise::operator=(const promise& rhs) {
		object::operator=(rhs);
		return *this;
	}

	promise::promise(promise&& move) : object(std::move(move)) {}

	promise& promise::operator=(promise&& rhs) {
		object::operator=(std::move(rhs));
		return *this;
	}

	var promise::addArgs(list args) {
		auto cArgs = list();
		assignList("args", cArgs);
		cArgs += args;
		return cArgs;
	}

	var promise::call(list) {
		// Every path must leave `status` settled: a job that throws (or a
		// promise with no callback) would otherwise stay at 0 forever and
		// spin any thread sitting in `await()`.
		var resp;
		try {
			if (getType("method") == typeMethod) {
				auto obj = getObject("self");
				auto args = getList("args");
				auto m = getMethod("method");
				resp = (obj.*m)(args);
			} else if (getType("func") == typeFunction) {
				auto obj = getObject("self");
				auto args = getList("args");
				auto f = getFunc("func");
				auto apArgs = list({obj});
				apArgs += args;  // obj, args...
				resp = (f)(apArgs);
			} else {
				// No callback: failed, not successful.
				setInt8("status", -1);
				return var();
			}
		} catch (const genericError& e) {
			setVar("response", var(e));
			setInt8("status", -1);
			return var();
		} catch (const exception& e) {
			setVar("response", genericError(e.what()));
			setInt8("status", -1);
			return var();
		} catch (...) {
			setVar("response", genericError("unknown exception in promise job"));
			setInt8("status", -1);
			return var();
		}
		erase("method");
		erase("func");
		erase("self");
		erase("args");
		setVar("response", resp);
		setInt8("status", 1);
		return resp;
	}

	var promise::await(list) {
		if (singleThread()) {
			return call();
		} else {
			auto status = getInt8("status");
			while (status == 0) {
				this_thread::sleep_for(
					std::chrono::microseconds(threadSleep));
				status = getInt8("status");
			}
			return getVar("response");
		}
	}

	promise::operator bool() {
		auto status = getInt8("status");
		return status != 0;
	}
}  // namespace gold