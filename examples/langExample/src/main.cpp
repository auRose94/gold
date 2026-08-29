// langExample — a gold::lang script using gold data and the script facade.
//
//   gold::lang is a TypeScript-like language whose runtime IS gold: values
//   are var/object/list and scripts share gold objects with the host.

#include <goldjs.hpp>
#include <iostream>
#include <lang/lang.hpp>

using namespace gold;
using namespace std;

int main() {
	// A script with optional types, classes, arrows, template literals,
	// error handling, and iteration.
	const char* source = R"SCRIPT(
		const TAX = 0.08;
		class Item {
			constructor(name: string, price: number) {
				this.name = name;
				this.price = price;
			}
			total() {
				return this.price * (1 + TAX);
			}
		}
		const cart = [
			new Item("apple", 2.5),
			new Item("coffee", 8.0),
			new Item("book", 15.0),
		];
		let sum = 0;
		for (const item of cart) {
			sum += item.total();
		}
		const rounded = Math.round(sum * 100) / 100;
		function describe(items) {
			return items.map((i) => `${i.name}: ${i.price}`).join(", ");
		}
		const message = `Cart (${cart.length} items): ${describe(cart)} — total $${rounded}`;
		let handled;
		try {
			throw "example failure";
		} catch (e) {
			handled = `caught: ${e}`;
		}
		const summary = { message, handled, items: cart.length };
		summary;
	)SCRIPT";

	auto result = langRun(source, object(), false);
	if (result.isError()) {
		cerr << "script error: " << (string)*result.getError() << endl;
		return 1;
	}
	auto out = result.getObject();
	cout << "message:  " << out.getString("message") << endl;
	cout << "handled:  " << out.getString("handled") << endl;
	cout << "items:    " << out.getInt64("items") << endl;

	// Embed a script via the `script` facade and share gold data.
	auto s = script({{"source", var(string(
		"function welcome(name) { return `Welcome, ${name}!`; }"))}});
	s.load();
	s.run();
	s.setGlobal({var(string("appName")), var(string("gold"))});
	auto greeting = s.call({var(string("welcome")), var(string("builder"))});
	cout << "facade:   " << (string)greeting << endl;

	// Evaluate an expression against gold data from the host.
	auto expr = s.eval({var(string("`${appName} ${welcome(\"script\")}`"))});
	cout << "eval:     " << (string)expr << endl;
	return 0;
}