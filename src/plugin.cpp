#include "plugin.hpp"


Plugin* pluginInstance;

void init(Plugin* p) {
	pluginInstance = p;
	p->addModel(modelDemo);
}


namespace demo {

const NVGcolor INK = nvgRGB(0xff, 0xff, 0xff);
const NVGcolor CHIP = nvgRGBA(0x11, 0x11, 0x13, 0xf2);
const NVGcolor ACCENT = nvgRGB(0xe0, 0xa3, 0x53);

std::shared_ptr<window::Font> uiFont() {
	return APP->window->loadFont(asset::system("res/fonts/Nunito-Bold.ttf"));
}

} // namespace demo
