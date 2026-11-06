#include "CBAmethystRewardLayer.hpp"
#include <Geode/binding/CCCircleWave.hpp>
#include <Geode/modify/CurrencyRewardLayer.hpp>

// vro me when i took gd's source code of CurrenyRewardLayer and modify it for myself lmaoooo

class $modify(CBCurrencyRewardLayer, CurrencyRewardLayer) {
    struct Fields {
        int m_pendingAmethysts = 0;
    };

    void update(float dt) {
        if (m_fields->m_pendingAmethysts > 0 && m_objects && m_objects->count() == 0) return;
        CurrencyRewardLayer::update(dt);
    }
};

namespace {
    constexpr char const* kAmethystFrame = "CB_amethyst_001.png"_spr;
    constexpr int kMaxPieces = 60;
    constexpr int kMaxTrails = 20;
    constexpr float kUnitHeight = 12.f;
    constexpr float kMaxObjectScale = 3.f;
    constexpr float kCounterIconHeight = 16.f;
    constexpr ccColor4F kStartColor{1.f, 120.f / 255.f, 1.f, 1.f};
    constexpr ccColor4F kEndColor{120.f / 255.f, 0.f, 1.f, 1.f};
    constexpr ccColor4F kNoVariance{0.f, 0.f, 0.f, 0.f};
    constexpr ccColor3B kLabelColor{255, 150, 255};
    constexpr ccColor3B kWaveColor{255, 100, 255};

    float random01() {
        return static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
    }

    float randomSigned() {
        return random01() * 2.f - 1.f;
    }

    CCSprite* createAmethyst(float height) {
        auto sprite = CCSprite::createWithSpriteFrameName(kAmethystFrame);
        if (sprite) sprite->setScale(height / std::max(sprite->getContentHeight(), 1.f));
        return sprite;
    }

    struct ObjectSettings {
        float base;    // time before the first object starts flying to the counter
        float spread;  // random extra delay per object
        float scale;   // GD object scale
    };

    ObjectSettings settingsForCount(int count, float time) {
        ObjectSettings s{time, time * 0.3f, 1.f};
        if (count < 10) {
            s.spread = 0.2f;
            s.base = 0.8f;
        } else if (count > 400) {
            s.spread = 0.9f;
            s.scale = 0.8f;
        } else if (count > 300) {
            s.spread = 0.8f;
            s.scale = 0.8f;
        } else if (count > 200) {
            s.spread = 0.7f;
            s.scale = 0.8f;
        } else if (count > 100) {
            s.spread = 0.6f;
            s.scale = 0.9f;
        } else if (count > 50) {
            s.spread = 0.5f;
            s.scale = 0.9f;
        } else if (count > 30) {
            s.spread = 0.4f;
            s.scale = 0.95f;
        } else if (count > 20) {
            s.scale = 0.95f;
        }
        return s;
    }
}

CurrencyRewardLayer* CBAmethystRewardLayer::findGDRewardLayer(CCNode* root) {
    if (!root) return nullptr;
    if (auto layer = typeinfo_cast<CurrencyRewardLayer*>(root)) {
        if (layer->m_mainNode && layer->m_objects && layer->m_objects->count() > 0) return layer;
        return nullptr;
    }
    for (auto child : root->getChildrenExt()) {
        if (auto found = findGDRewardLayer(child)) return found;
    }
    return nullptr;
}

CBAmethystRewardLayer* CBAmethystRewardLayer::create(int previousBalance, int reward, CCPoint position, CurrencyRewardLayer* gdLayer, float time) {
    auto ret = new CBAmethystRewardLayer();
    if (ret && ret->init(previousBalance, reward, position, gdLayer, time)) {
        ret->autorelease();
        return ret;
    }
    delete ret;
    return nullptr;
}

bool CBAmethystRewardLayer::init(int previousBalance, int reward, CCPoint position, CurrencyRewardLayer* gdLayer, float time) {
    if (!CCLayer::init() || reward <= 0) return false;

    m_time = time;
    m_balance = previousBalance;
    m_finalBalance = previousBalance + reward;

    if (gdLayer) {
        attachCounter(gdLayer);
    } else {
        createCounter();
    }
    createPieces(reward, position);
    setPending(static_cast<int>(m_pieces.size()));

    this->scheduleUpdate();
    return true;
}

void CBAmethystRewardLayer::attachCounter(CurrencyRewardLayer* gdLayer) {
    m_gdLayer = gdLayer;
    auto mainNode = gdLayer->m_mainNode;
    float y = static_cast<float>(gdLayer->m_rewardCount * -30);
    gdLayer->m_rewardCount++;

    auto row = CCNode::create();
    mainNode->addChild(row, 10);

    m_label = CCCounterLabel::create(m_balance, "bigFont.fnt", FormatterType::Integer);
    m_label->setAnchorPoint({1.f, 1.f});
    m_label->setScale(0.6f);
    m_label->setColor(kLabelColor);
    m_label->setPosition({0.f, y});
    row->addChild(m_label, 10);

    m_icon = createAmethyst(kCounterIconHeight);
    if (!m_icon) m_icon = CCSprite::create();
    m_icon->setPosition(m_label->getPosition() + CCPoint{15.f, -0.5f - m_label->getScaledContentHeight() * 0.5f});
    row->addChild(m_icon, 3);

    if (auto sparkle = CCParticleSystemQuad::create("keyEffect.plist", false)) {
        sparkle->setTotalParticles(8);
        sparkle->setStartColor(kStartColor);
        sparkle->setEndColor(kEndColor);
        sparkle->setStartColorVar(kNoVariance);
        sparkle->setEndColorVar(kNoVariance);
        sparkle->setLife(1.f);
        sparkle->setLifeVar(0.2f);
        sparkle->setPosVar({12.f, 10.f});
        sparkle->setPositionType(kCCPositionTypeGrouped);
        sparkle->setPosition(m_icon->getPosition());
        row->addChild(sparkle, 100);
        sparkle->resetSystem();
    }
    float offscreen = CCDirector::sharedDirector()->getWinSize().width - mainNode->getPositionX() + 40.f;
    row->setPositionX(std::max(offscreen, 80.f));
    row->runAction(CCEaseBackOut::create(CCMoveTo::create(m_time * 0.4f, CCPointZero)));
}

CCPoint CBAmethystRewardLayer::iconPosition() {
    if (!m_icon || !m_icon->getParent()) return CCPointZero;
    return this->convertToNodeSpace(m_icon->getParent()->convertToWorldSpace(m_icon->getPosition()));
}

CBAmethystRewardLayer::~CBAmethystRewardLayer() {
    setPending(0);
}

void CBAmethystRewardLayer::setPending(int pieces) {
    if (m_gdLayer) static_cast<CBCurrencyRewardLayer*>(m_gdLayer.data())->m_fields->m_pendingAmethysts = pieces;
}

void CBAmethystRewardLayer::createCounter() {
    auto winSize = CCDirector::sharedDirector()->getWinSize();
    CCPoint corner{winSize.width - 40.f, winSize.height - 5.f};

    m_counter = CCNode::create();
    m_counter->setPosition(corner);
    this->addChild(m_counter, 4);

    m_label = CCCounterLabel::create(m_balance, "bigFont.fnt", FormatterType::Integer);
    m_label->setAnchorPoint({1.f, 1.f});
    m_label->setScale(0.6f);
    m_label->setColor(kLabelColor);
    m_counter->addChild(m_label, 10);

    float labelHeight = m_label->getScaledContentHeight();
    m_icon = createAmethyst(kCounterIconHeight);
    if (!m_icon) m_icon = CCSprite::create();
    m_icon->setPosition({15.f, -0.5f - labelHeight * 0.5f});
    m_counter->addChild(m_icon, 3);

    // sparkle on the counter icon
    if (auto sparkle = CCParticleSystemQuad::create("keyEffect.plist", false)) {
        sparkle->setTotalParticles(8);
        sparkle->setStartColor(kStartColor);
        sparkle->setEndColor(kEndColor);
        sparkle->setStartColorVar(kNoVariance);
        sparkle->setEndColorVar(kNoVariance);
        sparkle->setLife(1.f);
        sparkle->setLifeVar(0.2f);
        sparkle->setPosVar({12.f, 10.f});
        sparkle->setPositionType(kCCPositionTypeGrouped);
        sparkle->setPosition(m_icon->getPosition());
        m_counter->addChild(sparkle, 100);
        sparkle->resetSystem();
    }

    auto resting = m_counter->getPosition();
    m_counter->setPosition(resting + CCPoint{0.f, 40.f});
    m_counter->runAction(CCEaseInOut::create(CCMoveTo::create(m_time * 0.5f, resting), 2.f));
}

void CBAmethystRewardLayer::createPieces(int reward, CCPoint position) {
    auto settings = settingsForCount(reward, m_time);
    float delayBase = m_time * 0.1f + settings.base;
    bool single = reward == 1;

    for (int i = 0; i < std::min(reward, kMaxPieces); ++i) {
        auto sprite = createAmethyst(kUnitHeight);
        if (!sprite) continue;

        Piece piece;
        piece.sprite = sprite;
        piece.unitScale = sprite->getScale();
        piece.scale = settings.scale;
        sprite->setScale(piece.unitScale * piece.scale);

        auto start = position + CCPoint{randomSigned() * 10.f, randomSigned() * 10.f};
        sprite->setPosition(start);
        sprite->setRotation(randomSigned() * 30.f);
        this->addChild(sprite, 2);

        piece.velocity = CCPoint(random01() * 5.f * (random01() > 0.5f ? 1.f : -1.f), random01() * 3.f * 4.f);
        if (single) piece.velocity.y = (random01() * 0.2f + 2.5f) * 4.f;
        piece.remaining = random01() * settings.spread + delayBase;
        piece.targetOffset = randomSigned() * 10.f;

        if (i < kMaxTrails) {
            if (auto trail = CCParticleSystemQuad::create("fireballEffect.plist", false)) {
                trail->setTotalParticles(40);
                trail->setStartSize(trail->getStartSize() * 0.7f);  // orb sized fireball
                trail->setEndSize(trail->getEndSize() * 0.1f);
                trail->setStartSizeVar(0.f);
                trail->setEndSizeVar(0.f);
                trail->setStartColor(kStartColor);
                trail->setEndColor(kEndColor);
                trail->setStartColorVar(kNoVariance);
                trail->setEndColorVar(kNoVariance);
                trail->setLife(random01() * 0.2f + 0.2f);
                trail->setLifeVar(0.f);
                trail->setPositionType(kCCPositionTypeFree);
                trail->setAutoRemoveOnFinish(false);
                trail->setPosition(start);
                this->addChild(trail, -1);
                piece.trail = trail;
            }
        }
        m_pieces.push_back(piece);
    }
    if (m_pieces.empty()) return;

    for (int extra = reward - static_cast<int>(m_pieces.size()); extra > 0; --extra) {
        auto& piece = m_pieces[rand() % m_pieces.size()];
        piece.count++;
        if (piece.scale < kMaxObjectScale) {
            piece.scale += 0.1f;
            piece.sprite->setScale(piece.unitScale * piece.scale);
        }
    }

    // burst + circle wave where they come out
    if (auto burst = CCParticleSystemQuad::create("explodeEffect.plist", false)) {
        burst->setPositionType(kCCPositionTypeGrouped);
        burst->setPosition(position);
        burst->setAutoRemoveOnFinish(true);
        burst->setStartColor(kStartColor);
        burst->setEndColor(kEndColor);
        burst->setStartColorVar(kNoVariance);
        burst->setEndColorVar(kNoVariance);
        burst->setSpeed(burst->getSpeed() * 0.8f);
        burst->setStartSize(burst->getStartSize() * 0.5f);
        burst->setEndSize(burst->getEndSize() * 0.1f);
        this->addChild(burst, 99);
        burst->resetSystem();
    }
    if (auto wave = CCCircleWave::create(10.f, 110.f, 0.3f, false, true)) {
        wave->m_color = kWaveColor;
        wave->setPosition(position);
        this->addChild(wave, 9);
    }
}

void CBAmethystRewardLayer::pulseIcon() {
    if (!m_icon || m_icon->getActionByTag(99)) return;
    float scale = m_icon->getScale();
    auto pulse = CCSequence::create(
        CCEaseOut::create(CCScaleTo::create(0.08f, scale * 1.5f), 1.8f),
        CCEaseIn::create(CCScaleTo::create(0.08f, scale), 1.8f),
        nullptr);
    pulse->setTag(99);
    m_icon->runAction(pulse);
}

void CBAmethystRewardLayer::update(float dt) {
    auto winSize = CCDirector::sharedDirector()->getWinSize();
    float frames = dt * 60.f;
    float ceiling = winSize.height - 6.f;
    float gravity = frames * 0.4f;
    float acceleration = frames * 0.1f;

    for (auto it = m_pieces.begin(); it != m_pieces.end();) {
        auto& piece = *it;
        auto position = piece.sprite->getPosition();
        piece.remaining -= dt;

        if (piece.remaining <= 0.f) {
            auto target = iconPosition() + CCPoint{piece.targetOffset, 0.f};
            float distance = ccpDistance(position, target);
            if (distance > 5.f) {
                CCPoint delta = target - position;
                float divisor = (distance / 10.f + 10.f) / piece.accel;
                CCPoint move = delta / divisor * frames;
                // never overshoot
                if (std::abs(move.x) > std::abs(delta.x)) move.x = delta.x;
                if (std::abs(move.y) > std::abs(delta.y)) move.y = delta.y;
                piece.sprite->setPosition(position + move);
                piece.accel += acceleration / m_time;
            } else {
                // arrived: count it and shrink away
                if (piece.trail) {
                    piece.trail->stopSystem();
                    piece.trail->removeFromParent();
                }
                m_balance += piece.count;
                m_label->setTargetCount(m_balance);
                pulseIcon();
                piece.sprite->runAction(CCSequence::create(
                    CCScaleTo::create(0.1f, 0.1f), CCRemoveSelf::create(), nullptr));
                it = m_pieces.erase(it);
                setPending(static_cast<int>(m_pieces.size()));
                continue;
            }
        } else {
            piece.velocity.y -= gravity;
            CCPoint next = position + piece.velocity * frames;
            if (next.y > ceiling) {
                next.y = ceiling;
                piece.velocity.y *= -(0.6f + random01() * 0.3f);
            } else if (next.y < 6.f) {
                next.y = 6.f;
                piece.velocity.y *= -(0.6f + random01() * 0.3f);
                if (piece.velocity.y < 4.f) piece.velocity.y = random01() * 2.f + 4.f;
            }
            if (next.x < 6.f) {
                next.x = 6.f;
                piece.velocity.x *= -0.9f;
            } else if (next.x > winSize.width - 6.f) {
                next.x = winSize.width - 6.f;
                piece.velocity.x *= -0.9f;
            }
            piece.sprite->setPosition(next);
        }

        if (piece.trail) piece.trail->setPosition(piece.sprite->getPosition());
        ++it;
    }

    if (m_pieces.empty()) finish();
}

void CBAmethystRewardLayer::finish() {
    this->unscheduleUpdate();
    m_balance = m_finalBalance;
    m_label->setTargetCount(m_balance);
    m_label->fastUpdateCounter();

    if (m_gdLayer) {
        setPending(0);
        m_gdLayer = nullptr;
        this->removeFromParent();
        return;
    }

    m_counter->runAction(CCSequence::create(
        CCEaseInOut::create(CCScaleTo::create(0.2f, 1.2f), 2.f),
        CCEaseInOut::create(CCScaleTo::create(0.2f, 1.f), 2.f),
        CCEaseInOut::create(CCMoveBy::create(0.5f * m_time, {0.f, 50.f}), 2.f),
        CCCallFunc::create(this, callfunc_selector(CCNode::removeMeAndCleanup)),
        nullptr));
}
