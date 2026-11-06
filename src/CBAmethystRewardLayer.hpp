#pragma once
#include <Geode/Geode.hpp>
#include <Geode/binding/CurrencyRewardLayer.hpp>
#include <Geode/binding/CCCounterLabel.hpp>

using namespace geode::prelude;

class CBAmethystRewardLayer : public CCLayer {
public:
    static CBAmethystRewardLayer* create(int previousBalance, int reward, CCPoint position, CurrencyRewardLayer* gdLayer = nullptr, float time = 1.f);

    static CurrencyRewardLayer* findGDRewardLayer(CCNode* root);

    void update(float dt) override;
    ~CBAmethystRewardLayer() override;

private:
    struct Piece {
        CCSprite* sprite = nullptr;
        CCParticleSystemQuad* trail = nullptr;
        CCPoint velocity;
        float targetOffset = 0.f;
        float remaining = 0.f;
        float accel = 1.f;
        int count = 1;
        float unitScale = 1.f;
        float scale = 1.f;
    };

    bool init(int previousBalance, int reward, CCPoint position, CurrencyRewardLayer* gdLayer, float time);
    void createCounter();
    void attachCounter(CurrencyRewardLayer* gdLayer);
    void createPieces(int reward, CCPoint position);
    CCPoint iconPosition();
    void pulseIcon();
    void setPending(int pieces);
    void finish();

    float m_time = 1.f;
    int m_balance = 0;
    int m_finalBalance = 0;
    Ref<CurrencyRewardLayer> m_gdLayer;
    CCNode* m_counter = nullptr;
    CCCounterLabel* m_label = nullptr;
    CCSprite* m_icon = nullptr;
    std::vector<Piece> m_pieces;
};
