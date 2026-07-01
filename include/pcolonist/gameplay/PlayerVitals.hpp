#pragma once

namespace pcolonist {

struct PlayerVitalsState {
    float health = 100.0F;
    float thirst = 100.0F;
    float hunger = 100.0F;
    float fatigue = 0.0F;
    float bodyTemperature = 37.0F;
    float sicknessRisk = 0.0F;
    bool sick = false;
    bool dehydrated = false;
    bool starving = false;
    bool exhausted = false;
    bool hypothermia = false;
    bool hyperthermia = false;
};

struct PlayerVitalsContext {
    float deltaTime = 0.0F;
    float thirstModifier = 1.0F;
    float hungerModifier = 1.0F;
    float fatigueModifier = 1.0F;
    float targetBodyTemperature = 37.0F;
    float stormStrength = 0.0F;
    bool sicknessExposure = false;
};

class PlayerVitals {
public:
    void update(const PlayerVitalsContext& context);
    void drinkCleanWater(int units = 1);
    void eat(float nutrition);
    void restNearHeat(float seconds);
    void applyState(const PlayerVitalsState& state);

    [[nodiscard]] const PlayerVitalsState& state() const;

private:
    void updateConditionFlags();
    void applyDamageAndRecovery(float deltaTime);

    PlayerVitalsState state_;
};

} // namespace pcolonist
