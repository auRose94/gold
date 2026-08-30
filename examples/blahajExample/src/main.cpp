#include <iostream>
#include <string>

#include <mesh.hpp>

using namespace gold;

int main(int argc, char** argv) {
	const std::string defaultPath =
		"./assets/models/props/Blahaj/"
		"Blahaj_Low_poly_blahaj1_Low_poly_blahaj1.gltf";
	const auto modelPath = argc > 1 ? std::string(argv[1]) : defaultPath;
	mesh model{path(modelPath)};
	auto error = model.getString("error");
	if (!error.empty()) {
		std::cerr << "Failed to load " << modelPath << ": " << error << '\n';
		return 1;
	}

	std::cout << "Loaded: " << modelPath << '\n'
		<< "  buffers: " << model.getList("buffers").size() << '\n'
		<< "  accessors: " << model.getList("accessors").size() << '\n'
		<< "  meshes: " << model.getList("meshes").size() << '\n'
		<< "  materials: " << model.getList("materials").size() << '\n'
		<< "  images: " << model.getList("images").size() << '\n';
	return 0;
}
