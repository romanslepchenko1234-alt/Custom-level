#include <Geode/Geode.hpp>
#include <Geode/modify/LevelSelectLayer.hpp>
#include <algorithm>

using namespace geode::prelude;

static bool g_openLast = false;

static std::vector<int> loadIDs() {
    return Mod::get()->getSavedValue<std::vector<int>>("custom-ids", {});
}

// ---------- Окно ввода ID ----------
class AddLevelPopup : public Popup<std::function<void()>>, public LevelDownloadDelegate {
protected:
    TextInput* m_input = nullptr;
    CCLabelBMFont* m_status = nullptr;
    std::function<void()> m_callback;

    bool setup(std::function<void()> cb) override {
        m_callback = std::move(cb);
        this->setTitle("Добавить уровень");

        m_input = TextInput::create(180.f, "ID уровня", "bigFont.fnt");
        m_input->setFilter("0123456789");
        m_input->setMaxCharCount(9);
        m_input->setPosition(m_mainLayer->getContentSize() / 2 + CCPoint{0, 15});
        m_mainLayer->addChild(m_input);

        m_status = CCLabelBMFont::create("", "bigFont.fnt");
        m_status->setScale(0.4f);
        m_status->setPosition(m_mainLayer->getContentSize().width / 2, 62.f);
        m_mainLayer->addChild(m_status);

        auto btn = CCMenuItemSpriteExtra::create(
            ButtonSprite::create("Добавить"), this, menu_selector(AddLevelPopup::onAdd)
        );
        m_buttonMenu->addChildAtPosition(btn, Anchor::Bottom, {0, 25});
        return true;
    }

    void onAdd(CCObject*) {
        auto str = m_input->getString();
        int id = numFromString<int>(str).unwrapOr(0);
        if (id <= 0) {
            m_status->setString("Неверный ID");
            return;
        }
        auto gm = GameLevelManager::get();
        if (auto saved = gm->getSavedLevel(id)) {
            this->finish(saved);
            return;
        }
        m_status->setString("Загрузка...");
        gm->m_levelDownloadDelegate = this;
        gm->downloadLevel(id, false);
    }

    void finish(GJGameLevel* level) {
        auto ids = loadIDs();
        int id = level->m_levelID.value();
        if (std::find(ids.begin(), ids.end(), id) == ids.end()) {
            ids.push_back(id);
        }
        Mod::get()->setSavedValue("custom-ids", ids);
        g_openLast = true;
        auto cb = m_callback;
        this->onClose(nullptr);
        if (cb) cb();
    }

    void levelDownloadFinished(GJGameLevel* level) override {
        GameLevelManager::get()->m_levelDownloadDelegate = nullptr;
        this->finish(level);
    }

    void levelDownloadFailed(int) override {
        GameLevelManager::get()->m_levelDownloadDelegate = nullptr;
        m_status->setString("Не удалось загрузить уровень");
    }

    ~AddLevelPopup() {
        auto gm = GameLevelManager::get();
        if (gm && gm->m_levelDownloadDelegate == this) {
            gm->m_levelDownloadDelegate = nullptr;
        }
    }

public:
    static AddLevelPopup* create(std::function<void()> cb) {
        auto ret = new AddLevelPopup();
        if (ret->initAnchored(280.f, 160.f, std::move(cb))) {
            ret->autorelease();
            return ret;
        }
        delete ret;
        return nullptr;
    }
};

// ---------- Страница с пользовательским уровнем ----------
class CustomLevelPage : public CCLayer {
    GJGameLevel* m_level = nullptr;

public:
    static CustomLevelPage* create(GJGameLevel* level) {
        auto ret = new CustomLevelPage();
        if (ret->init(level)) {
            ret->autorelease();
            return ret;
        }
        delete ret;
        return nullptr;
    }

    bool init(GJGameLevel* level) {
        if (!CCLayer::init()) return false;
        m_level = level;
        auto win = CCDirector::get()->getWinSize();
        this->setContentSize(win);

        auto name = CCLabelBMFont::create(level->m_levelName.c_str(), "bigFont.fnt");
        name->limitLabelWidth(win.width * 0.6f, 1.f, 0.3f);
        name->setPosition(win.width / 2, win.height / 2 + 50);
        this->addChild(name);

        auto by = CCLabelBMFont::create(
            fmt::format("by {}", std::string(level->m_creatorName)).c_str(), "goldFont.fnt"
        );
        by->setScale(0.7f);
        by->setPosition(win.width / 2, win.height / 2 + 20);
        this->addChild(by);

        auto idLabel = CCLabelBMFont::create(
            fmt::format("ID: {}", level->m_levelID.value()).c_str(), "chatFont.fnt"
        );
        idLabel->setPosition(win.width / 2, win.height / 2 + 2);
        this->addChild(idLabel);

        auto menu = CCMenu::create();
        auto spr = CCSprite::createWithSpriteFrameName("GJ_playBtn2_001.png");
        spr->setScale(0.9f);
        auto btn = CCMenuItemSpriteExtra::create(spr, this, menu_selector(CustomLevelPage::onPlay));
        menu->addChild(btn);
        menu->setPosition(win.width / 2, win.height / 2 - 50);
        this->addChild(menu);
        return true;
    }

    void onPlay(CCObject*) {
        auto scene = PlayLayer::scene(m_level, false, false);
        CCDirector::get()->replaceScene(CCTransitionFade::create(0.5f, scene));
    }
};

// ---------- Страница с плюсом ----------
class AddPage : public CCLayer {
public:
    static AddPage* create() {
        auto ret = new AddPage();
        if (ret->init()) {
            ret->autorelease();
            return ret;
        }
        delete ret;
        return nullptr;
    }

    bool init() override {
        if (!CCLayer::init()) return false;
        auto win = CCDirector::get()->getWinSize();
        this->setContentSize(win);

        auto menu = CCMenu::create();
        auto spr = CCSprite::createWithSpriteFrameName("GJ_plusBtn_001.png");
        auto btn = CCMenuItemSpriteExtra::create(spr, this, menu_selector(AddPage::onAdd));
        menu->addChild(btn);
        menu->setPosition(win.width / 2, win.height / 2);
        this->addChild(menu);

        auto label = CCLabelBMFont::create("Добавить уровень по ID", "goldFont.fnt");
        label->setScale(0.6f);
        label->setPosition(win.width / 2, win.height / 2 - 55);
        this->addChild(label);
        return true;
    }

    void onAdd(CCObject*) {
        AddLevelPopup::create([] {
            // Перезагружаем экран уровней, чтобы страница появилась
            auto scene = LevelSelectLayer::scene(0);
            CCDirector::get()->replaceScene(CCTransitionFade::create(0.3f, scene));
        })->show();
    }
};

// ---------- Хук экрана официальных уровней ----------
class $modify(MyLevelSelectLayer, LevelSelectLayer) {
    bool init(int page) {
        if (!LevelSelectLayer::init(page)) return false;

        int base = m_scrollLayer->m_extendedLayer->getChildrenCount();
        int added = 0;

        auto gm = GameLevelManager::get();
        for (int id : loadIDs()) {
            auto level = gm->getSavedLevel(id);
            if (!level) continue;
            m_scrollLayer->addPage(CustomLevelPage::create(level));
            added++;
        }
        m_scrollLayer->addPage(AddPage::create());

        if (g_openLast && added > 0) {
            m_scrollLayer->moveToPage(base + added - 1);
        }
        g_openLast = false;
        return true;
    }
};
