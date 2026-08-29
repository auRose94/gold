#include "worker.hpp"

#include <mutex>
#include <thread>
#include <chrono>
#include <iostream>

namespace gold {
	using namespace std;
	struct worker::job {
	 public:
		const method m;
		obj o;
		const list a;
		job(const method& me, obj& ob, const list& ar) : m(me), o(ob), a(ar) {}
		job(const job& copy) : m(copy.m), o(copy.o), a(copy.a) {}
		~job() {}
		void call() {
			try {
				if (o && m && a) {
					auto ret = (o.*m)(a);
					if (ret.isError()) cerr << ret << endl;
				}
			} catch (genericError e) {
				cerr << e << endl;
			}
		}
	};

	int worker::workerProcess(worker* w) {
		do {
			auto job = w->nextJob();
			if (job) job->call();
		} while (!w->shouldDie());
		return 0;
	}

	worker::jobPtr worker::nextJob() {
		if (mtx.try_lock()) {
			auto it = jobs.begin();
			if (it != jobs.end()) {
				auto j = it.operator*();
				jobs.erase(it);
				mtx.unlock();
				if (j) return j;
			}
			mtx.unlock();
		}
		return jobPtr();
	}

	bool worker::shouldDie() { return this->kill == true; }

	worker::worker() : jobs(), threads(), kill(false) {
		// useAll();
	}

	worker::jobPtr worker::add(const method& m, obj& o, const list& args) {
		auto j = make_shared<job>(m, o, args);
		mtx.lock();
		jobs.push_back(j);
		mtx.unlock();
		return j;
	}

	void worker::wait() {
		if (threads.empty()) {
			while (auto job = nextJob()) job->call();
			return;
		}
		for (;;) {
			{
				lock_guard<mutex> guard(mtx);
				if (jobs.empty()) return;
			}
			this_thread::sleep_for(chrono::milliseconds(1));
		}
	}

	void worker::killAll() {
		auto cores = thread::hardware_concurrency();
		if (cores != 0 && threads.size() >= cores) {
			kill = true;
			for (auto it = threads.begin(); it != threads.end(); ++it)
				it->join();
			kill = false;
		}
	}

	void worker::useAll() {
		for (uint32_t i = 0; i < thread::hardware_concurrency();
				 ++i)
			threads.push_back(thread(workerProcess, this));
	}

	void worker::clear() {
		mtx.lock();
		jobs.clear();
		mtx.unlock();
	}
}  // namespace gold
