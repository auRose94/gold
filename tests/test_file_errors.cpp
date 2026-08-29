#include "file.hpp"
#include "goldtest.hpp"

using namespace gold;

TEST(file_load_reports_missing_file) {
	file missing(path("/tmp/gold-file-that-does-not-exist"));
	auto result = missing.load();
	EXPECT_TRUE(result.isError());
	EXPECT_EQ(result.getError()->getString("msg"), "file does not exist");
}

TEST(file_trash_reports_missing_file) {
	file missing(path("/tmp/gold-file-that-does-not-exist"));
	auto result = missing.trash();
	EXPECT_TRUE(result.isError());
	EXPECT_EQ(result.getError()->getString("msg"), "file does not exist");
}

TEST(file_save_reports_unopenable_path) {
	file output(string_view("data"));
	auto result = output.save({"/tmp"});
	EXPECT_TRUE(result.isError());
}

int main() {
	return goldtest::runAll();
}
