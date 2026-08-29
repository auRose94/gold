#include "file.hpp"
#include "goldtest.hpp"

using namespace gold;

TEST(file_decode_percent_escape) {
	std::string url = "data:text/plain,%48%65%6c%6c%6f%20%57%6f%72%6c%64";
	std::string mime;
	binary bin = file::decodeDataURL(url, mime);
	EXPECT_EQ(mime, "text/plain");
	EXPECT_EQ(bin.size(), 11u);
	std::string s((char*)bin.data(), bin.size());
	EXPECT_EQ(s, "Hello World");
}
