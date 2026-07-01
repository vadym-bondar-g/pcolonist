#include "pcolonist/gameplay/PlayerVitals.hpp"

#include <algorithm>

namespace {

float clampPercent(float value) {
    return std::clamp(value, 0.0F, 100.0F);
}

float nonNegative(float value) {
    return std::max(value, 0.0F);
}

} // namespace

namespace pcolonist {

void PlayerVitals::update(const PlayerVitalsContext& context) {
    const float deltaTime = nonNegative(context.deltaTime);
    if (deltaTime <= 0.0F || state_.health <= 0.0F) {
        updateConditionFlags();
        return;
    }

    const float stormModifier = 1.0F + std::clamp(context.stormStrength, 0.0F, 1.0F) * 0.18F;
    state_.thirst = clampPercent(state_.thirst - 0.11F * nonNegative(context.thirstModifier) * stormModifier * deltaTime);
    state_.hunger = clampPercent(state_.hunger - 0.045F * nonNegative(context.hungerModifier) * deltaTime);
    state_.fatigue = clampPercent(state_.fatigue + 0.055F * nonNegative(context.fatigueModifier) * stormModifier * deltaTime);

    const float temperatureBlend = std::clamp(deltaTime * 0.035F, 0.0F, 1.0F);
    state_.bodyTemperature += (context.targetBodyTemperature - state_.bodyTemperature) * temperatureBlend;

    state_.sicknessRisk += context.sicknessExposure ? 1.9F * deltaTime : -2.2F * deltaTime;
    state_.sicknessRisk = clampPercent(state_.sicknessRisk);
    if (state_.sicknessRisk >= 100.0F) {
        state_.sick = true;
    }
    if (!context.sicknessExposure && state_.thirst > 55.0F && state_.hunger > 45.0F) {
        state_.sick = state_.sicknessRisk > 20.0F;
    }

    updateConditionFlags();
    applyDamageAndRecovery(deltaTime);
}

void PlayerVitals::drinkCleanWater(int units) {
    state_.thirst = clampPercent(state_.thirst + 28.0F * static_cast<float>(std::max(units, 0)));
    state_.sicknessRisk = std::max(state_.sicknessRisk - 18.0F, 0.0F);
    updateConditionFlags();
}

void PlayerVitals::eat(float nutrition) {
    state_.hunger = clampPercent(state_.hunger + nonNegative(nutrition));
    updateConditionFlags();
}

void PlayerVitals::restNearHeat(float seconds) {
    state_.fatigue = clampPercent(state_.fatigue - nonNegative(seconds) * 0.75F);
    state_.bodyTemperature += (37.1F - state_.bodyTemperature) * 0.25F;
    updateConditionFlags();
}

void PlayerVitals::applyState(const PlayerVitalsState& state) {
    state_ = state;
    state_.health = clampPercent(state_.health);
    state_.thirst = clampPercent(state_.thirst);
    state_.hunger = clampPercent(state_.hunger);
    state_.fatigue = clampPercent(state_.fatigue);
    state_.sicknessRisk = clampPercent(state_.sicknessRisk);
    updateConditionFlags();
}

const PlayerVitalsState& PlayerVitals::state() const {
    return state_;
}

void PlayerVitals::updateConditionFlags() {
    state_.dehydrated = state_.thirst <= 0.1F;
    state_.starving = state_.hunger <= 0.1F;
    state_.exhausted = state_.fatigue >= 98.0F;
    state_.hypothermia = state_.bodyTemperature < 35.4F;
    state_.hyperthermia = state_.bodyTemperature > 39.0F;
}

void PlayerVitals::applyDamageAndRecovery(float deltaTime) {
    float damage = 0.0F;
    damage += state_.dehydrated ? 2.5F : 0.0F;
    damage += state_.starving ? 0.65F : 0.0F;
    damage += state_.exhausted ? 0.32F : 0.0F;
    damage += state_.hypothermia || state_.hyperthermia ? 0.9F : 0.0F;
    damage += state_.sick ? 0.35F : 0.0F;
    if (damage > 0.0F) {
        state_.health = clampPercent(state_.health - damage * deltaTime);
        return;
    }

    if (state_.thirst > 55.0F && state_.hunger > 55.0F && state_.fatigue < 45.0F) {
        state_.health = clampPercent(state_.health + 0.08F * deltaTime);
    }
}

} // namespace pcolonist
