using namespace std;
#include <camera.hpp>
#include <component.hpp>
#include <engine.hpp>
#include <entity.hpp>
#include <graphics.hpp>
#include <iostream>
#include <mesh.hpp>
#include <mutex>
#include <random>
#include <sprite.hpp>
#include <string>
#include <texture.hpp>
#include <thread>
#include <transform.hpp>
#include <goldjs.hpp>

using namespace gold;

int main() {
	using list = gold::list;
	engine main =
		engine("MountainAndValley", "ConwaysGameOfLife");
	auto cam = main.getPrimaryCamera().getObject<camera>();
	auto camTrans = cam.getComponent({transform::getPrototype()})
										.getObject<gold::transform>();
	camTrans.setPosition({0, 0, 0});

	auto size = cam.getVar("size");
	auto width = size.getUInt32(0);
	auto height = size.getUInt32(1);
	auto itemsX = floor(width / 32);
	auto itemsY = floor(height / 32);

	cout << itemsX << "x" << itemsY << endl;

	auto circleTexture = gpuTexture(jo("path", "./assets/circle.dds", "flags", "min_anis;mag_point;mip_point"));

	component simulator;

	auto simUpdate = func([=](list args) -> var {
		auto self = args[0].getObject<component>();
		auto sprites = self.getList("sprites");
		auto last = self.getList("frame");
		auto frame = ja();
		for (auto y = 0u; y < itemsY; y++) {
			auto col = ja();
			for (auto x = 0u; x < itemsX; x++) {
				col.setBool(x, false);
			}
			frame.setList(y, col);
		}

		auto getLife = [=](auto x, auto y) -> bool {
			if ((x < itemsX && x >= 0) && (y < itemsY && y >= 0)) {
				auto xVector = last[y].getList();
				auto v = xVector[x].getBool();
				return v;
			}
			return false;
		};

		auto getSprite = [=](auto x, auto y) -> sprite {
			if ((x < itemsX && x >= 0) && (y < itemsY && y >= 0)) {
				auto xVector = sprites[y].getList();
				return xVector[x].template getObject<sprite>();
			}
			return sprite();
		};
		for (auto y = 0; y < itemsY; y++) {
			for (auto x = 0; x < itemsX; x++) {
				int nCount = 0;
				auto cSprite = getSprite(x, y);
				nCount += int(getLife(x + 0, y + 1));  // N
				nCount += int(getLife(x + 1, y + 1));  // NE
				nCount += int(getLife(x - 1, y + 1));  // NW
				nCount += int(getLife(x + 0, y - 1));  // S
				nCount += int(getLife(x + 1, y - 1));  // SE
				nCount += int(getLife(x - 1, y - 1));  // SW
				nCount += int(getLife(x + 1, y + 0));  // E
				nCount += int(getLife(x - 1, y + 0));  // W
				auto alive = getLife(x, y);
				if (nCount < 2)
					alive = false;
				else if (nCount == 3)
					alive = true;
				else if (nCount > 3)
					alive = false;
				auto aliveColor = self.getVar("aliveColor");
				auto deadColor = self.getVar("deadColor");
				if (alive)
					cSprite["color"] = aliveColor;
				else
					cSprite["color"] = deadColor;
				auto fRow = frame.getList(y);
				fRow.setBool(x, alive);
				frame.setList(y, fRow);
			}
		}
		self["frame"] = frame;
		return var();
	});

	simulator = component({
		{"aliveColor", vec4f(0, 1, 0, 1)},
		{"deadColor", vec4f(1, 0, 0, 1)},
		{"update", simUpdate},
		{"proto", component::getPrototype()},
	});

	auto simObj = entity({{"name", "sim"}});
	auto simCom = simulator;
	auto children = ja();
	auto sprites = ja();
	auto frame = ja();
	for (auto y = 0u; y < itemsY; y++) {
		auto fCol = ja();
		auto sCol = ja();
		for (auto x = 0u; x < itemsX; x++) {
			fCol.setBool(x, false);
			sCol.setNull(x);
		}
		frame.setList(y, fCol);
		sprites.setList(y, sCol);
	}
	std::random_device rd;
	std::mt19937 gen(rd());
	for (auto y = 0u; y < itemsY; y++) {
		for (auto x = 0u; x < itemsX; x++) {
			auto itemEnt = entity(jo());
			auto itemTrans = itemEnt.getTransform();
			auto halfX = itemsX / 2;
			auto halfY = (itemsY / 2) - 0.5;
			itemTrans.setPosition({x - halfX, y - halfY, 22});
			// ~30% random fill: dense grids collapse to a still-life in a
			// few generations, so start sparse enough to stay alive.
			auto alive =
				std::generate_canonical<double, 10>(gen) >= 0.70;

			auto aliveColor = simCom.getVar("aliveColor");
			auto deadColor = simCom.getVar("deadColor");
			auto lifeSprite = sprite({
				{"texture", circleTexture},
				{"size", vec2f(1.0, 0.4375)},
				{"color", alive ? aliveColor : deadColor},
			});
			itemEnt += {lifeSprite};
			children.pushObject(itemEnt);
			auto rowSprites = sprites.getList(y);
			rowSprites.setObject(x, lifeSprite);
			sprites.setList(y, rowSprites);
			auto rowFrame = frame.getList(y);
			rowFrame.setBool(x, alive);
			frame.setList(y, rowFrame);
		}
	}
	simCom["sprites"] = sprites;
	simCom["frame"] = frame;
	simObj += children;
	simObj += {simulator};
	main += {simObj};

	main.start();

	return 0;
}