#include "file.hpp"

#include "goldjson.hpp"

#include <openssl/evp.h>

#include <chrono>
#include <fstream>
#include <functional>
#include <iostream>
#include <algorithm>

namespace gold {
	using namespace std;
	namespace fs = std::filesystem;
	const auto preferred_separator =
		fs::path::preferred_separator;

	obj& file::getPrototype() {
		static auto proto = obj({
			{"save", method(&file::save)},
			{"load", method(&file::load)},
			{"trash", method(&file::trash)},
			{"getWriteTime", method(&file::getWriteTime)},
			{"hash", method(&file::hash)},
			{"extension", method(&file::extension)},
		});
		return proto;
	}

	path file::forwardPath(path p) {
		auto str = p.native();
		for (size_t i = 0; i < str.length(); ++i) {
			auto c = str[i];
			if (c == '\\') str[i] = '/';
		}
		p = path(str);
		return p;
	}

	file::file() : obj() {}

	file::file(path p) : obj() {
		setParent(getPrototype());
		setString("path", p.string());
	}

	file::file(binary data) : obj() {
		setParent(getPrototype());
		setBinary("data", data);
	}

	file::file(string_view data) {
		setParent(getPrototype());
		setStringView("data", data);
	}

	var file::save(list args) {
		auto path = getString("path");
		auto bin = getStringView("data");
		for (auto it = args.begin(); it != args.end(); ++it)
			if (it->isString() && path == "")
				path = it->getString();
			else if (
				(it->isBinary() || it->isString() || it->isView()) &&
				bin.size() == 0)
				bin = it->getStringView();
			else
				break;
		if (path.size() == 0)
			return genericError(
				"path is empty, supply as argument or set path on "
				"object");
		if (bin.size() == 0)
			return genericError(
				"data is empty, supply as argument or set data on "
				"object");
		auto str = ofstream(path, ios::binary);
		if (str.is_open()) {
			str.write((char*)bin.data(), bin.size());
			str.close();
			auto writeTime = fs::last_write_time(path);
			auto wtms =
				(uint64_t)
					std::chrono::duration_cast<std::chrono::nanoseconds>(
						writeTime.time_since_epoch())
						.count();
			setUInt64("writeTime", wtms);
			return true;
		}
		return false;
	}

	var file::load(list args) {
		try {
			auto path = args.size() > 0 ? args[0].getString()
																	: getString("path");
			if (path.size() == 0)
				return genericError(
					"path is empty, supply as argument or set path on "
					"object");
			if (fs::exists(path)) {
				auto bin = getStringView("data");
				auto writeTime = fs::last_write_time(path);
				auto wtms = (uint64_t)std::chrono::duration_cast<
											std::chrono::nanoseconds>(
											writeTime.time_since_epoch())
											.count();
				auto lastWriteTime = getUInt64("writeTime");
				if (bin.size() > 0 && wtms == lastWriteTime) return bin;
				auto str = ifstream(path, ios::binary);
				if (str.is_open()) {
					str.seekg(0, str.end);
					auto size = size_t(str.tellg());
					auto read = binary(size);
					str.seekg(0, str.beg);
					str.read((char*)read.data(), read.size());
					str.close();
					setUInt64("writeTime", wtms);
					setBinary("data", read);
					return getStringView("data");
				}
			}
			return gold::var();
		} catch (const exception& e) {
			return genericError(e.what());
		}
	}

	var file::trash(list args) {
		auto path =
			args.size() > 0 ? args[0].getString() : getString("path");
		if (path.size() == 0)
			return genericError(
				"path is empty, supply as argument or set path on "
				"object");
		setNull("writeTime");
		return remove(fs::path(path));
	}

	var file::getWriteTime(list args) {
		try {
			auto path =
				args.size() > 0 ? args[0].getString() : getString("path");
			if (path.size() == 0)
				return genericError(
					"path is empty, supply as argument or set path on "
					"object");
			auto cur = getUInt64("writeTime");
			if (cur != 0) return cur;
			if (!fs::exists(path)) return genericError("file does not exist");
			auto writeTime = fs::last_write_time(path);
			auto wtms =
				(uint64_t)
					std::chrono::duration_cast<std::chrono::nanoseconds>(
						writeTime.time_since_epoch())
						.count();
			return wtms;
		} catch (const exception& e) {
			return genericError(e.what());
		}
	}

	var file::hash(list args) {
		auto bin = binary();
		if (args.size() > 0)
			args[0].assignBinary(bin);
		else
			assignBinary("data", bin);
		auto strData = string_view((char*)bin.data(), bin.size());
		auto h = std::hash<string_view>();
		return to_string((uint64_t)h(strData));
	}

	var file::extension(list args) {
		return fs::path(getString("path")).extension().string();
	}

	var file::asJSON(list args) {
		auto d = getStringView("data");
		if (d.size() > 0) return file::parseJSON(d);
		return var();
	}

	var file::asBSON(list args) {
		auto d = getStringView("data");
		if (d.size() > 0) return file::parseBSON(d);
		return var();
	}

	var file::asCBOR(list args) {
		auto d = getStringView("data");
		if (d.size() > 0) return file::parseCBOR(d);
		return var();
	}

	var file::asMsgPack(list args) {
		auto d = getStringView("data");
		if (d.size() > 0) return file::parseMsgPack(d);
		return var();
	}

	var file::asUBJSON(list args) {
		auto d = getStringView("data");
		if (d.size() > 0) return file::parseUBJSON(d);
		return var();
	}

	file::operator binary() { return load().getBinary(); }

	file::operator string() { return load().getString(); }

	file::operator string_view() {
		return load().getStringView();
	}

	string file::currentWorkingDir() {
		return fs::current_path().string();
	}

	var file::readFile(path p) {
		auto f = file(p);
		auto err = f.load();
		if (err.isError()) return err;
		return f;
	}

	var file::saveFile(path p, string_view data) {
		auto f = file(data);
		auto err = f.save({p.string()});
		if (err.isError()) return err;
		return f;
	}

	obj& file::recursiveReadDirectory(path p, obj& results) {
		try {
			auto point = fs::canonical(p);
			auto cwd = fs::current_path();
			auto pName = point.string();
			pName =
				pName.substr(pName.rfind(preferred_separator) + 1);
			auto absPoint = fs::canonical(point);
			if (fs::exists(absPoint) && fs::is_directory(absPoint)) {
				for (auto& ip : fs::recursive_directory_iterator(
							 absPoint,
							 fs::directory_options::
								 follow_directory_symlink)) {
					auto chop = ip.path().string().find(pName);
					auto relURL = fs::relative(ip, cwd);
					auto url = forwardPath(
						fs::path(ip.path().string().substr(chop - 1)));

					if (!fs::is_directory(ip))
						results.setObject(url.string(), file(ip));
				}
			} else {
				auto e =
					"Directory doesn't exist. (" + point.string() + ")";
				results.setString("error", e);
			}
			return results;
		} catch (const exception& e) {
			return results;
		}
	}

	var file::parseJSON(string_view data) {
		return jsonParse(data);
	}

	var file::parseBSON(string_view data) {
		return bsonParse(data);
	}

	var file::parseCBOR(string_view data) {
		return cborParse(data);
	}

	var file::parseMsgPack(string_view data) {
		return msgpackParse(data);
	}

	var file::parseUBJSON(string_view data) {
		return ubjsonParse(data);
	}

	string file::serializeJSON(var data, bool pretty) {
		return jsonStringify(data, pretty);
	}

	binary file::serializeBSON(var data) {
		return bsonStringify(data);
	}

	binary file::serializeCBOR(var data) {
		return cborStringify(data);
	}

	binary file::serializeMsgPack(var data) {
		return msgpackStringify(data);
	}

	binary file::serializeUBJSON(var data) {
		return ubjsonStringify(data);
	}

	binary file::decodeDataURL(string_view v, string& mimeType) {
		binary out;
		auto s = v.size();
		if (s == 0) return out;
		if (v.substr(0, 5).compare("data:") != 0) return out;
		auto commaIndex = v.find(',');
		auto semiIndex = v.find(';');
		if (
			semiIndex == string::npos && commaIndex != string::npos) {
			mimeType = v.substr(5, commaIndex - 5);
		} else if (
			semiIndex != string::npos && semiIndex < commaIndex) {
			mimeType = v.substr(5, semiIndex - 5);
		}
		if (commaIndex == string::npos) return out;
		auto args = v.substr(5, commaIndex - 5);
		auto data = v.substr(commaIndex + 1);
		if (args.find(";base64") != string::npos) {
			out = decodeBase64(data);
		} else
// Decode percent‑escaped characters (e.g., %20 for space)
{
    std::string decoded;
    decoded.reserve(data.size());
    for (size_t i = 0; i < data.size(); ++i) {
        if (data[i] == '%' && i + 2 < data.size() && isxdigit(static_cast<unsigned char>(data[i+1])) && isxdigit(static_cast<unsigned char>(data[i+2]))) {
            std::string hexStr{data[i+1], data[i+2]};
            decoded.push_back(static_cast<char>(std::stoi(hexStr, nullptr, 16)));
            i += 2;
        } else {
            decoded.push_back(data[i]);
        }
    }
    out = binary(decoded.begin(), decoded.end());
}
		return out;
	}

	var file::pack(list entries) {
		struct entry {
			string path;
			binary data;
		};
		vector<entry> files;
		for (auto it = entries.begin(); it != entries.end(); ++it) {
			auto item = it->getObject();
			if (!item) return genericError("asset pack entry is not an object");
			auto name = item.getString("path");
			fs::path path(name);
			if (name.empty() || path.is_absolute() ||
				path.lexically_normal().string().find("..") != string::npos)
				return genericError("asset pack path must be relative and safe");
			for (auto& existing : files)
				if (existing.path == path.generic_string())
					return genericError("duplicate asset pack path");
			files.push_back({path.generic_string(), item.getBinary("data")});
		}
		sort(files.begin(), files.end(), [](const entry& a, const entry& b) {
			return a.path < b.path;
		});
		binary out;
		const char magic[] = "GOLDPAK1";
		out.insert(out.end(), magic, magic + 8);
		auto put32 = [&out](uint32_t value) {
			for (int i = 0; i < 4; ++i) out.push_back(uint8_t(value >> (i * 8)));
		};
		auto put64 = [&out](uint64_t value) {
			for (int i = 0; i < 8; ++i) out.push_back(uint8_t(value >> (i * 8)));
		};
		put32(uint32_t(files.size()));
		for (auto& item : files) {
			put32(uint32_t(item.path.size()));
			put64(uint64_t(item.data.size()));
			out.insert(out.end(), item.path.begin(), item.path.end());
			out.insert(out.end(), item.data.begin(), item.data.end());
		}
		return out;
	}

	var file::unpack(binary data) {
		if (data.size() < 12 || string((char*)data.data(), 8) != "GOLDPAK1")
			return genericError("invalid asset pack header");
		size_t pos = 8;
		auto get32 = [&]() -> uint32_t {
			uint32_t value = 0;
			for (int i = 0; i < 4; ++i) value |= uint32_t(data[pos++]) << (i * 8);
			return value;
		};
		auto get64 = [&]() -> uint64_t {
			uint64_t value = 0;
			for (int i = 0; i < 8; ++i) value |= uint64_t(data[pos++]) << (i * 8);
			return value;
		};
		uint32_t count = get32();
		list out;
		for (uint32_t i = 0; i < count; ++i) {
			if (pos > data.size() || data.size() - pos < 12)
				return genericError("truncated asset pack entry");
			uint32_t pathSize = get32();
			uint64_t dataSize = get64();
			if (pathSize == 0 || pathSize > data.size() - pos ||
				dataSize > data.size() - pos - pathSize)
				return genericError("invalid asset pack entry size");
			string path((char*)data.data() + pos, pathSize);
			pos += pathSize;
			fs::path safe(path);
			if (safe.is_absolute() || safe.lexically_normal().string().find("..") != string::npos)
				return genericError("unsafe asset pack path");
			binary bytes(data.begin() + pos, data.begin() + pos + dataSize);
			pos += dataSize;
			out.pushObject(obj({{"path", path}, {"data", bytes}}));
		}
		if (pos != data.size()) return genericError("trailing asset pack data");
		return out;
	}

	binary file::decodeBase64(string_view v) {
		if (v.empty()) return binary();

		// Accept both the standard (+ /) and URL-safe (- _) alphabets.
		auto b64 = string(v);
		for (auto& c : b64) {
			if (c == '-') c = '+';
			else if (c == '_') c = '/';
		}

		// EVP_DecodeBlock requires padding to a multiple of 4.
		auto padded = b64;
		while (padded.size() % 4 != 0) padded += '=';

		size_t padCount = 0;
		for (auto it = padded.rbegin(); it != padded.rend() && *it == '=';
				 ++it)
			padCount++;

		binary out((padded.size() / 4) * 3);
		if (out.empty()) return out;

		auto n = EVP_DecodeBlock(
			out.data(), (const unsigned char*)padded.data(),
			int(padded.size()));
		if (n < 0) return binary();
		// Each '=' accounts for one zero byte in EVP_DecodeBlock's output.
		size_t decoded = size_t(n) - padCount;
		if (decoded > out.size()) decoded = out.size();
		out.resize(decoded);
		return out;
	}

	string file::encodeBase64(binary b) {
		if (b.empty()) return "";
		binary out((b.size() / 3 + 1) * 4);
		auto n = EVP_EncodeBlock(out.data(), b.data(), int(b.size()));
		if (n < 0) return "";
		auto str = string((char*)out.data(), size_t(n));
		// URL-safe alphabet and no padding.
		for (auto& c : str) {
			if (c == '+') c = '-';
			else if (c == '/') c = '_';
			else if (c == '=') c = '\0';
		}
		auto end = str.find('\0');
		if (end != string::npos) str.resize(end);
		return str;
	}

}  // namespace gold
