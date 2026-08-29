#pragma once

#include "types.hpp"

namespace gold {
	class promise : public object {
	 protected:
		static object& getPrototype();
		static int worker();

	 public:
	 	static void useAllCores();
		static void joinThreads();
		promise();
		promise(const promise& copy);
		promise(promise&& move);
		promise(object self, method m, list args);
		promise(object self, func f, list args);
		~promise();
		promise& operator=(const promise& rhs);
		promise& operator=(promise&& rhs);

		var addArgs(list args);
		var call(list args = {});
		var await(list args = {});

		operator bool();
	};
}  // namespace gold