#include <filesystem>

#include "goldtest.hpp"
#include "web/database.hpp"
#include "web/dataStore.hpp"

using namespace gold;

TEST(file_data_store_crud) {
	const auto root = std::filesystem::temp_directory_path() /
		"gold_data_store_test";
	std::error_code ec;
	std::filesystem::remove_all(root, ec);

	auto store = createDataStore("file");
	EXPECT_TRUE(store != nullptr);
	if (!store) return;
	EXPECT_TRUE(store->open("testdb", root.string()));
	EXPECT_TRUE(store->createCollection("users"));

	auto inserted = store->insert("users", object({
		{"name", "alice"},
		{"tags", list({"admin", "user"})},
	}));
	EXPECT_TRUE(inserted.isObject());
	if (inserted.isObject()) {
		auto id = inserted.getObject().getString("_id");
		EXPECT_EQ(id.size(), (size_t)24);
		auto found = store->findOne("users", object({{"name", "alice"}}));
		EXPECT_EQ(found.getString("_id"), id);

		auto updated = store->updateOne("users", object({{"_id", id}}),
			object({{"$set", object({{"name", "bob"}})}}));
		EXPECT_EQ(updated.getObject().getString("name"), "bob");

		auto in = store->find("users", object({
			{"tags", object({{"$in", list({"admin"})}})},
		}));
		EXPECT_EQ(in.size(), (uint64_t)1);
		auto deleted = store->deleteOne("users", object({{"_id", id}}));
		EXPECT_EQ(deleted.getObject().getUInt64("deletedCount"), (uint64_t)1);
		EXPECT_FALSE(store->findOne("users", object({{"_id", id}})));
	}

	delete store;
	std::filesystem::remove_all(root, ec);
}

TEST(database_facade_rejects_missing_arguments) {
	database db({{"backend", "file"}, {"name", "test"},
		{"path", std::filesystem::temp_directory_path().string()}});
	EXPECT_TRUE(db.createCollection(list()).isError());
	EXPECT_TRUE(db.getCollection(list()).isError());
}

int main() {
	return goldtest::runAll();
}
