#include "game/mesh.hpp"
#include "goldtest.hpp"

#include <filesystem>
#include <fstream>

using namespace gold;

TEST(gltf_external_uri_is_contained_by_asset_directory) {
	auto root = std::filesystem::temp_directory_path() /
		"gold_gltf_uri_validation_test";
	std::error_code ec;
	std::filesystem::remove_all(root, ec);
	std::filesystem::create_directories(root / "nested");
	{
		std::ofstream outside(root.parent_path() / "gold_gltf_escape.bin",
			std::ios::binary);
		outside.put('x');
	}
	{
		std::ofstream scene(root / "traversal.gltf");
		scene << R"({"buffers":[{"uri":"../gold_gltf_escape.bin","byteLength":1}]})";
	}
	mesh traversal(root / "traversal.gltf");
	EXPECT_TRUE(traversal.getString("error").find("Invalid external glTF buffer URI") !=
		string::npos);

	{
		std::ofstream data(root / "nested" / "data.bin", std::ios::binary);
		data.put('x');
	}
	{
		std::ofstream scene(root / "nested.gltf");
		scene << R"({"buffers":[{"uri":"nested/data.bin","byteLength":1}]})";
	}
	mesh nested(root / "nested.gltf");
	EXPECT_EQ(nested.getString("error"), "");
	EXPECT_EQ(nested.getList("buffers").getObject(0).getBinary("data").size(),
		(size_t)1);

	std::filesystem::remove_all(root, ec);
	std::filesystem::remove(root.parent_path() / "gold_gltf_escape.bin", ec);
}

TEST(gltf_external_image_uri_is_contained_by_asset_directory) {
	auto root = std::filesystem::temp_directory_path() /
		"gold_gltf_image_uri_validation_test";
	std::error_code ec;
	std::filesystem::remove_all(root, ec);
	std::filesystem::create_directories(root);
	{
		std::ofstream outside(root.parent_path() / "gold_gltf_image_escape.bin",
			std::ios::binary);
		outside.put('x');
	}
	{
		std::ofstream scene(root / "scene.gltf");
		scene << R"({"images":[{"uri":"../gold_gltf_image_escape.bin"}]})";
	}
	mesh invalid(root / "scene.gltf");
	EXPECT_TRUE(invalid.getString("error").find("Invalid external glTF image URI") !=
		string::npos);
	std::filesystem::remove_all(root, ec);
	std::filesystem::remove(root.parent_path() / "gold_gltf_image_escape.bin", ec);
}
