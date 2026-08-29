#include "utility.hpp"

#include <random>

namespace gg {
	using namespace std;
	string randomString(size_t length) {
		// TODO: Found on SO, replace
		const string CHARACTERS =
			"0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrst"
			"uvwxyz";

		random_device random_device;
		mt19937 generator(random_device());
		uniform_int_distribution<> distribution(
			0, CHARACTERS.size() - 1);

		string random_string;

		for (size_t i = 0; i < length; ++i) {
			random_string += CHARACTERS[distribution(generator)];
		}

		return random_string;
	}
}