#pragma once

#include <html.hpp>
#include <server.hpp>

#include "user.hpp"
#include "session.hpp"

namespace gg {
	using namespace gold;
	gold::HTML::ul navbar(user u, gg::session sesh, string current);
	gold::HTML::ul navbar(request req);
}