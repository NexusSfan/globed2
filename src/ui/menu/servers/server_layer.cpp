#include "server_layer.hpp"

#include "switcher/server_switcher_popup.hpp"
#include <ui/menu/main/kofi_popup.hpp>
#include <ui/menu/credits/credits_popup.hpp>
#include <data/packets/server/connection.hpp>
#include <managers/account.hpp>
#include <managers/central_server.hpp>
#include <managers/game_server.hpp>
#include <managers/error_queues.hpp>
#include <ui/menu/main/globed_menu_layer.hpp>
#include <ui/menu/settings/settings_layer.hpp>
#include <net/manager.hpp>
#include <util/ui.hpp>
#include <util/format.hpp>

using namespace geode::prelude;

bool GlobedServersLayer::init() {
    if (!CCLayer::init()) return false;

    util::ui::prepareLayer(this, {60,60, 60});
    this->getChildByID("background")->removeFromParent();

    auto winSize = CCDirector::get()->getWinSize();

    auto* bg = util::ui::makeRepeatingBackground("game_bg_01_001.png", {40, 40, 40});
    this->addChild(bg);
    this->background = bg;

    GlobedAccountManager::get().autoInitialize();

    auto* buttonMenu = Build<CCMenu>::create()
        .layout(ColumnLayout::create()->setAutoScale(true)->setAxisAlignment(AxisAlignment::Start))
        .anchorPoint(0.f, 0.f)
        .pos(15.f, 20.f)
        .parent(this)
        .collect();

    // central server switcher
    Build<CCSprite>::createSpriteName("accountBtn_myLevels_001.png")
        .with([&](auto* spr) {
            util::ui::rescaleToMatch(spr, {43.f, 41.5f});
        })
        .intoMenuItem([this] {
            if (auto* popup = ServerSwitcherPopup::create()) {
                popup->m_noElasticity = true;
                popup->show();
            }
        })
        .scaleMult(1.15f)
        .parent(buttonMenu);

    // settings button
    Build<CCSprite>::createSpriteName("accountBtn_settings_001.png")
        .with([&](auto* spr) {
            util::ui::rescaleToMatch(spr, {43.f, 41.5f});
        })
        .intoMenuItem([](auto) {
            util::ui::switchToScene(GlobedSettingsLayer::create());
        })
        .scaleMult(1.15f)
        .id("btn-open-settings"_spr)
        .parent(buttonMenu);

    buttonMenu->updateLayout();

    // server list and signup layer
    Build<GlobedServerList>::create()
        .zOrder(2)
        .anchorPoint(0.f, 0.f)
        .parent(this)
        .id("server-list")
        .store(serverList);

    Build<GlobedSignupLayer>::create()
        .zOrder(2)
        .anchorPoint(0.f, 0.f)
        .pos(serverList->getPosition())
        .parent(this)
        .id("signup-list")
        .store(signupLayer);

    if (util::ui::getAspectRatio() < 1.6f) {
        serverList->setScaleX(0.9f);
        signupLayer->setScaleX(0.9f);
    }

    serverList->setPosition({winSize / 2 - serverList->getScaledContentSize() / 2});
    signupLayer->setPosition({winSize / 2 - signupLayer->getScaledContentSize() / 2});

    // right button menu
    auto* rightButtonMenu = Build<CCMenu>::create()
        .layout(
            ColumnLayout::create()
                ->setAutoScale(true)
                ->setGap(4.f)
                ->setAxisAlignment(AxisAlignment::Start)
        )
        .anchorPoint(1.f, 0.f)
        .pos(winSize.width - 15.f, 20.f)
        .parent(this)
        .id("right-button-menu"_spr)
        .collect();

    // kofi button
    Build<CCSprite>::createSpriteName("icon-kofi.png"_spr)
        .intoMenuItem([](auto) {
            GlobedKofiPopup::create()->show();
        })
        .scaleMult(1.15f)
        .id("btn-kofi"_spr)
        .parent(rightButtonMenu);

    // credits button
    Build<CCSprite>::createSpriteName("icon-credits.png"_spr)
        .intoMenuItem([](auto) {
            auto* popup = GlobedCreditsPopup::create();
            popup->m_noElasticity = true;
            popup->show();
        })
        .scaleMult(1.15f)
        .id("btn-credits"_spr)
        .parent(rightButtonMenu);

    rightButtonMenu->updateLayout();

    this->schedule(schedule_selector(GlobedServersLayer::updateServerList), 0.1f);
    this->schedule(schedule_selector(GlobedServersLayer::pingServers), 5.0f);

    this->requestServerList();

    return true;
}

void GlobedServersLayer::keyBackClicked() {
    util::ui::navigateBack();
}

void GlobedServersLayer::onExit() {
    CCLayer::onExit();
    this->cancelWebRequest();
}

void GlobedServersLayer::transitionToMainLayer() {
    util::ui::replaceScene(GlobedMenuLayer::create());
}

void GlobedServersLayer::updateServerList(float) {
    auto& am = GlobedAccountManager::get();
    auto& csm = CentralServerManager::get();
    auto& nm = NetworkManager::get();

    // if we are connected to a server, navigate away
    if (nm.established() && !typeinfo_cast<CCTransitionScene*>(CCScene::get()) && !transitioningAway) {
        auto* seq = CCSequence::create(
            CCDelayTime::create(0.2f),
            CCCallFunc::create(this, callfunc_selector(GlobedServersLayer::transitionToMainLayer)),
            nullptr
        );
        this->runAction(seq);
        transitioningAway = true;

        return;
    }

    // update ping of the active server, if any
    nm.updateServerPing();

    // if we do not have a session token from the central server, and are not in a standalone server, don't show game servers
    if (!csm.standalone() && !am.hasAuthKey()) {
        serverList->setVisible(false);
        signupLayer->setVisible(true);
        return;
    }

    signupLayer->setVisible(false);
    serverList->setVisible(true);

    // if we recently switched a central server, redo everything
    if (csm.recentlySwitched) {
        csm.recentlySwitched = false;
        this->cancelWebRequest();
        this->requestServerList();
    }

    // if there are pending changes, hard refresh the list and ping all servers
    auto& gsm = GameServerManager::get();
    if (gsm.pendingChanges) {
        gsm.pendingChanges = false;

        serverList->forceRefresh();

        this->pingServers(0.f);

        // also disconnect from the current server if it's gone
        auto activeId = gsm.getActiveId();
        if (!gsm.getServer(activeId).has_value()) {
            NetworkManager::get().disconnect(false);
        }

        return;
    }

    // if there were no pending changes, still update the server data (ping, players, etc.)
    serverList->softRefresh();
}

void GlobedServersLayer::requestServerList() {
    this->cancelWebRequest();

    auto& csm = CentralServerManager::get();

    if (csm.standalone()) {
        return;
    }

    auto centralUrl = csm.getActive();

    if (!centralUrl) {
        return;
    }

    auto request = WebRequestManager::get().fetchServers();

    requestListener.bind(this, &GlobedServersLayer::requestCallback);
    requestListener.setFilter(std::move(request));
}

void GlobedServersLayer::requestCallback(typename WebRequestManager::Event* event) {
    if (!event || !event->getValue()) return;

    auto result = std::move(*event->getValue());

    if (result.isErr()) {
        auto& gsm = GameServerManager::get();
        gsm.clearCache();
        gsm.clear();
        gsm.pendingChanges = true;

        ErrorQueues::get().error(fmt::format("Failed to fetch servers.\n\nReason: <cy>{}</c>", util::format::webError(result.unwrapErr())));

        return;
    }

    auto response = result.unwrap();

    auto& gsm = GameServerManager::get();
    gsm.updateCache(response);
    auto loadResult = gsm.loadFromCache();
    gsm.pendingChanges = true;

    if (loadResult.isErr()) {
        log::warn("failed to parse server list: {}", loadResult.unwrapErr());
        log::warn("{}", response);
        ErrorQueues::get().error(fmt::format("Failed to parse server list: <cy>{}</c>", loadResult.unwrapErr()));
    }
}

void GlobedServersLayer::cancelWebRequest() {
    requestListener.getFilter().cancel();
}

void GlobedServersLayer::pingServers(float) {
    auto& nm = NetworkManager::get();
    nm.pingServers();
}

GlobedServersLayer* GlobedServersLayer::create() {
    auto ret = new GlobedServersLayer();
    if (ret->init()) {
        ret->autorelease();
        return ret;
    }

    delete ret;
    return nullptr;
}