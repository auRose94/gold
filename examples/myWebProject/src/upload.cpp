#include "upload.hpp"

#include <file.hpp>
#include <filesystem>
#include <image.hpp>

#include "error.hpp"
#include "session.hpp"
#include "template.hpp"
#include "user.hpp"
#include <goldjs.hpp>

namespace gg {
	using namespace std;
	using span = HTML::span;
	using div = gold::HTML::div;
	namespace fs = std::filesystem;
	object& upload::getPrototype() {
		static auto proto = obj({
			{"verified", false},
			{"nsfw", false},
			{"hide", false},
			{"src", ""},
			{"width", 0},
			{"height", 0},
			{"size", 0},
			{"hash", ""},
			{"desc", ""},
			{"attr", ""},
			{"mime", ""},
			{"license", ""},
			{"owners", gold::var(gold::list())},
			{"proto", model::getPrototype()},
		});
		return proto;
	}

	upload::upload() : model() {}

	var upload::uploadCard(session s, user u, upload b) {
		auto q = s.getQuery().getString();
		auto iconSrc = "/upload/" + b.getString("_id") + q;
		auto iconPage = "/upload/" + b.getString("icon") + q;
		auto item = div({
			atts{{"class", "card"}},
			a({
				atts{{"href", iconSrc}},
				img({
					atts{
						{"class", "mr-3"},
						{"src", iconPage},
						{"width", 64},
						{"height", 64},
					},
				}),
			}),
			div(
				{atts{{"class", "card-body"}},
				 h5({atts{{"class", "mt-0"}}, b.getString("name")}),
				 p({b.getString("shortDesc")})}),
		});
		return item;
	}

	var upload::uploadMediaItem(session s, user u, upload b) {
		auto q = s.getQuery().getString();
		auto iconSrc = "/upload/" + b.getString("_id") + q;
		auto iconPage = "/upload/" + b.getString("icon") + q;
		auto item = div({
			atts{{"class", "media d-inline-flex m-3"}},
			a({
				atts{{"href", iconSrc}},
				img({
					atts{
						{"class", "mr-3"},
						{"src", iconPage},
						{"width", 64},
						{"height", 64},
					},
				}),
			}),
			div(
				{atts{{"class", "media-body"}},
				 h5({atts{{"class", "mt-0"}}, b.getString("name")}),
				 p({b.getString("shortDesc")})}),
		});
		return item;
	}

	void upload::createUploadFolder() {
		auto upPath = "./upload";
		auto exists = fs::is_directory(upPath);
		if (!exists) {
			auto created = fs::create_directory(upPath);
			if (!created)
				cerr << "Failed to create upload directory. (" << upPath
						 << ")" << endl;
		}
	}

	bool upload::createUpload(string p, string_view data) {
		auto f = file::saveFile(p, data);
		if (f.isError()) {
			cerr << *f.getError() << endl;
			return false;
		}
		if (f.isObject()) return true;
		return false;
	}

	upload::upload(object data) : model() {
		copy(data);
		setParent(upload::getPrototype());
		createUploadFolder();
		auto src = getStringView("src");
		if (src.find("data:") != string::npos) {
			auto mime = string();
			auto bin = file::decodeDataURL(src, mime);
			auto strData = string_view((char*)bin.data(), bin.size());
			auto img = image({{"data", strData}});
			auto h = std::hash<string_view>();
			auto hash = to_string((uint64_t)h(strData));
			auto path = "./upload/" + hash + ".bin";
			auto created = createUpload(path, strData);
			if (created) {
				setString("src", path);
			}
			setUInt32("width", img.getUInt32("width"));
			setUInt32("height", img.getUInt32("height"));
			setUInt64("size", bin.size());
			setString("hash", hash);
			setString("mime", mime);
		}
	}

	var upload::getOwners(list args) {
		auto owners = getList("owners");
		auto returns = list({});
		auto criteria = jo("_id", owners);
		returns += user::findMany({criteria}).getList();
		return returns;
	}

	void upload::setRoutes(database db, server serv) {
		auto colRes = db.getCollection({"upload"});
		if (!colRes.isObject())
			cerr << "Could not get upload collection" << endl;
		getPrototype().setObject(
			"col", colRes.getObject<collection>());
		auto cacheControl = serv.getString("cacheControl");

		func getUpload = [](list args) -> gold::var {
			serveArgs(args, req, res);

			auto id = req.getParameter({0}).getString();
			auto item = upload::findOne({jo("_id", id)})
										.getObject<upload>();
			auto hide = item.getBool("hide");
			auto nsfw = item.getBool("nsfw");

			auto respond = [&]() -> gold::var {
				auto sesh =
					req.callMethod("getSession").getObject<session>();
				auto u = req.callMethod("getUser").getObject<user>();
				if (req.acceptingJSON()) {
					return res.end({item});
				} else if (req.acceptingHTML()) {
					return res.end({getTemplate(
						req, upload::uploadIndex(sesh, u, item))});
				}
				return var();
			};

			auto sesh =
				req.callMethod("getSession").getObject<session>();
			if (sesh) {
				auto u = req.callMethod("getUser").getObject<user>();
				if (u) {
					if (hide && item.isOwner({u}))
						return respond();
					else if (!hide)
						return respond();
				}
			}
			if (!hide && !nsfw) return respond();

			return req.setYield({true});
		};

		func getUploadImage =
			[cacheControl](list args) -> gold::var {
			serveArgs(args, req, res);

			auto id = req.getParameter({0}).getString();
			auto item = upload::findOne({jo("_id", id)})
										.getObject<upload>();
			if (item) {
				auto dataURL = item.getString("src");
				auto mime = string();
				binary bin;
				if (dataURL.find("data:") != string::npos) {
					bin = file::decodeDataURL(dataURL, mime);
				}
				if (dataURL.find("./upload/") != string::npos) {
					mime = item.getString("mime");
					auto f = file::readFile(dataURL);
					if (f.isObject()) {
						auto fo = f.getObject<file>();
						bin = fo.getBinary("data");
					}
				}
				auto hide = item.getBool("hide");
				auto nsfw = item.getBool("nsfw");
				auto hash = item.getString("hash");
				auto chash = req.getHeader({"if-none-match"});

				if (hash.size() > 0 && hash.compare(chash) == 0) {
					res.writeStatus({304});
					res.writeHeader({"Cache-Control", cacheControl});
					res.end();
				} else {
					auto sesh =
						req.callMethod("getSession").getObject<session>();
					if (sesh) {
						auto u =
							req.callMethod("getUser").getObject<user>();
						if (u) {
							if (hide && item.isOwner({u})) {
								res.writeHeader({"Content-Type", mime});
								res.writeHeader(
									{"Cache-Control", cacheControl});
								res.writeHeader({"ETag", hash});
								return res.end({bin});
							} else if (!hide) {
								res.writeHeader({"Content-Type", mime});
								res.writeHeader(
									{"Cache-Control", cacheControl});
								res.writeHeader({"ETag", hash});
								return res.end({bin});
							}
						}
					}
					if (!hide && !nsfw) {
						res.writeHeader({"Content-Type", mime});
						res.writeHeader({"Cache-Control", cacheControl});
						res.writeHeader({"ETag", hash});
						return res.end({bin});
					}
				}
			}

			return req.setYield({true});
		};

		func getListUploads = [](list args) -> gold::var {
			serveArgs(args, req, res);

			auto sesh =
				req.callMethod("getSession").getObject<session>();
			if (sesh) {
				auto u = req.callMethod("getUser").getObject<user>();
				if (u) {
					auto find = obj{
						{"owners", jo("$in", list({u.getID()}))},
					};
					auto opt = obj({});
					auto resp = upload::findMany({find, opt});
					if (req.acceptingJSON()) {
						res.writeHeader(
							{"Content-Type", "application/json"});
						if (resp.isError()) {
							auto e = (string)*resp.getError();
							auto j = "{\"error\":" + e + "}";
							return res.end({j});
						} else if (resp.isList()) {
							auto l = resp.getList();
							auto j =
								jo("total", l.size(), "rows", l).getJSON();
							return res.end({j});
						}
					} else if (req.acceptingHTML()) {
						if (resp.isError()) {
							auto err = resp.getError();
							return res.end(
								{getTemplate(req, errorPage({*err}))});
						} else {
							auto items = resp.getList();
							return res.end({getTemplate(
								req, upload::uploadList(sesh, u, {}, items))});
						}
					}
				}
			}

			return req.setYield({true});
		};

		func putUpload = [](const list& pArgs) -> gold::var {
			using namespace HTML;
			serveArgs(pArgs, req, res);
			auto itemId = req.getParameter({1}).getString();

			func onDataCallback = [=](list args) -> gold::var {
				auto data = args[0].getString();
				auto req = args[2].getObject<request>();
				auto res = args[3].getObject<response>();
				auto sesh =
					req.callMethod("getSession").getObject<session>();
				auto u = req.callMethod("getUser").getObject<user>();
				if (req.isWWWFormURLEncoded()) {
					auto params = obj();
					obj::parseURLEncoded(data, params);

					auto itemRet =
						upload::findOne({jo("_id", itemId)});
					if (!itemRet.isError()) {
						auto item = itemRet.getObject<upload>();
						item.copy(params);
						auto saved = item.save();
						if (!saved.isError())
							return res.end({getTemplate(
								req, uploadOptions(sesh, u, item))});
						else
							return res.end(
								{getTemplate(req, errorPage({saved}))});
					} else
						return res.end({getTemplate(
							req, errorPage({"Upload not found."}))});
				} else
					return res.end({getTemplate(
						req,
						errorPage({"Could not parse encoded form."}))});
			};

			func onAbort = [](list args) -> gold::var {
				auto req = args[0].getObject<request>();
				auto res = args[1].getObject<response>();
				res.end({getTemplate(
					req, errorPage({genericError("Aborted.")}))});
				return gold::var();
			};

			auto sesh =
				req.callMethod("getSession").getObject<session>();
			if (sesh) {
				auto u = req.callMethod("getUser").getObject<user>();
				if (u) {
					res.onAborted({onAbort, req});
					res.onData({onDataCallback, req});
					return gold::var();
				}
			}

			return req.setYield({true});
		};

		serv.get({"/list/upload", getListUploads});
		serv.get({"/list/upload.json", getListUploads});
		serv.get({"/upload/:uploadID/image", getUploadImage});
		serv.get({"/upload/:uploadID", getUpload});

		serv.put({"/upload/:uploadID", putUpload});
	}

	var upload::findOne(list args) {
		auto col =
			upload::getPrototype().getObject<collection>("col");
		auto value = col.findOne(args);
		return collection::setParentModel(
			value, upload::getPrototype());
	}

	var upload::findMany(list args) {
		auto col =
			upload::getPrototype().getObject<collection>("col");
		auto value = col.findMany(args);
		return collection::setParentModel(
			value, upload::getPrototype());
	}

	var upload::cropperDialog() {
		return div({
			atts{
				{"class", "modal fade"},
				{"id", "cropperModal"},
				{"tabIndex", "-1"},
				{"role", "dialog"},
				{"aria-labelledby", "cropperTitle"},
				{"aria-hidden", true},
			},
			div({
				atts{
					{"class", "modal-dialog text-light bg-dark"},
					{"role", "document"},
				},
				div({
					atts{
						{"class", "modal-content text-light bg-dark"},
					},
					div({
						atts{{"class", "modal-header"}},
						h5(
							{atts{
								 {"class", "modal-title"},
								 {"id", "cropperTitle"},
							 },
							 "Crop before uploading..."}),
						button({
							atts{
								{"type", "button"},
								{"class", "close"},
								{"data-dismiss", "modal"},
								{"aria-label", "Close"},
							},
							span({
								atts{
									{"aria-hidden", true},
								},
								"&times;",
							}),
						}),
					}),
					div({
						atts{
							{"class", "modal-body"},
						},
						input({atts{
							{"type", "file"},
							{"class", "invisible"},
							{"id", "fileInputDummy"},
						}}),
						div({
							atts{{"id", "cropperContainer"}},
							div({atts{{"id", "cropper"}}}),
						}),
					}),
					div({
						atts{{"class", "modal-footer"}},
						button({
							atts{
								{"type", "button"},
								{"class", "btn btn-secondary"},
								{"data-dismiss", "modal"},
							},
							"Close",
						}),
						button({
							atts{
								{"type", "button"},
								{"class", "btn btn-primary"},
								{"id", "cropperModalAccept"}},
							"Accept",
						}),
					}),
				}),
			}),
		});
	}
}  // namespace gg