#include <Geode/Geode.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/PauseLayer.hpp>
#include <Geode/modify/MenuLayer.hpp>
#include <algorithm>
#include <cmath>
#include <type_traits>

using namespace geode::prelude;

namespace {

constexpr int STEPS_PER_FRAME = 4;
constexpr float TOUCH = 30.f;
constexpr float V_TOL = 120.f;
constexpr int RING_IDS[] = {36, 84, 141, 1022, 1330, 1333, 1594, 1704, 1751};

struct State {
    bool pending = false;
    bool releaseQueued = false;
    bool firing = false;
    int button = 1;
    int stepsLeft = 0;
    float lastX = 0.f;
    float dxPerStep = 10.4f;
    std::vector<GameObject*> rings;
    int clicks = 0;
    CCLabelBMFont* counter = nullptr;
} S;

constexpr char const* KEYS[4] = {"cbfh-enabled", "fps-display", "practice-helper", "click-counter"};

bool isOn(char const* key) {
    return Mod::get()->getSettingValue<bool>(key);
}

int windowFrames() {
    return std::clamp(static_cast<int>(Mod::get()->getSettingValue<int64_t>("frame-window")), 1, 20);
}

void setWindowFrames(int n) {
    Mod::get()->setSettingValue<int64_t>("frame-window", std::clamp(n, 1, 20));
}

void applyGlobals() {
    CCDirector::get()->setDisplayStats(isOn("fps-display"));
    if (S.counter) S.counter->setVisible(isOn("click-counter"));
}

bool touchingRing(PlayerObject* p) {
    float px = p->getPositionX(), py = p->getPositionY();
    for (auto* r : S.rings) {
        float dx = r->getPositionX() - px;
        if (dx < -TOUCH) continue;
        if (dx > TOUCH) break;
        if (std::abs(r->getPositionY() - py) <= TOUCH) return true;
    }
    return false;
}

bool shouldBuffer(PlayerObject* p) {
    float px = p->getPositionX(), py = p->getPositionY();
    float reach = S.dxPerStep * static_cast<float>(windowFrames() * STEPS_PER_FRAME);
    bool ahead = false;
    for (auto* r : S.rings) {
        float dx = r->getPositionX() - px;
        if (dx < -TOUCH) continue;
        if (dx > reach + TOUCH) break;
        float dy = std::abs(r->getPositionY() - py);
        if (dy > V_TOL) continue;
        if (dx <= TOUCH && dy <= TOUCH) return false;
        ahead = true;
    }
    return ahead;
}

template <class T>
void forEachObject(T& objs, auto&& fn) {
    if constexpr (std::is_pointer_v<std::remove_reference_t<T>>) {
        for (auto* o : CCArrayExt<GameObject*>(objs)) fn(o);
    } else {
        for (auto* o : objs) fn(o);
    }
}

CCNode* makeWIcon() {
    auto label = CCLabelBMFont::create("W", "bigFont.fnt");
    label->setScale(0.9f);
    return CircleButtonSprite::create(label, CircleBaseColor::Green, CircleBaseSize::Medium);
}

}  // namespace

class WMenuPopup : public geode::Popup {
protected:
    CCLabelBMFont* m_windowLabel = nullptr;
    Slider* m_slider = nullptr;

    static CCPoint P(float x, float y) { return ccp(x - 160.f, y - 130.f); }

    CCScale9Sprite* card(float w, float h, float x, float y, ccColor3B col, int opacity) {
        auto bg = CCScale9Sprite::create("square02b_001.png", {0, 0, 80, 80});
        bg->setContentSize({w, h});
        bg->setColor(col);
        bg->setOpacity(opacity);
        m_mainLayer->addChildAtPosition(bg, Anchor::Center, P(x, y));
        return bg;
    }

    CCMenuItemToggler* toggler(int idx, float x, float y, float scale) {
        auto t = CCMenuItemToggler::createWithStandardSprites(this, menu_selector(WMenuPopup::onToggle), scale);
        t->setTag(idx);
        t->toggle(isOn(KEYS[idx]));
        m_buttonMenu->addChildAtPosition(t, Anchor::Center, P(x, y));
        return t;
    }

    void updateWindowLabel() {
        m_windowLabel->setString(fmt::format("Window: {} frame{}", windowFrames(), windowFrames() == 1 ? "" : "s").c_str());
    }

    bool init() {
        if (!Popup::init(320.f, 260.f)) return false;
        this->setTitle("W Menu");

        card(296.f, 100.f, 160.f, 172.f, {0, 120, 40}, 150);

        auto cbfhTitle = CCLabelBMFont::create("CBFH", "bigFont.fnt");
        cbfhTitle->setScale(0.6f);
        cbfhTitle->setAnchorPoint({0.f, 0.5f});
        m_mainLayer->addChildAtPosition(cbfhTitle, Anchor::Center, P(26.f, 206.f));

        auto cbfhSub = CCLabelBMFont::create("Auto frame-perfect clicks", "chatFont.fnt");
        cbfhSub->setScale(0.55f);
        cbfhSub->setAnchorPoint({0.f, 0.5f});
        m_mainLayer->addChildAtPosition(cbfhSub, Anchor::Center, P(26.f, 188.f));

        this->toggler(0, 276.f, 200.f, 0.8f);

        m_windowLabel = CCLabelBMFont::create("", "goldFont.fnt");
        m_windowLabel->setScale(0.5f);
        m_mainLayer->addChildAtPosition(m_windowLabel, Anchor::Center, P(160.f, 166.f));
        updateWindowLabel();

        m_slider = Slider::create(this, menu_selector(WMenuPopup::onSlider), 0.7f);
        m_slider->setValue((windowFrames() - 1) / 19.f);
        m_mainLayer->addChildAtPosition(m_slider, Anchor::Center, P(160.f, 144.f));

        auto left = CCSprite::createWithSpriteFrameName("navArrowBtn_001.png");
        left->setFlipX(true);
        left->setScale(0.5f);
        auto leftBtn = CCMenuItemSpriteExtra::create(left, this, menu_selector(WMenuPopup::onStep));
        leftBtn->setTag(-1);
        m_buttonMenu->addChildAtPosition(leftBtn, Anchor::Center, P(34.f, 144.f));

        auto right = CCSprite::createWithSpriteFrameName("navArrowBtn_001.png");
        right->setScale(0.5f);
        auto rightBtn = CCMenuItemSpriteExtra::create(right, this, menu_selector(WMenuPopup::onStep));
        rightBtn->setTag(1);
        m_buttonMenu->addChildAtPosition(rightBtn, Anchor::Center, P(286.f, 144.f));

        char const* names[3] = {"FPS Display", "Practice Helper", "Click Counter"};
        float xs[3] = {89.f, 231.f, 89.f};
        float ys[3] = {82.f, 82.f, 34.f};
        for (int i = 0; i < 3; i++) {
            card(138.f, 40.f, xs[i], ys[i], {0, 0, 0}, 110);
            auto l = CCLabelBMFont::create(names[i], "bigFont.fnt");
            l->setScale(0.32f);
            l->setAnchorPoint({0.f, 0.5f});
            m_mainLayer->addChildAtPosition(l, Anchor::Center, P(xs[i] - 62.f, ys[i]));
            this->toggler(i + 1, xs[i] + 50.f, ys[i], 0.6f);
        }
        return true;
    }

    void onToggle(CCObject* sender) {
        auto t = static_cast<CCMenuItemToggler*>(sender);
        bool nowOn = !t->isToggled();
        Mod::get()->setSettingValue<bool>(KEYS[t->getTag()], nowOn);
        applyGlobals();
    }

    void onSlider(CCObject* sender) {
        float v = static_cast<SliderThumb*>(sender)->getValue();
        setWindowFrames(1 + static_cast<int>(std::round(v * 19.f)));
        updateWindowLabel();
    }

    void onStep(CCObject* sender) {
        int n = std::clamp(windowFrames() + static_cast<CCNode*>(sender)->getTag(), 1, 20);
        setWindowFrames(n);
        m_slider->setValue((n - 1) / 19.f);
        updateWindowLabel();
    }

public:
    static WMenuPopup* create() {
        auto ret = new WMenuPopup();
        if (ret->init()) {
            ret->autorelease();
            return ret;
        }
        delete ret;
        return nullptr;
    }
};

class $modify(WMainMenu, MenuLayer) {
    bool init() {
        if (!MenuLayer::init()) return false;

        auto btn = CCMenuItemSpriteExtra::create(makeWIcon(), this, menu_selector(WMainMenu::onW));
        btn->setID("w-menu-button"_spr);
        if (auto menu = this->getChildByID("bottom-menu")) {
            menu->addChild(btn);
            menu->updateLayout();
        }

        static bool applied = false;
        if (!applied) {
            applied = true;
            applyGlobals();
        }
        return true;
    }

    void onW(CCObject*) { WMenuPopup::create()->show(); }
};

class $modify(WPause, PauseLayer) {
    void customSetup() {
        PauseLayer::customSetup();

        auto win = CCDirector::get()->getWinSize();
        auto menu = CCMenu::create();
        menu->setPosition({win.width - 30.f, win.height - 30.f});
        auto btn = CCMenuItemSpriteExtra::create(makeWIcon(), this, menu_selector(WPause::onW));
        btn->setID("w-menu-button"_spr);
        menu->addChild(btn);
        this->addChild(menu);
    }

    void onW(CCObject*) { WMenuPopup::create()->show(); }
};

class $modify(WPlayLayer, PlayLayer) {
    void setupHasCompleted() {
        PlayLayer::setupHasCompleted();

        S.rings.clear();
        S.pending = false;
        S.releaseQueued = false;
        S.clicks = 0;
        S.lastX = 0.f;

        forEachObject(m_objects, [](GameObject* o) {
            if (!o) return;
            for (int id : RING_IDS) {
                if (o->m_objectID == id) {
                    S.rings.push_back(o);
                    break;
                }
            }
        });
        std::sort(S.rings.begin(), S.rings.end(), [](GameObject* a, GameObject* b) {
            return a->getPositionX() < b->getPositionX();
        });

        auto win = CCDirector::get()->getWinSize();
        S.counter = CCLabelBMFont::create("Clicks: 0", "bigFont.fnt");
        S.counter->setScale(0.4f);
        S.counter->setAnchorPoint({0.f, 1.f});
        S.counter->setPosition({8.f, win.height - 8.f});
        S.counter->setOpacity(160);
        this->addChild(S.counter, 100);
        applyGlobals();
        if (isOn("practice-helper")) this->togglePracticeMode(true);
    }

    void resetLevel() {
        S.pending = false;
        S.releaseQueued = false;
        S.lastX = 0.f;
        S.clicks = 0;
        if (S.counter) S.counter->setString("Clicks: 0");
        PlayLayer::resetLevel();
    }

    void onQuit() {
        S.pending = false;
        S.rings.clear();
        S.counter = nullptr;
        PlayLayer::onQuit();
    }
};

class $modify(WBaseGameLayer, GJBaseGameLayer) {
    bool isLivePlay() {
        auto pl = PlayLayer::get();
        return pl && static_cast<GJBaseGameLayer*>(pl) == static_cast<GJBaseGameLayer*>(this) && m_player1;
    }

    void handleButton(bool down, int button, bool isPlayer1) {
        if (!this->isLivePlay() || !isPlayer1 || S.firing) {
            GJBaseGameLayer::handleButton(down, button, isPlayer1);
            return;
        }

        if (down) {
            S.clicks++;
            if (S.counter) S.counter->setString(fmt::format("Clicks: {}", S.clicks).c_str());
        }

        if (isOn("cbfh-enabled")) {
            if (down && !S.pending && shouldBuffer(m_player1)) {
                S.pending = true;
                S.releaseQueued = false;
                S.button = button;
                S.stepsLeft = windowFrames() * STEPS_PER_FRAME;
                return;
            }
            if (!down && S.pending && button == S.button) {
                S.releaseQueued = true;
                return;
            }
        }

        GJBaseGameLayer::handleButton(down, button, isPlayer1);
    }

    void processCommands(float dt, bool isHalfTick, bool isLastTick) {
        if (!isHalfTick && this->isLivePlay()) {
            float x = m_player1->getPositionX();
            float d = x - S.lastX;
            if (d > 0.f && d < 60.f) S.dxPerStep = d;
            S.lastX = x;

            if (S.pending) {
                S.stepsLeft--;
                if (touchingRing(m_player1) || S.stepsLeft <= 0) {
                    S.pending = false;
                    S.firing = true;
                    this->handleButton(true, S.button, true);
                    if (S.releaseQueued) this->handleButton(false, S.button, true);
                    S.firing = false;
                    S.releaseQueued = false;
                }
            }
        }
        GJBaseGameLayer::processCommands(dt, isHalfTick, isLastTick);
    }
};
