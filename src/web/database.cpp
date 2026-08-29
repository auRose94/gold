#include "database.hpp"

#include <mongoc.h>
#include <string.h>

#include <chrono>
#include <cstdint>
#include <file.hpp>
#include <iostream>
#include <sstream>
#include <vector>

namespace gold {
	using namespace std;

	typedef struct {
		union {
			struct {
				uint32_t stamp;
				uint32_t inc;
			} pair;
			uint64_t data;
		} u;
	} timeStamp;

	bson_t* newBSONFromObject(obj& obj) {
		auto data = obj.getBSON();
		auto reader =
			bson_reader_new_from_data(data.data(), data.size());
		bool eof;
		auto doc = bson_reader_read(reader, &eof);
		if (doc) return bson_copy(doc);
		return nullptr;
	}

	list listFromBSON(bson_t* b) {
		auto view = string_view((char*)bson_get_data(b), b->len);
		auto res = file::parseBSON(view);
		if (res.isList()) return res.getList();
		return list();
	}

	obj objectFromBSON(bson_t* b) {
		auto view = string_view((char*)bson_get_data(b), b->len);
		auto res = file::parseBSON(view);
		if (res.isObject()) return res.getObject();
		return obj();
	}

	var varFromBSON(bson_t* b) {
		if (bson_has_field(b, "0")) {
			// List?
			return var(listFromBSON(b));
		}
		return var(objectFromBSON(b));
	}

	obj& database::getPrototype() {
		static auto proto = obj{
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
		static bool inited = false;
		if (inited == false) {
			inited = true;
			mongoc_init();
		}
		bson_error_t error;
		auto uriString = getString("host");
		auto uri =
			mongoc_uri_new_with_error(uriString.c_str(), &error);
		if (!uri) {
			cerr << "failed to parse URI:	" << uriString << endl
					 << "error message:				" << error.message << endl;
			return genericError(error.message);
		}
		auto dbName = getString("name");
		mongoc_uri_set_database(uri, dbName.c_str());
		mongoc_uri_set_option_as_bool(uri, "retryreads", true);
		auto pool = mongoc_client_pool_new(uri);
		if (!pool) return genericError("failed to create pool");
		auto appName = getString("appName");
		mongoc_client_pool_set_appname(pool, appName.c_str());
		setPtr("pool", pool);
		auto namesReturn = getDatabaseNames();
		if (namesReturn.isList()) {
			auto dbNames = namesReturn.getList();
			cout << "connection made: " << uriString << endl
					 << "databases: " << dbNames.getJSON() << endl;
		} else {
			cerr << namesReturn << endl;
		}
		auto client = mongoc_client_pool_pop(pool);
		auto db =
			mongoc_client_get_database(client, dbName.c_str());
		mongoc_client_pool_push(pool, client);
		setPtr("handle", db);
		mongoc_uri_destroy(uri);
		return var();
	}

	var database::disconnect(list) {
		auto pool = (mongoc_client_pool_t*)getPtr("pool");
		auto db = (mongoc_database_t*)getPtr("handle");
		mongoc_database_destroy(db);
		mongoc_client_pool_destroy(pool);
		return var();
	}

	var database::destroy(list) {
		disconnect();
		mongoc_cleanup();
		return var();
	}

	var database::getDatabaseNames(list args) {
		auto pool = (mongoc_client_pool_t*)getPtr("pool");
		auto client = mongoc_client_pool_pop(pool);
		bson_t* opts = nullptr;
		if (args.size() > 1) {
			auto optsObj = args[0].getObject();
			opts = newBSONFromObject(optsObj);
		}
		bson_error_t error;
		auto strV = mongoc_client_get_database_names_with_opts(
			client, opts, &error);
		mongoc_client_pool_push(pool, client);
		if (opts) bson_destroy(opts);
		if (strV) {
			auto strings = list();
			auto i = 0;
			auto str = strV[i];
			while (str != nullptr) {
				auto found = strings.find(str);
				if (found == strings.end()) strings.pushString(str);
				str = strV[i++];
			}
			bson_strfreev(strV);
			return strings;
		}
		return genericError(error.message);
	}

	var database::createCollection(list args) {
		auto db = (mongoc_database_t*)getPtr("handle");
		auto name = args[0].getString();
		auto optsObj = args[1].getObject();
		bson_t* opts =
			optsObj ? newBSONFromObject(optsObj) : nullptr;
		bson_error_t error;
		auto col = (struct _mongoc_collection_t*)
			mongoc_database_create_collection(
				db, name.c_str(), opts, &error);
		if (col) return var(collection(*this, col));
		return genericError(error.message);
	}

	var database::getCollection(list args) {
		auto pool = (mongoc_client_pool_t*)getPtr("pool");
		auto client = mongoc_client_pool_pop(pool);
		auto name = args[0].getString();
		auto dbName = getString("name");
		auto col = (struct _mongoc_collection_t*)
			mongoc_client_get_collection(
				client, dbName.c_str(), name.c_str());
		mongoc_client_pool_push(pool, client);
		if (col) return collection(*this, col);
		return var();
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

	collection::collection(
		database d, struct _mongoc_collection_t* c)
		: obj() {
		setParent(getPrototype());
		setPtr("handle", c);
		setObject("database", d);
		setString(
			"name",
			mongoc_collection_get_name((mongoc_collection_t*)c));
	}

	var collection::addIndexes(list args) {
		auto handle = (mongoc_collection_t*)getPtr("handle");
		auto cName = args[0].getString();
		auto keys = args[1].getObject();
		auto optsObj = args[2].getObject();
		if (cName.size() == 0)
			return genericError(
				"Missing collection name for first arg");
		if (!keys)
			return genericError("Missing keys from second arg");
		auto bKeys = newBSONFromObject(keys);
		auto indexName =
			mongoc_collection_keys_to_index_string(bKeys);
		auto indexObj = obj{
			{"key", (keys)},
			{"name", (indexName)},
		};
		if (optsObj) indexObj.copy(optsObj);
		auto command = obj({
			{"createIndexes", cName},
			{
				"indexes",
				var(list{
					indexObj,
				}),
			},
		});
		bson_error_t error;
		auto cBson = newBSONFromObject(command);
		bson_t replyBson;
		auto success = mongoc_collection_write_command_with_opts(
			handle, cBson, 0, &replyBson, &error);
		bson_destroy(cBson);
		if (!success) return genericError(error.message);
		auto reply = varFromBSON(&replyBson);
		return reply;
	}

	var collection::dropIndex(list args) {
		auto handle = (mongoc_collection_t*)getPtr("handle");
		auto indexName = args[0].getString();
		bson_error_t err;
		if (mongoc_collection_drop_index(
					handle, indexName.c_str(), &err))
			return true;
		return var(genericError(err.message));
	}

	var collection::deleteOne(list args) {
		auto handle = (mongoc_collection_t*)getPtr("handle");
		auto selObj = args[0].getObject();
		auto optObj = args[1].getObject();
		if (!selObj)
			return genericError("Missing selector for first arg");
		bson_t* opts = optObj ? newBSONFromObject(optObj) : nullptr;
		auto sel = newBSONFromObject(selObj);
		bson_t replyBson;
		bson_error_t err;
		auto success = mongoc_collection_delete_one(
			handle, sel, opts, &replyBson, &err);
		bson_destroy(sel);
		if (opts) bson_destroy(opts);
		if (!success) {
			return genericError(err.message);
		}
		auto reply = varFromBSON(&replyBson);
		return reply;
	}

	var collection::deleteMany(list args) {
		auto handle = (mongoc_collection_t*)getPtr("handle");
		auto selObj = args[0].getObject();
		auto optObj = args[1].getObject();
		if (!selObj)
			return genericError("Missing selector for first arg");
		bson_t* opts = optObj ? newBSONFromObject(optObj) : nullptr;
		auto sel = newBSONFromObject(selObj);
		bson_t replyBson;
		bson_error_t err;
		auto success = mongoc_collection_delete_many(
			handle, sel, opts, &replyBson, &err);
		bson_destroy(sel);
		if (opts) bson_destroy(opts);
		if (!success) return genericError(err.message);
		auto reply = varFromBSON(&replyBson);
		return reply;
	}

	var collection::findOne(list args) {
		static auto def = obj(initList{{"limit", int32_t(1)}});
		auto handle = (mongoc_collection_t*)getPtr("handle");
		auto selObj = args[0].getObject();
		auto optObj = obj();
		if (args.size() >= 2) optObj = args[1].getObject();
		if (optObj)
			optObj.copy(def);
		else
			optObj = def;
		if (!selObj)
			return genericError("Missing selector for first arg");
		bson_t* opts = newBSONFromObject(optObj);
		auto sel = newBSONFromObject(selObj);
		bson_error_t err;
		auto cursor = mongoc_collection_find_with_opts(
			handle, sel, opts, nullptr);
		bson_destroy(sel);
		bson_destroy(opts);
		const bson_t* doc = nullptr;
		var resp;
		while (mongoc_cursor_next(cursor, &doc)) {
			auto resi = varFromBSON((bson_t*)doc);
			if (resi.isObject()) {
				auto respObj = resi.getObject();
				resp = respObj;
			}
		}
		if (mongoc_cursor_error(cursor, &err)) {
			mongoc_cursor_destroy(cursor);
			return genericError(err.message);
		}
		mongoc_cursor_destroy(cursor);
		return resp;
	}

	var collection::findMany(list args) {
		auto handle = (mongoc_collection_t*)getPtr("handle");
		auto selObj = args[0].getObject();
		auto optObj = args[1].getObject();
		if (!selObj)
			return genericError("Missing selector for first arg");
		bson_t* opts = optObj ? newBSONFromObject(optObj) : nullptr;
		auto sel = newBSONFromObject(selObj);
		bson_error_t err;
		auto cursor = mongoc_collection_find_with_opts(
			handle, sel, opts, nullptr);
		bson_destroy(sel);
		if (opts) bson_destroy(opts);
		const bson_t* doc = nullptr;
		auto response = list();
		while (mongoc_cursor_next(cursor, &doc)) {
			auto resi = varFromBSON((bson_t*)doc);
			if (resi.isList()) {
				auto arr = resi.getList();
				response.pushList(arr);
			} else if (resi.isObject()) {
				auto o = resi.getObject();
				response.pushObject(o);
			}
		}
		if (mongoc_cursor_error(cursor, &err)) {
			mongoc_cursor_destroy(cursor);
			auto msg = string(err.message);
			return genericError(msg);
		}
		mongoc_cursor_destroy(cursor);
		return response;
	}

	var collection::updateOne(list args) {
		auto handle = (mongoc_collection_t*)getPtr("handle");
		auto selObj = args[0].getObject();
		auto upObj = args[1].getObject();
		auto optObj = args[2].getObject();

		if (!selObj)
			return genericError("Missing selector for first arg");
		if (!upObj)
			return genericError(
				"Missing update object for second arg");
		bson_t* opts = optObj ? newBSONFromObject(optObj) : nullptr;
		auto sel = newBSONFromObject(selObj);
		auto up = newBSONFromObject(upObj);
		bson_t replyBson;
		bson_error_t err;
		auto success = mongoc_collection_update_one(
			handle, sel, up, opts, &replyBson, &err);
		bson_destroy(sel);
		bson_destroy(up);
		if (opts) bson_destroy(opts);
		if (!success) {
			return genericError(err.message);
		}
		auto reply = varFromBSON(&replyBson);
		return reply;
	}

	var collection::updateMany(list args) {
		auto handle = (mongoc_collection_t*)getPtr("handle");
		auto selObj = args[0].getObject();
		auto upObj = args[1].getObject();
		auto optObj = args[2].getObject();
		if (!selObj)
			return genericError("Missing selector for first arg");
		if (!upObj)
			return genericError(
				"Missing update object for second arg");
		bson_t* opts = optObj ? newBSONFromObject(optObj) : nullptr;
		auto sel = newBSONFromObject(selObj);
		auto up = newBSONFromObject(upObj);
		bson_t replyBson;
		bson_error_t err;
		auto success = mongoc_collection_update_many(
			handle, sel, up, opts, &replyBson, &err);
		bson_destroy(sel);
		bson_destroy(up);
		if (opts) bson_destroy(opts);
		if (!success) {
			return genericError(err.message);
		}
		auto reply = varFromBSON(&replyBson);
		return reply;
	}

	var collection::insert(list args) {
		auto handle = (mongoc_collection_t*)getPtr("handle");
		auto objData = args[0].getObject();
		auto optObj = obj();
		if (args.size() >= 2) optObj = args[1].getObject();
		if (!objData)
			return genericError("Missing object for first arg");
		auto obj = newBSONFromObject(objData);
		bson_t* opts = optObj ? newBSONFromObject(optObj) : nullptr;
		bson_t replyBson;
		bson_error_t err;
		auto success = mongoc_collection_insert_one(
			handle, obj, opts, &replyBson, &err);
		bson_destroy(obj);
		if (opts) bson_destroy(opts);
		if (!success) return genericError(err.message);
		auto reply = varFromBSON(&replyBson);
		return reply;
	}

	var collection::replace(list args) {
		auto handle = (mongoc_collection_t*)getPtr("handle");
		auto selObj = args[0].getObject();
		auto upObj = args[1].getObject();
		auto optObj = args[2].getObject();
		if (!selObj)
			return genericError("Missing selector for first arg");
		if (!upObj)
			return genericError(
				"Missing update object for second arg");
		bson_t* opts = optObj ? newBSONFromObject(optObj) : nullptr;
		auto sel = newBSONFromObject(selObj);
		auto up = newBSONFromObject(upObj);
		bson_t replyBson;
		bson_error_t err;
		auto success = mongoc_collection_replace_one(
			handle, sel, up, opts, &replyBson, &err);
		bson_destroy(sel);
		bson_destroy(up);
		if (opts) bson_destroy(opts);
		if (!success) {
			return genericError(err.message);
		}
		auto reply = varFromBSON(&replyBson);
		return reply;
	}

	var collection::rename(list args) {
		auto handle = (mongoc_collection_t*)getPtr("handle");
		auto db = database();
		getDatabase(db);
		auto dbName = db.getString("name");
		auto newName = args[0].getString();
		bson_error_t err;
		auto success = mongoc_collection_rename(
			handle, dbName.c_str(), newName.c_str(), false, &err);
		if (!success) {
			return genericError(err.message);
		}
		setString("name", newName);
		return newName;
	}

	var collection::destroy(list) {
		auto handle = (mongoc_collection_t*)getPtr("handle");
		mongoc_collection_destroy(handle);
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
			id = newID();
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

	string model::newID() {
		bson_oid_t oid;
		auto ss = stringstream();
		bson_oid_init(&oid, nullptr);
		for (uint64_t i = 0; i < 12; ++i)
			ss << hex << (int)oid.bytes[i];
		return ss.str();
	}

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