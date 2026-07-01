#include "pcolonist/gameplay/PlayerVitals.hpp"

#include <stdexcept>

void runPlayerVitalsTests() {
    pcolonist::PlayerVitals vitals;
    vitals.update(pcolonist::PlayerVitalsContext{
        60.0F,
        1.0F,
        1.0F,
        1.0F,
        37.0F,
        0.0F,
        false,
    });
    if (vitals.state().thirst >= 100.0F || vitals.state().hunger >= 100.0F || vitals.state().fatigue <= 0.0F) {
        throw std::runtime_error("PlayerVitals must drain thirst/hunger and accumulate fatigue over time");
    }

    pcolonist::PlayerVitalsState critical;
    critical.thirst = 0.0F;
    critical.hunger = 0.0F;
    critical.fatigue = 100.0F;
    critical.bodyTemperature = 34.8F;
    vitals.applyState(critical);
    const float healthBeforeDamage = vitals.state().health;
    vitals.update(pcolonist::PlayerVitalsContext{
        10.0F,
        1.0F,
        1.0F,
        1.0F,
        34.8F,
        0.0F,
        false,
    });
    if (!vitals.state().dehydrated || !vitals.state().starving || !vitals.state().hypothermia) {
        throw std::runtime_error("PlayerVitals must expose critical condition flags");
    }
    if (vitals.state().health >= healthBeforeDamage) {
        throw std::runtime_error("PlayerVitals must damage the player under critical survival conditions");
    }

    vitals.drinkCleanWater(2);
    vitals.eat(40.0F);
    vitals.restNearHeat(20.0F);
    if (vitals.state().thirst <= 0.0F || vitals.state().hunger <= 0.0F || vitals.state().fatigue >= 100.0F) {
        throw std::runtime_error("PlayerVitals recovery actions must improve survival resources");
    }

    pcolonist::PlayerVitalsState invalid;
    invalid.health = 220.0F;
    invalid.thirst = -10.0F;
    invalid.hunger = 170.0F;
    invalid.fatigue = -20.0F;
    invalid.sicknessRisk = 180.0F;
    vitals.applyState(invalid);
    if (vitals.state().health != 100.0F
        || vitals.state().thirst != 0.0F
        || vitals.state().hunger != 100.0F
        || vitals.state().fatigue != 0.0F
        || vitals.state().sicknessRisk != 100.0F) {
        throw std::runtime_error("PlayerVitals must clamp restored state");
    }
}
