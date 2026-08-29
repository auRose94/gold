#pragma once

#include "types.hpp"

namespace gold {
	using std::string;
	using std::string_view;
	struct database : public object {
	 protected:
		static object& getPrototype();

	 public:
		database();
		database(initList config);

		var connect(list args = {});
		var disconnect(list args = {});
		var destroy(list args = {});

		var getDatabaseNames(list args = {});

		var createCollection(list args);
		var getCollection(list args);
	};

	struct collection : public object {
	 protected:
		friend struct model;
		static object& getPrototype();

	 public:
		collection();
		collection(database db, struct _mongoc_collection_t*);

		var addIndexes(list args);
		var dropIndex(list args);
		var deleteOne(list args);
		var deleteMany(list args);
		var findOne(list args);
		var findMany(list args);
		var updateOne(list args);
		var updateMany(list args);
		var insert(list args);
		var replace(list args);
		var rename(list args);
		var destroy(list args);

		void getDatabase(database& db);

		static var& setParentModel(var& args, object parent);
	};

	struct model : public object {
	 public:
		static object& getPrototype();
		model();
		model(collection col, object data = {});

		var save(list args = {});
		var remove(list args = {});
		var addOwners(list args);
		var removeOwners(list args);
		var isOwner(list args);

		void getDatabase(database& db);
		void getCollection(collection& col);
		string getID();
		static string newID();
		static bool validID(string_view id);

	};

	uint64_t getMonoTime();
}  // namespace gold