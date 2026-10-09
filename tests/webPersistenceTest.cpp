#include <atomic>
#include <filesystem>
#include <thread>
#include <vector>

#include "goldtest.hpp"
#include "goldjs.hpp"
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

TEST(file_data_store_concurrent_reads_and_writes) {
	const auto root = std::filesystem::temp_directory_path() /
		"gold_data_store_concurrent";
	std::error_code ec;
	std::filesystem::remove_all(root, ec);

	auto store = createDataStore("file");
	EXPECT_TRUE(store != nullptr);
	if (!store) return;
	EXPECT_TRUE(store->open("testdb", root.string()));
	EXPECT_TRUE(store->createCollection("items"));

	// Seed a known set of documents.
	const int kSeed = 50;
	for (int i = 0; i < kSeed; ++i) {
		auto doc = jo("_id", "seed" + std::to_string(i), "v", i);
		store->insert("items", doc);
	}

	// Concurrent readers (find) + writers (insert) on the same store.
	const int kThreads = 8;
	const int kPerThread = 50;
	std::atomic<int> errors{0};

	std::vector<std::thread> threads;
	for (int t = 0; t < kThreads; ++t) {
		threads.emplace_back([&, t]() {
			for (int i = 0; i < kPerThread; ++i) {
				// Concurrent read: every seeded doc must be findable.
				auto found = store->findOne("items",
					jo("_id", "seed" + std::to_string(i % kSeed)));
				if (found.getString("_id").empty()) ++errors;
				// Concurrent write: distinct ids per thread.
				auto id = "t" + std::to_string(t) + "_" + std::to_string(i);
				auto r = store->insert("items", jo("_id", id, "v", i));
				if (r.isError()) ++errors;
			}
		});
	}
	for (auto& th : threads) th.join();

	// All seeded + all written docs must be present.
	auto all = store->find("items", jo(), 0);
	EXPECT_EQ(all.size(), (uint64_t)(kSeed + kThreads * kPerThread));
	EXPECT_EQ(errors.load(), 0);

	delete store;
	std::filesystem::remove_all(root, ec);
}

int main() {
	return goldtest::runAll();
}

TEST(file_data_store_filter_operators) {
	const auto root = std::filesystem::temp_directory_path() /
		"gold_data_store_filter_ops";
	std::error_code ec;
	std::filesystem::remove_all(root, ec);

	auto store = createDataStore("file");
	EXPECT_TRUE(store != nullptr);
	if (!store) return;
	EXPECT_TRUE(store->open("testdb", root.string()));
	EXPECT_TRUE(store->createCollection("ops"));

	store->insert("ops", jo("_id", "alice", "age", 30,
		"tags", ja("admin", "dev")));
	store->insert("ops", jo("_id", "bob", "age", 25, "tags", ja("admin")));
	store->insert("ops", jo("_id", "carol", "age", 35));
	store->insert("ops", jo("_id", "dave", "age", 40, "tags", ja("ops")));

	// Scalar comparisons through $gt/$lte.
	EXPECT_EQ(store->find("ops", jo("age", jo("$gt", 29)), 0).size(),
		(size_t)3);
	EXPECT_EQ(store->find("ops", jo("age", jo("$lte", 25)), 0).size(),
		(size_t)1);
	// Multiple operators on one field AND together.
	EXPECT_EQ(store->find("ops",
		jo("age", jo("$gt", 29, "$lt", 40)), 0).size(), (size_t)2);
	// $ne matches absent fields (MongoDB convention) and values differ.
	EXPECT_EQ(store->find("ops", jo("name", jo("$ne", "alice")), 0).size(),
		(size_t)4);
	EXPECT_EQ(store->find("ops", jo("age", jo("$ne", 25)), 0).size(),
		(size_t)3);
	// $in works on list fields (intersection) and scalars (membership).
	EXPECT_EQ(store->find("ops", jo("tags", jo("$in", ja("admin"))), 0)
								.size(),
		(size_t)2);
	EXPECT_EQ(store->find("ops", jo("_id", jo("$in", ja("alice", "bob"))),
		0).size(), (size_t)2);
	// $nin: list fields miss the wanted set; absent fields match.
	EXPECT_EQ(store->find("ops", jo("tags", jo("$nin", ja("admin"))), 0)
								.size(),
		(size_t)2);
	// $exists on both sides.
	EXPECT_EQ(store->find("ops", jo("tags", jo("$exists", true)), 0)
								.size(),
		(size_t)3);
	EXPECT_EQ(store->find("ops", jo("tags", jo("$exists", false)), 0)
								.size(),
		(size_t)1);

	// Unknown operators fail closed rather than matching anything.
	EXPECT_EQ(store->find("ops", jo("age", jo("$mystery", 30)), 0).size(),
		(size_t)0);

	delete store;
	std::filesystem::remove_all(root, ec);
}

TEST(file_data_store_write_failures_report_errors) {
	const auto root = std::filesystem::temp_directory_path() /
		"gold_data_store_write_fail";
	std::error_code ec;
	std::filesystem::remove_all(root, ec);

	auto store = createDataStore("file");
	EXPECT_TRUE(store != nullptr);
	if (!store) return;
	EXPECT_TRUE(store->open("testdb", root.string()));
	EXPECT_TRUE(store->createCollection("w"));

	// A document whose `_id` is an existing DIRECTORY would make the
	// atomic rename of the temp file fail; the insert must surface it
	// rather than reporting success.
	std::filesystem::create_directories(root / "testdb" / "w" / "b.json");
	auto insertErr = store->insert("w", jo("_id", "b", "v", 1));
	EXPECT_TRUE(insertErr.isError());

	delete store;
	std::filesystem::remove_all(root, ec);
}
