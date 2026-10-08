#pragma once

#include <database.hpp>
#include <server.hpp>

#include "session.hpp"
#include "user.hpp"

namespace gg {
	using namespace gold;

	using list = gold::list;
	using var = gold::var;

	struct upload : public model {
		static void createUploadFolder();
		static bool createUpload(string p, string_view data);

	 public:
		static object& getPrototype();
		upload();
		upload(object data);

		var getOwners(list args);

		static var uploadCard(gg::session sesh, user u, upload data);
		static var uploadMediaItem(
			gg::session sesh, user u, upload data);
		static list uploadOptions(
			gg::session sesh, user u, upload item);
		static list uploadFind(
			gg::session sesh, user u, obj data, list items);
		static list uploadList(
			gg::session sesh, user u, obj filter, list results);
		static list uploadIndex(gg::session sesh, user u, upload b);
		static list uploadDelete(session s, user u, list data);
		static var cropperDialog();

		static void setRoutes(database, server);
		static var findOne(list args);
		static var findMany(list args);
	};
}  // namespace gg