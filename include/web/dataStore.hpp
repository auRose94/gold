#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "types.hpp"

namespace gold {

	/**
	 * Embedded document-oriented data store backend. Backends persist
	 * gold objects (JSON/BSON) keyed by "_id". The existing
	 * database/collection/model facade sits on top of this interface, so
	 * swapping engines is a config line ("backend": "file" | "mongo").
	 *
	 * Filters are gold objects of field -> value (equality); an object
	 * value `{"$in", [...]}` matches when the field (a list) intersects
	 * the given list. Update documents use `{"$set", {...}}`.
	 */
	class dataStore {
	 public:
		virtual ~dataStore() = default;

		/** Open (creating if needed) the database `dbName` under `path`. */
		virtual bool open(const std::string& dbName,
			const std::string& path) = 0;
		virtual void close() = 0;

		/** Names of all databases under the root path. */
		virtual std::vector<std::string> getDatabaseNames() = 0;

		virtual bool createCollection(const std::string& name) = 0;
		virtual std::vector<std::string> getCollectionNames() = 0;
		virtual bool renameCollection(const std::string& oldName,
			const std::string& newName) = 0;

		/** Find one matching document (empty object if none). */
		virtual object findOne(const std::string& collection,
			const object& filter) = 0;
		/** Find matching documents, up to `limit` (0 = all). */
		virtual list find(const std::string& collection,
			const object& filter, uint64_t limit = 0) = 0;

		/** Insert a document; ensures an "_id". Returns the stored doc. */
		virtual var insert(const std::string& collection,
			const object& doc) = 0;
		/** Apply a $set update to one matching doc. Returns the updated
		 * doc or null if none matched. */
		virtual var updateOne(const std::string& collection,
			const object& filter, const object& update) = 0;
		/** Apply a $set update to all matching docs. Returns a
		 * {"modifiedCount", n} reply. */
		virtual var updateMany(const std::string& collection,
			const object& filter, const object& update) = 0;
		/** Delete one matching doc. Returns {"deletedCount", 0|1}. */
		virtual var deleteOne(const std::string& collection,
			const object& filter) = 0;
		/** Delete all matching docs. Returns {"deletedCount", n}. */
		virtual var deleteMany(const std::string& collection,
			const object& filter) = 0;
		/** Replace the content of one matching doc. Returns the new doc. */
		virtual var replace(const std::string& collection,
			const object& filter, const object& doc) = 0;

		/** Indexes are advisory for the "file" backend. Returns true. */
		virtual bool addIndexes(const std::string& collection,
			const object& keys) = 0;
		virtual bool dropIndex(const std::string& collection) = 0;

		/** Backend name, e.g. "file". */
		virtual const char* name() const = 0;
	};

	/** Create a data store backend by name. */
	dataStore* createDataStore(const std::string& name);
	void registerDataStore(const std::string& name,
		dataStore* (*factory)());

	/** Generate a document id (24 hex chars). */
	std::string dataStoreNewID();

}  // namespace gold