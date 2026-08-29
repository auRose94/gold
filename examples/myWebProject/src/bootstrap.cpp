#include "bootstrap.hpp"

#include <iostream>

namespace gg {
	using namespace std;
	namespace bs {
		using namespace gold;
		using namespace gold::HTML;
		div row(gold::list content) {
			auto element = div({
				obj{{"class", "row"}},
			});
			element += content;
			return element;
		}

		div col(gold::list content) {
			auto element = div({
				obj{{"class", "col"}},
			});
			element += content;
			return element;
		}

		div formCheck(
			string name, string labelText, obj inputAtts) {
			auto inputAttributes = obj{
				{"type", "checkbox"},
				{"class", "form-check-input"},
				{"id", name},
				{"aria-describedby", name + "Help"},
				{"name", name},
			};
			inputAttributes.copy(inputAtts);
			auto inputElement = input({inputAttributes});
			auto element = div({
				obj{{"class", "form-group row"}},
				label({
					obj{{"class", "col-sm-5 col-form-label"},
							{"for", name}},
					labelText,
				}),
				div({obj{{"class", "col-sm"}}, inputElement}),
			});
			return element;
		}

		div formRadio(
			string name, string id, string labelText, string value,
			obj inputAtts) {
			auto inputAttributes = obj{
				{"type", "radio"}, {"class", "form-check-input"},
				{"id", id},        {"aria-describedby", name + "Help"},
				{"name", name},    {"value", value},
			};
			inputAttributes.copy(inputAtts);
			auto inputElement = input({inputAttributes});
			auto element = div({
				obj{{"class", "form-check form-check-inline"}},
				inputElement,
				label({
					obj{{"class", "form-check-label"}, {"for", id}},
					labelText,
				}),
			});
			return element;
		}

		div formInputRow(
			string type, string name, string labelText, bool req,
			string error, obj inputAtts) {
			auto hasError = error.size() > 0;
			auto inputClasses = string("form-control");
			if (hasError) inputClasses += " is-invalid";
			auto inputAttributes = obj{
				{"type", type}, {"class", inputClasses},
				{"id", name},   {"aria-describedby", name + "Help"},
				{"name", name}, {"required", req},
			};
			inputAttributes.copy(inputAtts);
			auto inputElement = input({inputAttributes});
			auto element = div({
				obj{{"class", "form-group row"}},
				label({
					obj{{"class", "col-sm-5 col-form-label"},
							{"for", name}},
					labelText,
				}),
				div({obj{{"class", "col-sm"}}, inputElement}),
			});
			if (hasError)
				element +=
					{div({obj{{"class", "invalid-feedback"}}, error})};
			return element;
		}

		div formInputColumn(
			string type, string name, string labelText, bool req,
			string error, object inputAtts) {
			auto hasError = error.size() > 0;
			auto inputClasses = string("form-control");
			if (hasError) inputClasses += " is-invalid";
			auto inputAttributes = obj{
				{"type", type}, {"class", inputClasses},
				{"id", name},   {"aria-describedby", name + "Help"},
				{"name", name}, {"required", req},
			};
			inputAttributes.copy(inputAtts);
			auto inputElement = input({inputAttributes});
			auto element = div({
				obj{{"class", "form-group"}},
				label({
					obj{{"class", "form-label"}, {"for", name}},
					labelText,
				}),
				inputElement,
			});
			if (hasError)
				element +=
					{div({obj{{"class", "invalid-feedback"}}, error})};
			return element;
		}

		div formFileInput(
			string id, string labelText, object inputAtts) {
			auto inputAttributes = obj{
				{"type", "file"},
				{"class", "form-control-file"},
				{"id", id},
				{"aria-describedby", id + "Help"},
			};
			inputAttributes.copy(inputAtts);
			auto inputElement = input({inputAttributes});
			auto element = div({
				obj{{"class", "form-group"}},
				label({
					obj{{"style", "margin-right: 16px;"}, {"for", id}},
					labelText,
				}),
				inputElement,
			});
			return element;
		}

		div formSelect(
			string name, string labelText, bool req, obj inputAtts,
			gold::list content) {
			auto selAtt = obj{
				{"class", "form-control"},
				{"id", name},
				{"name", name},
				{"required", req},
				{"autocomplete", "off"},
			};
			selAtt.copy(inputAtts);
			auto sel = HTML::select({selAtt});
			sel += content;
			auto el = div({
				obj{{"class", "form-group row"}},
				label({
					obj{{"class", "col-sm-5 col-form-label"},
							{"for", name}},
					labelText,
				}),
				div({
					obj{{"class", "col-sm"}},
					sel,
				}),
			});
			return el;
		}

		option formSelectOption(
			string title, string value, bool selected) {
			auto o = option({
				obj{{"value", value},
						{"selected",
						 selected ? gold::var("true") : gold::var(false)}},
				title,
			});
			return o;
		}
	}  // namespace bs
}  // namespace gg