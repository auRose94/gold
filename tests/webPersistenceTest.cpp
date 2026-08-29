#include <filesystem>

#include "goldtest.hpp"
#include "web/dataStore.hpp"

using namespace gold;

TEST(file_data_store_rejects_unsafe_path_components) {
	const auto root = std::filesystem::temp_directory_path() /
		"gold_data_store_persistence_names";
	std::error_code ec;
	std::filesystem::remove_all(root, ec);

	auto store = createDataStore("file");
	EXPECT_TRUE(store != nullptr);
	if (!store) return;

	EXPECT_FALSE(store->open("../outside", root.string()));
	EXPECT_FALSE(std::filesystem::exists(root));
	EXPECT_TRUE(store->open("testdb", root.string()));
	EXPECT_FALSE(store->createCollection("../outside"));
	EXPECT_FALSE(store->createCollection("nested/users"));
	EXPECT_FALSE(std::filesystem::exists(root.parent_path() / "outside"));
	EXPECT_TRUE(store->createCollection("users"));

	delete store;
	std::filesystem::remove_all(root, ec);
}

int main() {
	return goldtest::runAll();
}
