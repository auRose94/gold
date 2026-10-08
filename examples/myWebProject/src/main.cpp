using namespace std;
#include <database.hpp>
#include <iostream>
#include <server.hpp>
#include <thread>
#include <file.hpp>

#include "error.hpp"
#include "index.hpp"
#include "login.hpp"
#include "register.hpp"
#include "session.hpp"
#include "template.hpp"
#include "upload.hpp"
#include "user.hpp"

using namespace gold;
using namespace gg;

int main() {
	auto err = gold::var();
	auto serv = server(obj{});
	auto db = database(
		{{"name", "MyWebProject"}, {"appName", "my-web-project"}});
	if ((err = serv.setMountPoint({"./js", "./css", "./assets"}))
				.isError()) {
		cerr << "current working directory: "
				 << file::currentWorkingDir() << endl
				 << *err.getError() << endl;
		return 2;
	}
	if ((err = db.connect()).isError()) {
		cerr << *err.getError() << endl;
		return 1;
	}

	gg::session::getPrototype()["domain"] = "127.0.0.1:8080";

	setIndexRoute(db, serv);

	gg::session::setRoutes(db, serv);
	upload::setRoutes(db, serv);
	user::setRoutes(db, serv);

	serv.setErrorHandler({func([&](gold::list args) -> gold::var {
		auto req = args[0].getObject<request>();
		auto res = args[1].getObject<response>();
		gold::list content =
			errorPage({genericError("404 Page not found")});
		res.writeStatus({404});
		res.end({getTemplate(req, content)});
		return gold::var();
	})});

	serv.start();
	serv.destroy();
	db.destroy();

	return 0;
}