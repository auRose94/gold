#include "web/dataStore.hpp"

#include <openssl/rand.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <goldjs.hpp>
#include <goldjson.hpp>
#include <map>
#include <mutex>
#include <shared_mutex>
#include <system_error>

namespace gold {
	using namespace std;

	namespace {

		namespace fs = std::filesystem;

		// A gold-native object store: each database is a directory, each
		// collection a subdirectory, and each document a JSON file named
		// by its "_id". Queries scan the collection's files and match gold
		// objects directly. Writes are atomic (temp file + rename).
		class fileDataStore : public dataStore {
			string root;
			string dbName;
			shared_mutex mtx;

			string dbDir() const { return root + "/" + dbName; }
			string colDir(const string& cname) const {
				return dbDir() + "/" + cname;
			}

			static bool filenameSafe(const string& id) {
				if (id.empty()) return false;
				for (unsigned char c : id)
					if (!isalnum(c) && c != '-' && c != '_') return false;
				return true;
			}

			// Database and collection names become directory components. Keep
			// them as simple names so callers cannot escape the configured root.
			static bool nameSafe(const string& name) {
				return filenameSafe(name);
			}

			static bool writeJSON(const string& path, const object& doc) {
				auto json = jsonStringify(var(doc));
				string tmp = path + ".tmp";
				ofstream out(tmp, ios::binary);
				if (!out) return false;
				out.write(json.data(), streamsize(json.size()));
				out.close();
				std::error_code ec;
				fs::rename(tmp, path, ec);
				return !ec;
			}

			static object readJSON(const string& path) {
				ifstream in(path, ios::binary);
				if (!in) return object();
				string data((istreambuf_iterator<char>(in)),
					istreambuf_iterator<char>());
				auto v = jsonParse(data);
				if (v.isObject()) return v.getObject();
				return object();
			}

			// Match a filter object against a document. Equality for plain
			// values; `{"$in", [...]}` matches when the field (a list)
			// intersects the given list. (Takes non-const refs because
			// object accessors are non-const.)
			static bool matches(object& doc, object& filter) {
				for (auto it = filter.begin(); it != filter.end(); ++it) {
					auto fv = it->second;
					if (fv.getType() == typeObject) {
						auto fo = fv.getObject();
						auto in = fo.getVar("$in");
						if (in.getType() != typeList) return false;
						auto inList = in.getList();
						auto docField = doc.getVar(it->first);
						if (docField.getType() != typeList) return false;
						auto docList = docField.getList();
						bool any = false;
						for (auto d : docList)
							for (auto x : inList)
								if (d == x) { any = true; break; }
						if (!any) return false;
					} else {
						auto dv = doc.getVar(it->first);
						if (!(dv == fv)) return false;
					}
				}
				return true;
			}

			static bool applyUpdate(object& doc, object& update) {
				auto set = update.getObject("$set");
				if (!set) return false;
				for (auto it = set.begin(); it != set.end(); ++it)
					doc.setVar(it->first, it->second);
				return true;
			}

			void collectMatches(const string& cname, object& filter,
				uint64_t limit, list& out) {
				auto dir = colDir(cname);
				if (!fs::is_directory(dir)) return;
				uint64_t n = 0;
				for (const auto& entry : fs::directory_iterator(dir)) {
					if (!entry.is_regular_file()) continue;
					auto name = entry.path().filename().string();
					if (name.size() < 6 ||
						name.substr(name.size() - 5) != ".json")
						continue;
					auto doc = readJSON(entry.path().string());
					if (doc && matches(doc, filter)) {
						out.pushObject(doc);
						++n;
						if (limit > 0 && n >= limit) break;
					}
				}
			}

		 public:
			bool open(const std::string& dbName_,
				const std::string& path) override {
				lock_guard<shared_mutex> guard(mtx);
				if (!nameSafe(dbName_) || path.empty()) return false;
				dbName = dbName_;
				root = path;
				fs::create_directories(dbDir());
				return fs::is_directory(dbDir());
			}

			void close() override {}

			std::vector<std::string> getDatabaseNames() override {
				shared_lock<shared_mutex> guard(mtx);
				std::vector<std::string> out;
				if (!fs::is_directory(root)) return out;
				for (const auto& entry : fs::directory_iterator(root))
					if (entry.is_directory())
						out.push_back(entry.path().filename().string());
				return out;
			}

			bool createCollection(const std::string& cname) override {
				lock_guard<shared_mutex> guard(mtx);
				if (!nameSafe(cname)) return false;
				fs::create_directories(colDir(cname));
				return fs::is_directory(colDir(cname));
			}

			std::vector<std::string> getCollectionNames() override {
				shared_lock<shared_mutex> guard(mtx);
				std::vector<std::string> out;
				if (!fs::is_directory(dbDir())) return out;
				for (const auto& entry : fs::directory_iterator(dbDir()))
					if (entry.is_directory())
						out.push_back(entry.path().filename().string());
				return out;
			}

			bool renameCollection(const std::string& oldName,
				const std::string& newName) override {
				lock_guard<shared_mutex> guard(mtx);
				if (!nameSafe(oldName) || !nameSafe(newName)) return false;
				std::error_code ec;
				fs::rename(colDir(oldName), colDir(newName), ec);
				return !ec;
			}

			object findOne(const std::string& cname,
				const object& filter) override {
				shared_lock<shared_mutex> guard(mtx);
				if (!nameSafe(cname)) return object();
				object f = filter;
				list matches;
				collectMatches(cname, f, 1, matches);
				if (matches.size() > 0) return matches.getObject(0);
				return object();
			}

			list find(const std::string& cname, const object& filter,
				uint64_t limit) override {
				shared_lock<shared_mutex> guard(mtx);
				if (!nameSafe(cname)) return list();
				object f = filter;
				list out;
				collectMatches(cname, f, limit, out);
				return out;
			}

			var insert(const std::string& cname,
				const object& doc) override {
				lock_guard<shared_mutex> guard(mtx);
				if (!nameSafe(cname)) return genericError("invalid collection name");
				auto toStore = doc;
				auto id = toStore.getString("_id");
				if (!filenameSafe(id)) {
					id = dataStoreNewID();
					toStore.setString("_id", id);
				}
				auto dir = colDir(cname);
				fs::create_directories(dir);
				if (!writeJSON(dir + "/" + id + ".json", toStore))
					return genericError("failed to write document");
				return var(toStore);
			}

			var updateOne(const std::string& cname, const object& filter,
				const object& update) override {
				lock_guard<shared_mutex> guard(mtx);
				if (!nameSafe(cname)) return var();
				object f = filter;
				object u = update;
				auto dir = colDir(cname);
				if (!fs::is_directory(dir)) return var();
				for (const auto& entry : fs::directory_iterator(dir)) {
					if (!entry.is_regular_file()) continue;
					auto name = entry.path().filename().string();
					if (name.size() < 6 ||
						name.substr(name.size() - 5) != ".json")
						continue;
					auto doc = readJSON(entry.path().string());
					if (doc && matches(doc, f)) {
						if (applyUpdate(doc, u))
							writeJSON(entry.path().string(), doc);
						return var(doc);
					}
				}
				return var();
			}

			var updateMany(const std::string& cname, const object& filter,
				const object& update) override {
				lock_guard<shared_mutex> guard(mtx);
				if (!nameSafe(cname)) return var(object{{"modifiedCount", uint64_t(0)}});
				object f = filter;
				object u = update;
				uint64_t count = 0;
				auto dir = colDir(cname);
				if (!fs::is_directory(dir))
					return var(object{{"modifiedCount", uint64_t(0)}});
				for (const auto& entry : fs::directory_iterator(dir)) {
					if (!entry.is_regular_file()) continue;
					auto name = entry.path().filename().string();
					if (name.size() < 6 ||
						name.substr(name.size() - 5) != ".json")
						continue;
					auto doc = readJSON(entry.path().string());
					if (doc && matches(doc, f)) {
						if (applyUpdate(doc, u)) {
							writeJSON(entry.path().string(), doc);
							++count;
						}
					}
				}
				return var(object{{"modifiedCount", count}});
			}

			var deleteOne(const std::string& cname,
				const object& filter) override {
				lock_guard<shared_mutex> guard(mtx);
				if (!nameSafe(cname)) return var(object{{"deletedCount", uint64_t(0)}});
				object f = filter;
				auto dir = colDir(cname);
				if (!fs::is_directory(dir))
					return var(object{{"deletedCount", uint64_t(0)}});
				for (const auto& entry : fs::directory_iterator(dir)) {
					if (!entry.is_regular_file()) continue;
					auto name = entry.path().filename().string();
					if (name.size() < 6 ||
						name.substr(name.size() - 5) != ".json")
						continue;
					auto doc = readJSON(entry.path().string());
					if (doc && matches(doc, f)) {
						fs::remove(entry.path());
						return var(object{{"deletedCount", uint64_t(1)}});
					}
				}
				return var(object{{"deletedCount", uint64_t(0)}});
			}

			var deleteMany(const std::string& cname,
				const object& filter) override {
				lock_guard<shared_mutex> guard(mtx);
				if (!nameSafe(cname)) return var(object{{"deletedCount", uint64_t(0)}});
				object f = filter;
				uint64_t count = 0;
				auto dir = colDir(cname);
				if (!fs::is_directory(dir))
					return var(object{{"deletedCount", uint64_t(0)}});
				for (const auto& entry : fs::directory_iterator(dir)) {
					if (!entry.is_regular_file()) continue;
					auto name = entry.path().filename().string();
					if (name.size() < 6 ||
						name.substr(name.size() - 5) != ".json")
						continue;
					auto doc = readJSON(entry.path().string());
					if (doc && matches(doc, f)) {
						fs::remove(entry.path());
						++count;
					}
				}
				return var(object{{"deletedCount", count}});
			}

			var replace(const std::string& cname, const object& filter,
				const object& doc) override {
				lock_guard<shared_mutex> guard(mtx);
				if (!nameSafe(cname)) return var();
				object f = filter;
				auto dir = colDir(cname);
				if (!fs::is_directory(dir)) return var();
				for (const auto& entry : fs::directory_iterator(dir)) {
					if (!entry.is_regular_file()) continue;
					auto name = entry.path().filename().string();
					if (name.size() < 6 ||
						name.substr(name.size() - 5) != ".json")
						continue;
					auto existing = readJSON(entry.path().string());
					if (existing && matches(existing, f)) {
						writeJSON(entry.path().string(), doc);
						return var(doc);
					}
				}
				return var();
			}

			bool addIndexes(const std::string& cname,
				const object& keys) override {
				// Advisory for the file backend: record the index request.
				lock_guard<shared_mutex> guard(mtx);
				if (!nameSafe(cname)) return false;
				auto dir = colDir(cname);
				fs::create_directories(dir);
				auto meta = jo("indexes", var(list{var(keys)}));
				writeJSON(dir + "/.index.json", meta);
				return true;
			}

			bool dropIndex(const std::string& cname) override {
				lock_guard<shared_mutex> guard(mtx);
				if (!nameSafe(cname)) return false;
				fs::remove(colDir(cname) + "/.index.json");
				return true;
			}

			const char* name() const override { return "file"; }
		};

		struct fileRegistrar {
			fileRegistrar() {
				registerDataStore("file", []() -> dataStore* {
					return new fileDataStore();
				});
			}
		};
		fileRegistrar fileReg;

	}  // namespace

	namespace {
		std::mutex& storeMutex() {
			static std::mutex m;
			return m;
		}
		std::map<std::string, dataStore* (*)()>& storeFactories() {
			static std::map<std::string, dataStore* (*)()> f;
			return f;
		}
	}  // namespace

	void registerDataStore(const std::string& name,
		dataStore* (*factory)()) {
		lock_guard<mutex> guard(storeMutex());
		storeFactories()[name] = factory;
	}

	dataStore* createDataStore(const std::string& name) {
		lock_guard<mutex> guard(storeMutex());
		auto it = storeFactories().find(name);
		if (it != storeFactories().end()) return it->second();
		return nullptr;
	}

	string dataStoreNewID() {
		unsigned char bytes[12];
		RAND_bytes(bytes, sizeof(bytes));
		static const char* hex = "0123456789abcdef";
		string out;
		out.reserve(24);
		for (unsigned char b : bytes) {
			out += hex[b >> 4];
			out += hex[b & 0xF];
		}
		return out;
	}

}  // namespace gold
