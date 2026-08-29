#include "database.hpp"

#include <chrono>
#include <cstdint>
#include <file.hpp>
#include <iostream>
#include <sstream>
#include <string>
#include <web/dataStore.hpp>

namespace gold {
	using namespace std;

	obj& database::getPrototype() {
		static auto proto = obj{
			{"backend", "file"},
			{"path", "./data"},
			{"host", "mongodb://localhost:27017"},
			{"appName", "gold-app"},
			{"name", "db_name"},
			{"connect", method(&database::connect)},
			{"disconnect", method(&database::disconnect)},
			{"destroy", method(&database::destroy)},
			{"createCollection", method(&database::createCollection)},
			{"getCollection", method(&database::getCollection)},
			{"getDatabaseNames", method(&database::getDatabaseNames)},
		};
		return proto;
	}

	database::database() : obj() {}

	database::database(initList config) : obj(config) {
		setParent(getPrototype());
	}

	var database::connect(list) {
		auto backend = getString("backend", "file");
		auto store = createDataStore(backend);
		if (!store)
			return genericError("Unknown data store backend: " + backend);
		auto dbName = getString("name");
		auto path = getString("path", "./data");
		if (!store->open(dbName, path)) {
			delete store;
			return genericError("Failed to open data store");
		}
		setPtr("store", store);
		setString("backend", store->name());
		auto names = store->getDatabaseNames();
		cout << "data store '" << backend << "' ready at " << path << endl;
		if (!names.empty()) {
			auto namesList = list();
			for (auto& n : names) namesList.pushString(n);
			cout << "databases: " << namesList.getJSON() << endl;
		}
		return var();
	}

	var database::disconnect(list) {
		auto store = (dataStore*)getPtr("store");
		if (store) {
			store->close();
			delete store;
			setPtr("store", nullptr);
		}
		return var();
	}

	var database::destroy(list) {
		disconnect();
		return var();
	}

	var database::getDatabaseNames(list) {
		auto store = (dataStore*)getPtr("store");
		if (!store) return genericError("Not connected");
		auto names = store->getDatabaseNames();
		auto out = list();
		for (auto& n : names) out.pushString(n);
		return out;
	}

	var database::createCollection(list args) {
		auto store = (dataStore*)getPtr("store");
		if (!store) return genericError("Not connected");
		auto name = args[0].getString();
		if (name.empty()) return genericError("Missing collection name");
		if (!store->createCollection(name))
			return genericError("Failed to create collection");
		return var(collection(*this, store, name));
	}

	var database::getCollection(list args) {
		auto store = (dataStore*)getPtr("store");
		if (!store) return genericError("Not connected");
		auto name = args[0].getString();
		return var(collection(*this, store, name));
	}

	obj& collection::getPrototype() {
		static auto proto = obj({
			{"name", "undefined"},
			{"addIndexes", method(&collection::addIndexes)},
			{"dropIndex", method(&collection::dropIndex)},
			{"deleteOne", method(&collection::deleteOne)},
			{"deleteMany", method(&collection::deleteMany)},
			{"findOne", method(&collection::findOne)},
			{"findMany", method(&collection::findMany)},
			{"updateOne", method(&collection::updateOne)},
			{"updateMany", method(&collection::updateMany)},
			{"insert", method(&collection::insert)},
			{"replace", method(&collection::replace)},
			{"rename", method(&collection::rename)},
			{"destroy", method(&collection::destroy)},
		});
		return proto;
	}

	collection::collection() : obj() {}

	collection::collection(database d, dataStore* store, string name)
		: obj() {
		setParent(getPrototype());
		setPtr("store", store);
		setObject("database", d);
		setString("name", name);
	}

	var collection::addIndexes(list args) {
		auto store = (dataStore*)getPtr("store");
		if (!store) return genericError("Not connected");
		auto cName = args[0].getString();
		auto keys = args[1].getObject();
		if (cName.size() == 0)
			return genericError("Missing collection name for first arg");
		if (!keys) return genericError("Missing keys from second arg");
		if (!store->addIndexes(cName, keys))
			return genericError("Failed to create indexes");
		return true;
	}

	var collection::dropIndex(list args) {
		auto store = (dataStore*)getPtr("store");
		if (!store) return genericError("Not connected");
		auto indexName = args[0].getString();
		if (!store->dropIndex(indexName))
			return genericError("Failed to drop index");
		return true;
	}

	var collection::deleteOne(list args) {
		auto store = (dataStore*)getPtr("store");
		if (!store) return genericError("Not connected");
		auto selObj = args[0].getObject();
		if (!selObj) return genericError("Missing selector for first arg");
		return store->deleteOne(getString("name"), selObj);
	}

	var collection::deleteMany(list args) {
		auto store = (dataStore*)getPtr("store");
		if (!store) return genericError("Not connected");
		auto selObj = args[0].getObject();
		if (!selObj) return genericError("Missing selector for first arg");
		return store->deleteMany(getString("name"), selObj);
	}

	var collection::findOne(list args) {
		auto store = (dataStore*)getPtr("store");
		if (!store) return genericError("Not connected");
		auto selObj = args[0].getObject();
		if (!selObj) return genericError("Missing selector for first arg");
		auto found = store->findOne(getString("name"), selObj);
		if (found) return var(found);
		return var();
	}

	var collection::findMany(list args) {
		auto store = (dataStore*)getPtr("store");
		if (!store) return genericError("Not connected");
		auto selObj = args[0].getObject();
		if (!selObj) return genericError("Missing selector for first arg");
		uint64_t limit = 0;
		if (args.size() >= 2 && args[1].isObject()) {
			auto optObj = args[1].getObject();
			limit = optObj.getUInt64("limit", 0);
		}
		auto found = store->find(getString("name"), selObj, limit);
		return var(found);
	}

	var collection::updateOne(list args) {
		auto store = (dataStore*)getPtr("store");
		if (!store) return genericError("Not connected");
		auto selObj = args[0].getObject();
		auto upObj = args[1].getObject();
		if (!selObj) return genericError("Missing selector for first arg");
		if (!upObj) return genericError("Missing update object for second arg");
		return store->updateOne(getString("name"), selObj, upObj);
	}

	var collection::updateMany(list args) {
		auto store = (dataStore*)getPtr("store");
		if (!store) return genericError("Not connected");
		auto selObj = args[0].getObject();
		auto upObj = args[1].getObject();
		if (!selObj) return genericError("Missing selector for first arg");
		if (!upObj) return genericError("Missing update object for second arg");
		return store->updateMany(getString("name"), selObj, upObj);
	}

	var collection::insert(list args) {
		auto store = (dataStore*)getPtr("store");
		if (!store) return genericError("Not connected");
		auto objData = args[0].getObject();
		if (!objData) return genericError("Missing object for first arg");
		return store->insert(getString("name"), objData);
	}

	var collection::replace(list args) {
		auto store = (dataStore*)getPtr("store");
		if (!store) return genericError("Not connected");
		auto selObj = args[0].getObject();
		auto upObj = args[1].getObject();
		if (!selObj) return genericError("Missing selector for first arg");
		if (!upObj) return genericError("Missing update object for second arg");
		return store->replace(getString("name"), selObj, upObj);
	}

	var collection::rename(list args) {
		auto store = (dataStore*)getPtr("store");
		if (!store) return genericError("Not connected");
		auto newName = args[0].getString();
		if (!store->renameCollection(getString("name"), newName))
			return genericError("Failed to rename collection");
		setString("name", newName);
		return newName;
	}

	var collection::destroy(list) {
		setPtr("store", nullptr);
		return var();
	}

	void collection::getDatabase(database& db) {
		assignObject<database>("database", db);
	}

	var& collection::setParentModel(var& args, obj parent) {
		if (args.isList()) {
			auto arr = args.getList();
			for (auto it = arr.begin(); it != arr.end(); ++it)
				if (it->isObject()) {
					auto o = it->getObject();
					o.setParent(parent);
				}
		} else if (args.isObject()) {
			auto o = args.getObject();
			o.setParent(parent);
		}
		return args;
	}

	obj& model::getPrototype() {
		static auto proto = obj({
			{"_id", ""},
			{"updated", 0},
			{"created", 0},
			{"save", method(&model::save)},
			{"remove", method(&model::remove)},
		});
		return proto;
	}

	model::model() : obj() {}

	model::model(collection c, obj data) : obj() {
		setParent(getPrototype());
		copy(data);
		auto parent = getParent();
		if (parent) parent.setObject("col", c);
	}

	var model::save(list) {
		auto col = collection();
		getCollection(col);
		auto id = getString("_id");
		setUInt64("updated", getMonoTime());
		if (id.empty()) {
			id = dataStoreNewID();
			setString("_id", id);
			setUInt64("created", getMonoTime());
			auto res = col.insert({*this});
			if (res.isError())
				return res;
			else if (res.isObject()) {
				auto o = res.getObject();
				this->copy(o);
			}
			return *this;
		}
		auto res = col.updateOne(
			{obj{{"_id", id}}, obj{{"$set", *this}}, obj{}});
		if (res.isError())
			return res;
		else if (res.isObject()) {
			auto o = res.getObject();
			this->copy(o);
		}
		return *this;
	}

	var model::remove(list) {
		auto col = collection();
		getCollection(col);
		auto opt = obj{{"_id", getString("_id")}};
		return col.deleteOne({opt});
	}

	void model::getDatabase(database& db) {
		auto col = collection();
		getCollection(col);
		col.assignObject<database>("database", db);
	}

	void model::getCollection(collection& col) {
		assignObject<collection>("col", col);
	}

	string model::getID() { return getString("_id"); }

	var model::addOwners(list args) {
		auto owners = getList("owners");
		if (owners)
			for (auto it = args.begin(); it != args.end(); ++it) {
				string id = "";
				if (it->isObject())
					id = it->getObject().getString("_id");
				else if (it->isString())
					id = it->getString();
				if (validID(id) && owners.find(id) == owners.end())
					owners.pushString(id);
			}
		return gold::var();
	}

	var model::removeOwners(list args) {
		auto owners = getList("owners");
		if (owners)
			for (auto it = args.begin(); it != args.end(); ++it) {
				if (it->isObject()) {
					auto o = it->getObject();
					auto id = o.getString("_id");
					auto oit = owners.find(id);
					if (oit != owners.end()) owners.erase(oit);
				} else if (it->isString()) {
					auto id = it->getString();
					auto oit = owners.find(id);
					if (oit != owners.end()) owners.erase(oit);
				}
			}
		return gold::var();
	}

	var model::isOwner(list args) {
		auto owners = getList("owners");
		if (args.size() > 1) {
			auto ret = gold::list();
			for (auto it = args.begin(); it != args.end(); ++it) {
				if (it->isObject()) {
					auto o = it->getObject();
					auto id = o.getString("_id");
					auto oit = owners.find(id);
					if (oit != owners.end())
						ret.pushBool(true);
					else
						ret.pushBool(false);
				} else if (it->isString()) {
					auto id = it->getString();
					auto oit = owners.find(id);
					if (oit != owners.end())
						ret.pushBool(true);
					else
						ret.pushBool(false);
				} else {
					ret.pushBool(false);
				}
			}
			return ret;
		} else {
			// 1
			auto arg = args[0];
			if (arg.isObject()) {
				auto o = arg.getObject();
				auto id = o.getString("_id");
				auto oit = owners.find(id);
				if (oit != owners.end()) return true;
			} else if (arg.isString()) {
				auto id = arg.getString();
				auto oit = owners.find(id);
				if (oit != owners.end()) return true;
			}
		}
		return false;
	}

	string model::newID() { return dataStoreNewID(); }

	bool model::validID(string_view id) {
		if (id.length() > 24) return false;
		for (auto it = id.begin(); it != id.end(); ++it)
			if (!isalnum(*it)) return false;
		return true;
	}

	uint64_t getMonoTime() {
		return duration_cast<std::chrono::milliseconds>(
						 std::chrono::system_clock::now()
							 .time_since_epoch())
			.count();
	}

}  // namespace gold