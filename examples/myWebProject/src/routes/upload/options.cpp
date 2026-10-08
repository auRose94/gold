#include "bootstrap.hpp"
#include "upload.hpp"
namespace gg {
	using namespace std;
	list upload::uploadOptions(gg::session sesh, user u, upload item) {
		// A minimal edit-confirmation page for an updated upload: the
		// media card plus its stored fields.
		using namespace gold;
		auto fields = list({});
		for (auto it = item.begin(); it != item.end(); ++it)
			fields.pushVar(
				var(it->first + ": " + it->second.getString()));

		auto content = list{
			uploadMediaItem(sesh, u, item),
			fields,
		};
		return content;
	}
}  // namespace gg