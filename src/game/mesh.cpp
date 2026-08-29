#include "mesh.hpp"

#include <bx/math.h>
#include <bx/timer.h>
#include <string.h>

#include <fstream>
#include <filesystem>

#include "graphics.hpp"

namespace gold {
	using namespace std;
	object& mesh::getPrototype() {
		static auto proto = obj{
			{"getVertexLayoutHandle",
			 method(&mesh::getVertexLayoutHandle)},
			{"getVertexBufferHandle",
			 method(&mesh::getVertexBufferHandle)},
			{"getIndexBufferHandle",
			 method(&mesh::getIndexBufferHandle)},
			{"getTrianglesFromMesh",
			 method(&mesh::getTrianglesFromMesh)},
			{"destroy", method(&mesh::destroy)},
			{"proto", file::getPrototype()},
		};
		return proto;
	}

	mesh::mesh() : file() {}

	mesh::mesh(path meshFilePath) : mesh(file(meshFilePath)) {}

	mesh::mesh(file meshFile) : file(meshFile) {
		load();
		auto ext = extension().getString();
		if (ext.find("gltf") != string::npos) {
			auto o = object(asJSON());
			if (o) copy(o);
			auto buffers = getList("buffers");
			auto accessors = getList("accessors");
			auto bufferViews = getList("bufferViews");
			auto nodes = getList("nodes");
			auto meshes = getList("meshes");
			for (auto it = buffers.begin(); it != buffers.end();
					 ++it) {
					auto bufferObj = it->getObject();
					if (bufferObj.getType("uri") == typeString) {
						auto uri = bufferObj.getString("uri");
						auto byteLength = bufferObj.getUInt64("byteLength");
						binary bin;
						if (uri.rfind("data:", 0) == 0) {
							auto type = string();
							bin = file::decodeDataURL(uri, type);
						} else {
							auto base = filesystem::path(getString("path"))
								.parent_path();
							auto external = file::readFile(base / uri);
							if (external.isError()) {
								setString("error", "Failed to load external glTF buffer: " + uri);
								return;
							}
							bin = external.getObject<file>().getBinary("data");
							if (bin.empty() && byteLength != 0) {
								setString("error", "Failed to load external glTF buffer: " + uri);
								return;
							}
						}
						if (bin.empty() || (byteLength != 0 && bin.size() < byteLength)) {
							setString("error", "Invalid glTF buffer data: " + uri);
							return;
						}
						bufferObj.setBinary("data", bin);
						bufferObj.erase("uri");
				}
			}
			auto bufferLists = list();
			for (auto it = accessors.begin(); it != accessors.end();
					 ++it) {
				auto section = it->getObject();
				auto view = bufferViews.getObject(
					section.getUInt64("bufferView"));
				auto bufferObj =
					buffers.getObject(section.getUInt64("buffer"));
				auto offset = view.getUInt64("byteOffset") +
					section.getUInt64("byteOffset");
				auto scalarType = section.getUInt32("componentType");
				auto count = section.getUInt64("count");
				auto type = section.getString("type");
				auto normalized = section.getBool("normalized", false);
				auto data = bufferObj.getBinary("data");
				auto parsed = list();
				parseGLTFBuffer(
					type, scalarType, offset, count, data.data(), parsed);
				if (normalized) {
					auto scale = scalarType == 5121 ? 255.0
						: scalarType == 5123 ? 65535.0
						: scalarType == 5125 ? 4294967295.0
						: scalarType == 5120 ? 127.0
						: scalarType == 5122 ? 32767.0 : 1.0;
					auto normalize = [&](var value) -> var {
						if (value.isVec2())
							return vec2f(float(value.getDouble(0) / scale),
								float(value.getDouble(1) / scale));
						if (value.isVec3())
							return vec3f(float(value.getDouble(0) / scale),
								float(value.getDouble(1) / scale),
								float(value.getDouble(2) / scale));
						if (value.isVec4())
							return vec4f(float(value.getDouble(0) / scale),
								float(value.getDouble(1) / scale),
								float(value.getDouble(2) / scale),
								float(value.getDouble(3) / scale));
						return float(value.getDouble() / scale);
				};
					auto normalizedValues = list();
					for (auto value : parsed) normalizedValues.pushVar(normalize(value));
					parsed = normalizedValues;
				}
				section.setList("parsed", parsed);
			}
			for (auto it = nodes.begin(); it != nodes.end(); ++it) {
				auto node = it->getObject();
				auto mesh = meshes.getObject(node.getUInt64("mesh"));
				if (mesh) {
					auto primitives = mesh.getList("primitives");
					for (auto pi = primitives.begin();
							 pi != primitives.end();
							 ++pi) {
						auto primitive = pi->getObject();
					}
				}
			}
		}
	}

	var mesh::getNode(list args) {
		auto name = args.getString(0);
		auto nodes = getList("nodes");
		auto node = object();
		for (auto it = nodes.begin(); it != nodes.end(); ++it) {
			auto n = it->getObject();
			if (n.getString("name").compare(name) == 0) node = n;
		}
		return node;
	}

	var mesh::getPrimitiveCount(list args) {
		auto name = args.getString(0);
		obj node, meshObj;
		list meshes, primitives;
		if (
			(node = getNode({name}).getObject()) &&
			(meshes = getList("meshes")) &&
			(meshObj = meshes.getObject(node.getUInt64("mesh"))) &&
			(primitives = meshObj.getList("primitives")))
			return primitives.size();
		return 0;
	}

	var mesh::getVertexLayoutHandle(list args) {
		auto name = args.getString(0);
		auto pri = args.getUInt64(1);
		auto nodes = getList("nodes");
		auto meshes = getList("meshes");
		auto accessors = getList("accessors");
		auto node = getNode({name}).getObject();
		if (node) {
			auto meshObj = meshes.getObject(node.getUInt64("mesh"));
			auto primitives = meshObj.getList("primitives");
			if (pri >= primitives.size())
				return genericError("Primitive index out of range");
			auto it = primitives.begin();
			advance(it, pri);
			if (it != primitives.end()) {
				auto primitive = it->getObject();
				if (!primitive)
					return genericError("No primitive object");
				auto attributes = primitive.getObject("attributes");
				if (!attributes)
					return genericError("No attributes object");
				if (attributes.size() == 0)
					return genericError("No attributes");
				auto layout = vertexLayout();
				primitive.setObject("vertexLayout", layout);
				layout.begin();

				using attrib = vertexLayout::attrib;
				using attribT = vertexLayout::attribType;
				auto addAttribute = [&](string name, attrib e) {
					auto index = var();
					if ((index = attributes.getVar(name)).isNumber()) {
						auto accessor = accessors[index].getObject();
						auto type = accessor.getString("type");
						auto count = accessor.getUInt64("count");
						uint8_t fc = 0;
						if (type == "VEC2") fc = 2;
						if (type == "VEC3") fc = 3;
						if (type == "VEC4") fc = 4;
						if (fc != 0 && count > 0)
							layout.add(e, attribT::Float, fc, false, false);
					}
				};

				addAttribute("POSITION", attrib::Position);
				addAttribute("NORMAL", attrib::Normal);
				addAttribute("TANGENT", attrib::Tangent);
				addAttribute("TEXCOORD_0", attrib::TexCoord0);

				layout.end();

				return layout;
			}
		}
		return var();
	}

	var mesh::getVertexBufferHandle(list args) {
		auto name = args.getString(0);
		auto pri = args.getUInt64(1);
		auto nodes = getList("nodes");
		auto meshes = getList("meshes");
		auto accessors = getList("accessors");
		auto node = object();
		if ((node = getNode({name}).getObject())) {
			auto meshObj = meshes.getObject(node.getUInt64("mesh"));
			auto primitives = meshObj.getList("primitives");
			if (pri >= primitives.size())
				return genericError("Primitive index out of range");
			auto it = primitives.begin();
			advance(it, pri);
			auto primitive = object();
			if (it != primitives.end()) {
				if (!(primitive = it->getObject()))
					return genericError("No primitive object");
				if (primitive.getType("vbh") == typeObject)
					return primitive.getObject("vbh");
				auto attributes = primitive.getObject("attributes");
				if (!attributes)
					return genericError("No attributes object");
				if (attributes.size() == 0)
					return genericError("No attributes");
				size_t count = -1;

				auto bin = binary();
				auto pushBack = [&bin](auto v) {
					uint8_t temp[sizeof(v)];
					memcpy(temp, &v, sizeof(v));
					for (size_t i = 0; i < sizeof(v); i++)
						bin.push_back(temp[i]);
				};
				size_t vit = 0;
				auto accessor = object();
				while (count == (size_t)(-1) || vit == count) {
					for (auto ait = attributes.begin();
							 ait != attributes.end();
							 ++ait) {
						if ((accessor = accessors.getObject(
									 ait->second.getUInt64()))) {
							count = (count == (size_t)(-1))
												? accessor.getUInt64("count")
												: count;
							auto parsed = accessor.getList("parsed");
							auto pit = parsed.begin();
							advance(pit, vit);
							if (pit != parsed.end()) {
								if (pit->isFloating()) {
									if (pit->isVec2()) {
										pushBack(pit->getFloat(0));
										pushBack(pit->getFloat(1));
									} else if (pit->isVec3()) {
										pushBack(pit->getFloat(0));
										pushBack(pit->getFloat(1));
										pushBack(pit->getFloat(2));
									} else if (pit->isVec4()) {
										pushBack(pit->getFloat(0));
										pushBack(pit->getFloat(1));
										pushBack(pit->getFloat(2));
										pushBack(pit->getFloat(3));
									}
								} else if (pit->isSigned()) {
									if (pit->isVec2()) {
										pushBack(pit->getInt16(0));
										pushBack(pit->getInt16(1));
									} else if (pit->isVec3()) {
										pushBack(pit->getInt16(0));
										pushBack(pit->getInt16(1));
										pushBack(pit->getInt16(2));
									} else if (pit->isVec4()) {
										pushBack(pit->getInt16(0));
										pushBack(pit->getInt16(1));
										pushBack(pit->getInt16(2));
										pushBack(pit->getInt16(3));
									}
								} else if (
									pit->getType() == typeVec2UInt8 ||
									pit->getType() == typeVec3UInt8 ||
									pit->getType() == typeVec4UInt8) {
									if (pit->isVec2()) {
										pushBack(pit->getUInt8(0));
										pushBack(pit->getUInt8(1));
									} else if (pit->isVec3()) {
										pushBack(pit->getUInt8(0));
										pushBack(pit->getUInt8(1));
										pushBack(pit->getUInt8(2));
									} else if (pit->isVec4()) {
										pushBack(pit->getUInt8(0));
										pushBack(pit->getUInt8(1));
										pushBack(pit->getUInt8(2));
										pushBack(pit->getUInt8(3));
									}
								}
							}
						}
					}
					vit++;
				}
				primitive.setBinary("packed", bin);

				auto layout = getVertexLayoutHandle({name, pri})
												.getObject<vertexLayout>();
				auto vbh = vertexBuffer({
					{"type", standardBufferType},
					{"data", bin},
					{"layout", layout},
				});
				primitive.setObject("vbh", vbh);
				return vbh;
			}
		}
		return var();
	}

	var mesh::getIndexBufferHandle(list args) {
		auto name = args.getString(0);
		auto pri = args.getUInt64(1);
		auto nodes = getList("nodes");
		auto meshes = getList("meshes");
		auto accessors = getList("accessors");
		auto node = object();
		if ((node = getNode({name}).getObject())) {
			auto meshObj = meshes.getObject(node.getUInt64("mesh"));
			auto primitives = meshObj.getList("primitives");
			if (pri >= primitives.size())
				return genericError("Primitive index out of range");
			auto it = primitives.begin();
			advance(it, pri);
			auto primitive = object();
			if (it != primitives.end()) {
				if (!(primitive = it->getObject()))
					return genericError("No primitive object");
				if (!primitive.getVar("primitive").isNumber())
					return genericError("No index buffer");
				auto indicies =
					accessors.getObject(primitive.getUInt64("indices"));
				if (indicies.getType("ibh") == typeObject)
					return indicies.getObject("ibh");
				auto parsed = indicies.getList("parsed");
				auto bin = binary();
				auto componentType = indicies.getUInt32("componentType");
				for (auto iit = parsed.begin(); iit != parsed.end();
						 iit++) {
					uint32_t v = iit->getUInt32();
					size_t width = componentType == 5125 ? sizeof(v)
						: sizeof(uint16_t);
					uint8_t temp[sizeof(uint32_t)];
					if (width == sizeof(uint32_t))
						memcpy(temp, &v, width);
					else {
						auto shortValue = uint16_t(v);
						memcpy(temp, &shortValue, width);
					}
					for (size_t i = 0; i < width; i++)
						bin.push_back(temp[i]);
				}
				indicies.setBinary("packed", bin);

				auto ibh = indexBuffer({
					{"type", standardBufferType},
					{"data", bin},
				});
				indicies.setObject("ibh", ibh);
				return ibh;
			}
		}
		return var();
	}

	var mesh::getTrianglesFromMesh(list args) {
		auto name = args.getString(0);
		auto nodes = getList("nodes");
		auto meshes = getList("meshes");
		auto accessors = getList("accessors");
		auto node = object();
		if ((node = getNode({name}).getObject())) {
			auto triangles = list();
			auto meshObj = meshes.getObject(node.getUInt64("mesh"));
			auto primitives = meshObj.getList("primitives");
			for (auto it = primitives.begin(); it != primitives.end();
					 ++it) {
				auto primitive = it->getObject();
				if (!primitive)
					return genericError("No primitive object");
				if (!primitive.getVar("primitive").isNumber())
					return genericError("No index buffer");
				auto attributes = primitive.getObject("attributes");
				auto indicies =
					accessors.getObject(primitive.getUInt64("indices"));
				auto verts =
					accessors.getObject(attributes.getUInt64("POSITION"));

				auto vertParsed = verts.getList("parsed");
				auto inParsed = indicies.getList("parsed");
				for (auto iit = inParsed.begin(); iit != inParsed.end();
						 iit++) {
					auto vert = vertParsed.getVar(iit->getUInt64());
					if (vert) triangles.pushVar(vert);
				}
			}
			return triangles;
		}
		return var();
	}

	var mesh::getMaterial(list args) {
		auto fArg = args.getVar(0);
		if (fArg.isString()) {
			auto name = fArg.getString();
			auto materials = getList("materials");
			if (materials)
				for (auto it = materials.begin(); it != materials.end();
						 ++it) {
					auto mat = it->getObject();
					if (mat && name.compare(mat.getString("name")) == 0)
						return *it;
				}
		} else if (fArg.isNumber()) {
			auto id = fArg.getUInt64();
			auto materials = getList("materials");
			if (materials) return materials.getVar(id);
		}
		return var();
	}

	var mesh::getMaterialFromPrimitive(list args) {
		auto name = args.getString(0);
		auto pri = args.getUInt64(1);
		auto materials = getList("materials");
		auto nodes = getList("nodes");
		auto meshes = getList("meshes");
		auto accessors = getList("accessors");
		auto node = object();
		if ((node = getNode({name}).getObject())) {
			auto meshObj = meshes.getObject(node.getUInt64("mesh"));
			auto primitives = meshObj.getList("primitives");
			auto it = primitives.begin();
			advance(it, pri);
			if (it != primitives.end()) {
				auto primitive = it->getObject();
				if (!primitive)
					return genericError("No primitive object");
				if (!primitive.getVar("material").isNumber())
					return var();
				auto matId = primitive.getUInt64("material");
				// Texture resources are resolved by the renderer.
				return materials.getObject(matId);
			}
		}
		return var();
	}

	var mesh::destroy(list) { return var(); }

	void mesh::parseGLTFBuffer(
		string type,
		uint32_t scalarType,
		uint64_t offset,
		uint64_t count,
		void* data,
		list& parsed) {
		if (type.compare("SCALAR") == 0) {
			switch (scalarType) {
				case 5120: {  // INT8
					auto ptr = (int8_t*)(((uint8_t*)data) + offset);
					for (size_t i = 0; i < count; ++i) {
						parsed.pushInt8((ptr[i]));
					}
					break;
				}
				case 5121: {  // UINT8
					auto ptr = (uint8_t*)(((uint8_t*)data) + offset);
					for (size_t i = 0; i < count; ++i) {
						parsed.pushUInt8((ptr[i]));
					}
					break;
				}
				case 5122: {  // INT16
					auto ptr = (int16_t*)(((uint8_t*)data) + offset);
					for (size_t i = 0; i < count; ++i) {
						parsed.pushInt16((ptr[i]));
					}
					break;
				}
				case 5123: {  // UINT16
					auto ptr = (uint16_t*)(((uint8_t*)data) + offset);
					for (size_t i = 0; i < count; ++i) {
						parsed.pushUInt16((ptr[i]));
					}
					break;
				}
				case 5125: {  // UINT32
					auto ptr = (uint32_t*)(((uint8_t*)data) + offset);
					for (size_t i = 0; i < count; ++i) {
						parsed.pushUInt32((ptr[i]));
					}
					break;
				}
				case 5126: {  // FLOAT
					auto ptr = (float*)(((uint8_t*)data) + offset);
					for (size_t i = 0; i < count; ++i) {
						parsed.pushFloat((ptr[i]));
					}
					break;
				}
				default:
					break;
			}
		} else if (type.compare("VEC2") == 0) {
			switch (scalarType) {
				case 5120: {  // INT8
					auto ptr = (int8_t*)(((uint8_t*)data) + offset);
					for (size_t i = 0; i < count; ++i) {
						parsed.pushVar(
							vec2i8(ptr[(i * 2)], ptr[(i * 2) + 1]));
					}
					break;
				}
				case 5121: {  // UINT8
					auto ptr = (uint8_t*)(((uint8_t*)data) + offset);
					for (size_t i = 0; i < count; ++i) {
						parsed.pushVar(
							vec2u8(ptr[(i * 2)], ptr[(i * 2) + 1]));
					}
					break;
				}
				case 5122: {  // INT16
					auto ptr = (int16_t*)(((uint8_t*)data) + offset);
					for (size_t i = 0; i < count; ++i) {
						parsed.pushVar(
							vec2i16(ptr[(i * 2)], ptr[(i * 2) + 1]));
					}
					break;
				}
				case 5123: {  // UINT16
					auto ptr = (uint16_t*)(((uint8_t*)data) + offset);
					for (size_t i = 0; i < count; ++i) {
						parsed.pushVar(
							vec2u16(ptr[(i * 2)], ptr[(i * 2) + 1]));
					}
					break;
				}
				case 5125: {  // UINT32
					auto ptr = (uint32_t*)(((uint8_t*)data) + offset);
					for (size_t i = 0; i < count; ++i) {
						parsed.pushVar(
							vec2u32(ptr[(i * 2)], ptr[(i * 2) + 1]));
					}
					break;
				}
				case 5126: {  // FLOAT
					auto ptr = (float*)(((uint8_t*)data) + offset);
					for (size_t i = 0; i < count; ++i) {
						parsed.pushVar(
							vec2f(ptr[(i * 2)], ptr[(i * 2) + 1]));
					}
					break;
				}
				default:
					break;
			}
		} else if (type.compare("VEC3") == 0) {
			switch (scalarType) {
				case 5120: {  // INT8
					auto ptr = (int8_t*)(((uint8_t*)data) + offset);
					for (size_t i = 0; i < count; ++i) {
						parsed.pushVar(vec3i8(
							ptr[(i * 3)],
							ptr[(i * 3) + 1],
							ptr[(i * 3) + 2]));
					}
					break;
				}
				case 5121: {  // UINT8
					auto ptr = (uint8_t*)(((uint8_t*)data) + offset);
					for (size_t i = 0; i < count; ++i) {
						parsed.pushVar(vec3u8(
							ptr[(i * 3)],
							ptr[(i * 3) + 1],
							ptr[(i * 3) + 2]));
					}
					break;
				}
				case 5122: {  // INT16
					auto ptr = (int16_t*)(((uint8_t*)data) + offset);
					for (size_t i = 0; i < count; ++i) {
						parsed.pushVar(vec3i16(
							ptr[(i * 3)],
							ptr[(i * 3) + 1],
							ptr[(i * 3) + 2]));
					}
					break;
				}
				case 5123: {  // UINT16
					auto ptr = (uint16_t*)(((uint8_t*)data) + offset);
					for (size_t i = 0; i < count; ++i) {
						parsed.pushVar(vec3u16(
							ptr[(i * 3)],
							ptr[(i * 3) + 1],
							ptr[(i * 3) + 2]));
					}
					break;
				}
				case 5125: {  // UINT32
					auto ptr = (uint32_t*)(((uint8_t*)data) + offset);
					for (size_t i = 0; i < count; ++i) {
						parsed.pushVar(vec3u32(
							ptr[(i * 3)],
							ptr[(i * 3) + 1],
							ptr[(i * 3) + 2]));
					}
					break;
				}
				case 5126: {  // FLOAT
					auto ptr = (float*)(((uint8_t*)data) + offset);
					for (size_t i = 0; i < count; ++i) {
						parsed.pushVar(vec3f(
							ptr[(i * 3)],
							ptr[(i * 3) + 1],
							ptr[(i * 3) + 2]));
					}
					break;
				}
				default:
					break;
			}
		} else if (type.compare("VEC4") == 0) {
			switch (scalarType) {
				case 5120: {  // INT8
					auto ptr = (int8_t*)(((uint8_t*)data) + offset);
					for (size_t i = 0; i < count; ++i) {
						parsed.pushVar(vec4i8(
							ptr[(i * 4)],
							ptr[(i * 4) + 1],
							ptr[(i * 4) + 2],
							ptr[(i * 4) + 3]));
					}
					break;
				}
				case 5121: {  // UINT8
					auto ptr = (uint8_t*)(((uint8_t*)data) + offset);
					for (size_t i = 0; i < count; ++i) {
						parsed.pushVar(vec4u8(
							ptr[(i * 4)],
							ptr[(i * 4) + 1],
							ptr[(i * 4) + 2],
							ptr[(i * 4) + 3]));
					}
					break;
				}
				case 5122: {  // INT16
					auto ptr = (int16_t*)(((uint8_t*)data) + offset);
					for (size_t i = 0; i < count; ++i) {
						parsed.pushVar(vec4i16(
							ptr[(i * 4)],
							ptr[(i * 4) + 1],
							ptr[(i * 4) + 2],
							ptr[(i * 4) + 3]));
					}
					break;
				}
				case 5123: {  // UINT16
					auto ptr = (uint16_t*)(((uint8_t*)data) + offset);
					for (size_t i = 0; i < count; ++i) {
						parsed.pushVar(vec4u16(
							ptr[(i * 4)],
							ptr[(i * 4) + 1],
							ptr[(i * 4) + 2],
							ptr[(i * 4) + 3]));
					}
					break;
				}
				case 5125: {  // UINT32
					auto ptr = (uint32_t*)(((uint8_t*)data) + offset);
					for (size_t i = 0; i < count; ++i) {
						parsed.pushVar(vec4u32(
							ptr[(i * 4)],
							ptr[(i * 4) + 1],
							ptr[(i * 4) + 2],
							ptr[(i * 4) + 3]));
					}
					break;
				}
				case 5126: {  // FLOAT
					auto ptr = (float*)(((uint8_t*)data) + offset);
					for (size_t i = 0; i < count; ++i) {
						parsed.pushVar(vec4f(
							ptr[(i * 4)],
							ptr[(i * 4) + 1],
							ptr[(i * 4) + 2],
							ptr[(i * 4) + 3]));
					}
					break;
				}
				default:
					break;
			}
		} else if (type.compare("MAT2") == 0) {
			switch (scalarType) {
				case 5120: {  // INT8
					auto ptr = (int8_t*)(((uint8_t*)data) + offset);
					for (size_t i = 0; i < count; ++i) {
						parsed.pushVar(
							list({ptr[(i * 4)], ptr[(i * 4) + 1],
										ptr[(i * 4) + 2], ptr[(i * 4) + 3]}));
					}
					break;
				}
				case 5121: {  // UINT8
					auto ptr = (uint8_t*)(((uint8_t*)data) + offset);
					for (size_t i = 0; i < count; ++i) {
						parsed.pushVar(
							list({ptr[(i * 4)], ptr[(i * 4) + 1],
										ptr[(i * 4) + 2], ptr[(i * 4) + 3]}));
					}
					break;
				}
				case 5122: {  // INT16
					auto ptr = (int16_t*)(((uint8_t*)data) + offset);
					for (size_t i = 0; i < count; ++i) {
						parsed.pushVar(
							list({ptr[(i * 4)], ptr[(i * 4) + 1],
										ptr[(i * 4) + 2], ptr[(i * 4) + 3]}));
					}
					break;
				}
				case 5123: {  // UINT16
					auto ptr = (uint16_t*)(((uint8_t*)data) + offset);
					for (size_t i = 0; i < count; ++i) {
						parsed.pushVar(
							list({ptr[(i * 4)], ptr[(i * 4) + 1],
										ptr[(i * 4) + 2], ptr[(i * 4) + 3]}));
					}
					break;
				}
				case 5125: {  // UINT32
					auto ptr = (uint32_t*)(((uint8_t*)data) + offset);
					for (size_t i = 0; i < count; ++i) {
						parsed.pushVar(
							list({ptr[(i * 4)], ptr[(i * 4) + 1],
										ptr[(i * 4) + 2], ptr[(i * 4) + 3]}));
					}
					break;
				}
				case 5126: {  // FLOAT
					auto ptr = (float*)(((uint8_t*)data) + offset);
					for (size_t i = 0; i < count; ++i) {
						parsed.pushVar(
							list({ptr[(i * 4)], ptr[(i * 4) + 1],
										ptr[(i * 4) + 2], ptr[(i * 4) + 3]}));
					}
					break;
				}
				default:
					break;
			}
		} else if (type.compare("MAT3") == 0) {
			switch (scalarType) {
				case 5120: {  // INT8
					auto ptr = (int8_t*)(((uint8_t*)data) + offset);
					for (size_t i = 0; i < count; ++i) {
						parsed.pushVar(mat3x3f({
							(float)ptr[(i * 9)],
							(float)ptr[(i * 9) + 1],
							(float)ptr[(i * 9) + 2],
							(float)ptr[(i * 9) + 3],
							(float)ptr[(i * 9) + 4],
							(float)ptr[(i * 9) + 5],
							(float)ptr[(i * 9) + 6],
							(float)ptr[(i * 9) + 7],
							(float)ptr[(i * 9) + 8],
						}));
					}
					break;
				}
				case 5121: {  // UINT8
					auto ptr = (uint8_t*)(((uint8_t*)data) + offset);
					for (size_t i = 0; i < count; ++i) {
						parsed.pushVar(mat3x3f({
							(float)ptr[(i * 9)],
							(float)ptr[(i * 9) + 1],
							(float)ptr[(i * 9) + 2],
							(float)ptr[(i * 9) + 3],
							(float)ptr[(i * 9) + 4],
							(float)ptr[(i * 9) + 5],
							(float)ptr[(i * 9) + 6],
							(float)ptr[(i * 9) + 7],
							(float)ptr[(i * 9) + 8],
						}));
					}
					break;
				}
				case 5122: {  // INT16
					auto ptr = (int16_t*)(((uint8_t*)data) + offset);
					for (size_t i = 0; i < count; ++i) {
						parsed.pushVar(mat3x3f({
							(float)ptr[(i * 9)],
							(float)ptr[(i * 9) + 1],
							(float)ptr[(i * 9) + 2],
							(float)ptr[(i * 9) + 3],
							(float)ptr[(i * 9) + 4],
							(float)ptr[(i * 9) + 5],
							(float)ptr[(i * 9) + 6],
							(float)ptr[(i * 9) + 7],
							(float)ptr[(i * 9) + 8],
						}));
					}
					break;
				}
				case 5123: {  // UINT16
					auto ptr = (uint16_t*)(((uint8_t*)data) + offset);
					for (size_t i = 0; i < count; ++i) {
						parsed.pushVar(mat3x3f({
							(float)ptr[(i * 9)],
							(float)ptr[(i * 9) + 1],
							(float)ptr[(i * 9) + 2],
							(float)ptr[(i * 9) + 3],
							(float)ptr[(i * 9) + 4],
							(float)ptr[(i * 9) + 5],
							(float)ptr[(i * 9) + 6],
							(float)ptr[(i * 9) + 7],
							(float)ptr[(i * 9) + 8],
						}));
					}
					break;
				}
				case 5125: {  // UINT32
					auto ptr = (uint32_t*)(((uint8_t*)data) + offset);
					for (size_t i = 0; i < count; ++i) {
						parsed.pushVar(mat3x3f({
							(float)ptr[(i * 9)],
							(float)ptr[(i * 9) + 1],
							(float)ptr[(i * 9) + 2],
							(float)ptr[(i * 9) + 3],
							(float)ptr[(i * 9) + 4],
							(float)ptr[(i * 9) + 5],
							(float)ptr[(i * 9) + 6],
							(float)ptr[(i * 9) + 7],
							(float)ptr[(i * 9) + 8],
						}));
					}
					break;
				}
				case 5126: {  // FLOAT
					auto ptr = (float*)(((uint8_t*)data) + offset);
					for (size_t i = 0; i < count; ++i) {
						parsed.pushVar(mat3x3f({
							(float)ptr[(i * 9)],
							(float)ptr[(i * 9) + 1],
							(float)ptr[(i * 9) + 2],
							(float)ptr[(i * 9) + 3],
							(float)ptr[(i * 9) + 4],
							(float)ptr[(i * 9) + 5],
							(float)ptr[(i * 9) + 6],
							(float)ptr[(i * 9) + 7],
							(float)ptr[(i * 9) + 8],
						}));
					}
					break;
				}
				default:
					break;
			}
		} else if (type.compare("MAT4") == 0) {
			switch (scalarType) {
				case 5120: {  // INT8
					auto ptr = (int8_t*)(((uint8_t*)data) + offset);
					for (size_t i = 0; i < count; ++i) {
						parsed.pushVar(mat4x4f({
							(float)ptr[(i * 16)],
							(float)ptr[(i * 16) + 1],
							(float)ptr[(i * 16) + 2],
							(float)ptr[(i * 16) + 3],
							(float)ptr[(i * 16) + 4],
							(float)ptr[(i * 16) + 5],
							(float)ptr[(i * 16) + 6],
							(float)ptr[(i * 16) + 7],
							(float)ptr[(i * 16) + 8],
							(float)ptr[(i * 16) + 9],
							(float)ptr[(i * 16) + 10],
							(float)ptr[(i * 16) + 11],
							(float)ptr[(i * 16) + 12],
							(float)ptr[(i * 16) + 13],
							(float)ptr[(i * 16) + 14],
							(float)ptr[(i * 16) + 15],
						}));
					}
					break;
				}
				case 5121: {  // UINT8
					auto ptr = (uint8_t*)(((uint8_t*)data) + offset);
					for (size_t i = 0; i < count; ++i) {
						parsed.pushVar(mat4x4f({
							(float)ptr[(i * 16)],
							(float)ptr[(i * 16) + 1],
							(float)ptr[(i * 16) + 2],
							(float)ptr[(i * 16) + 3],
							(float)ptr[(i * 16) + 4],
							(float)ptr[(i * 16) + 5],
							(float)ptr[(i * 16) + 6],
							(float)ptr[(i * 16) + 7],
							(float)ptr[(i * 16) + 8],
							(float)ptr[(i * 16) + 9],
							(float)ptr[(i * 16) + 10],
							(float)ptr[(i * 16) + 11],
							(float)ptr[(i * 16) + 12],
							(float)ptr[(i * 16) + 13],
							(float)ptr[(i * 16) + 14],
							(float)ptr[(i * 16) + 15],
						}));
					}
					break;
				}
				case 5122: {  // INT16
					auto ptr = (int16_t*)(((uint8_t*)data) + offset);
					for (size_t i = 0; i < count; ++i) {
						parsed.pushVar(mat4x4f({
							(float)ptr[(i * 16)],
							(float)ptr[(i * 16) + 1],
							(float)ptr[(i * 16) + 2],
							(float)ptr[(i * 16) + 3],
							(float)ptr[(i * 16) + 4],
							(float)ptr[(i * 16) + 5],
							(float)ptr[(i * 16) + 6],
							(float)ptr[(i * 16) + 7],
							(float)ptr[(i * 16) + 8],
							(float)ptr[(i * 16) + 9],
							(float)ptr[(i * 16) + 10],
							(float)ptr[(i * 16) + 11],
							(float)ptr[(i * 16) + 12],
							(float)ptr[(i * 16) + 13],
							(float)ptr[(i * 16) + 14],
							(float)ptr[(i * 16) + 15],
						}));
					}
					break;
				}
				case 5123: {  // UINT16
					auto ptr = (uint16_t*)(((uint8_t*)data) + offset);
					for (size_t i = 0; i < count; ++i) {
						parsed.pushVar(mat4x4f({
							(float)ptr[(i * 16)],
							(float)ptr[(i * 16) + 1],
							(float)ptr[(i * 16) + 2],
							(float)ptr[(i * 16) + 3],
							(float)ptr[(i * 16) + 4],
							(float)ptr[(i * 16) + 5],
							(float)ptr[(i * 16) + 6],
							(float)ptr[(i * 16) + 7],
							(float)ptr[(i * 16) + 8],
							(float)ptr[(i * 16) + 9],
							(float)ptr[(i * 16) + 10],
							(float)ptr[(i * 16) + 11],
							(float)ptr[(i * 16) + 12],
							(float)ptr[(i * 16) + 13],
							(float)ptr[(i * 16) + 14],
							(float)ptr[(i * 16) + 15],
						}));
					}
					break;
				}
				case 5125: {  // UINT32
					auto ptr = (uint32_t*)(((uint8_t*)data) + offset);
					for (size_t i = 0; i < count; ++i) {
						parsed.pushVar(mat4x4f({
							(float)ptr[(i * 16)],
							(float)ptr[(i * 16) + 1],
							(float)ptr[(i * 16) + 2],
							(float)ptr[(i * 16) + 3],
							(float)ptr[(i * 16) + 4],
							(float)ptr[(i * 16) + 5],
							(float)ptr[(i * 16) + 6],
							(float)ptr[(i * 16) + 7],
							(float)ptr[(i * 16) + 8],
							(float)ptr[(i * 16) + 9],
							(float)ptr[(i * 16) + 10],
							(float)ptr[(i * 16) + 11],
							(float)ptr[(i * 16) + 12],
							(float)ptr[(i * 16) + 13],
							(float)ptr[(i * 16) + 14],
							(float)ptr[(i * 16) + 15],
						}));
					}
					break;
				}
				case 5126: {  // FLOAT
					auto ptr = (float*)(((uint8_t*)data) + offset);
					for (size_t i = 0; i < count; ++i) {
						parsed.pushVar(mat4x4f({
							(float)ptr[(i * 16)],
							(float)ptr[(i * 16) + 1],
							(float)ptr[(i * 16) + 2],
							(float)ptr[(i * 16) + 3],
							(float)ptr[(i * 16) + 4],
							(float)ptr[(i * 16) + 5],
							(float)ptr[(i * 16) + 6],
							(float)ptr[(i * 16) + 7],
							(float)ptr[(i * 16) + 8],
							(float)ptr[(i * 16) + 9],
							(float)ptr[(i * 16) + 10],
							(float)ptr[(i * 16) + 11],
							(float)ptr[(i * 16) + 12],
							(float)ptr[(i * 16) + 13],
							(float)ptr[(i * 16) + 14],
							(float)ptr[(i * 16) + 15],
						}));
					}
					break;
				}
				default:
					break;
			}
		}
	}

}  // namespace gold

/*
Parse OBJ ::
auto ext = extension().getString();
		if (ext.find("obj") != string::npos) {
			auto buffer = string("");
			auto bin = getBinary("data");
			auto it = bin.begin();
			auto groups = object();
			auto group = object();
			auto breakList = list{uint8_t(' '), uint8_t('\n')};
			auto breakFaceList =
				list{uint8_t('/'), uint8_t(' '), uint8_t('\n')};
			while (true) {
				if (it == bin.end()) break;
				buffer = "";
				for (; it != bin.end(); ++it) {
					if (*it == '\n') {
						it++;
						break;
					} else {
						buffer.push_back(*it);
					}
				}
				auto opEnd = buffer.find(' ');
				auto argStart = opEnd + 1;
				if (argStart != string::npos)
					while (buffer[argStart] == ' ') argStart++;
				auto op = buffer.substr(0, opEnd);
				if (op.compare("g") == 0) {
					// GROUP
					group = obj();
					auto value = buffer.substr(argStart);
					group.setString("name", value);
					groups.setObject(value, group);
				} else if (op == "mtllib")
					groups.setString(
						"materialFile", buffer.substr(argStart));
				else if (op.compare("usemtl") == 0)
					group.setString("material", buffer.substr(argStart));
				else if (op.compare("v") == 0) {
					// VERT
					if (group.getType("vertices") != typeList)
						group.setList("vertices", list({}));
					auto numList =
						explode(buffer.substr(argStart), breakList);
					if (numList.size() == 3)
						group.getList("vertices")
							.pushVar(vec3f(
								stof(numList[0]),
								stof(numList[1]),
								stof(numList[2])));
					else if (numList.size() == 4)
						group.getList("vertices")
							.pushVar(vec4f(
								stof(numList[0]),
								stof(numList[1]),
								stof(numList[2]),
								stof(numList[3])));
				} else if (op.compare("vt") == 0) {
					// TEXTURE
					if (group.getType("tex0") != typeList)
						group.setList("tex0", list({}));
					auto numList =
						explode(buffer.substr(argStart), breakList);

					if (numList.size() == 1)
						group.getList("tex0").pushVar(
							vec2f(stof(numList[0]), 0));
					if (numList.size() == 2)
						group.getList("tex0").pushVar(
							vec2f(stof(numList[0]), stof(numList[1])));
					else if (numList.size() == 3)
						group.getList("tex0").pushVar(vec3f(
							stof(numList[0]),
							stof(numList[1]),
							stof(numList[2])));
				} else if (op.compare("vn") == 0) {
					// NORMAL
					if (group.getType("normals") != typeList)
						group.setList("normals", list({}));
					auto numList =
						explode(buffer.substr(argStart), breakList);

					if (numList.size() == 3)
						group.getList("normals").pushVar(vec3f(
							stof(numList[0]),
							stof(numList[1]),
							stof(numList[2])));
				} else if (op.compare("vp") == 0) {
					// PARAM SPACE
					if (group.getType("params") != typeList)
						group.setList("params", list({}));
					auto numList =
						explode(buffer.substr(argStart), breakList);

					if (numList.size() == 1)
						group.getList("params").pushVar(
							vec2f(stof(numList[0]), 0));
					if (numList.size() == 2)
						group.getList("params").pushVar(
							vec2f(stof(numList[0]), stof(numList[1])));
					else if (numList.size() == 3)
						group.getList("params").pushVar(vec3f(
							stof(numList[0]),
							stof(numList[1]),
							stof(numList[2])));
				} else if (op.compare("l") == 0) {
					// PARAM SPACE
					if (group.getType("lines") != typeList)
						group.setList("lines", list({}));
					auto strList =
						explode(buffer.substr(argStart), breakList);
					if (strList.size() > 2) {
						auto numList = list({});
						for (auto sit = strList.begin();
								 sit != strList.end();
								 ++sit)
							numList.pushInt32(stoi(sit->getString()));
						group.getList("lines").pushList(numList);
					}
				} else if (op.compare("f") == 0) {
					// FACE
					if (group.getType("faces") != typeList)
						group.setList("faces", list({}));
					auto values = buffer.substr(argStart);
					auto numList = explode(values, breakList);
					if (numList.size() == 3) {
						auto points = list({});
						auto point = list({});
						size_t si = 0;
						auto iBuff = string("");
						while (si != values.size()) {
							auto c = values[si];
							if (c == '\n' || c == ' ') {
								if (iBuff.size() >= 1) {
									point.pushInt32(stoi(iBuff) - 1);
									iBuff = "";
									points.pushList(point);
									point = list({});
								}
							} else if (c == '/') {
								if (iBuff.size() >= 1) {
									point.pushInt32(stoi(iBuff) - 1);
									iBuff = "";
								} else
									point.pushNull();
								iBuff = "";
							} else
								iBuff.push_back(c);
							si++;
						}
						if (iBuff.size() >= 1) {
							point.pushInt32(stoi(iBuff) - 1);
							iBuff = "";
							points.pushList(point);
							point = list({});
						}
						group.getList("faces").pushList(points);
					}
				}
			}
			setObject("groups", groups);
		}
*/
