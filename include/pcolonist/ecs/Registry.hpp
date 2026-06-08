#pragma once

#include <cstdint>
#include <memory>
#include <stdexcept>
#include <typeindex>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace pcolonist {

using Entity = std::uint32_t;
inline constexpr Entity nullEntity = 0;

class Registry {
public:
    Entity create() {
        const Entity entity = nextEntity_++;
        entities_.insert(entity);
        return entity;
    }

    void destroy(Entity entity) {
        if (!entities_.erase(entity)) {
            return;
        }
        for (auto& [type, componentStorage] : storages_) {
            static_cast<void>(type);
            componentStorage->erase(entity);
        }
    }

    [[nodiscard]] bool alive(Entity entity) const {
        return entities_.contains(entity);
    }

    [[nodiscard]] std::size_t size() const {
        return entities_.size();
    }

    template <typename Component, typename... Arguments>
    Component& emplace(Entity entity, Arguments&&... arguments) {
        if (!alive(entity)) {
            throw std::out_of_range("Cannot add component to dead entity");
        }
        return storage<Component>().components
            .insert_or_assign(entity, Component{std::forward<Arguments>(arguments)...})
            .first->second;
    }

    template <typename Component>
    void remove(Entity entity) {
        if (auto* componentStorage = findStorage<Component>()) {
            componentStorage->components.erase(entity);
        }
    }

    template <typename Component>
    [[nodiscard]] bool has(Entity entity) const {
        const auto* componentStorage = findStorage<Component>();
        return componentStorage != nullptr && componentStorage->components.contains(entity);
    }

    template <typename Component>
    Component& get(Entity entity) {
        return storage<Component>().components.at(entity);
    }

    template <typename Component>
    const Component& get(Entity entity) const {
        const auto* componentStorage = findStorage<Component>();
        if (componentStorage == nullptr) {
            throw std::out_of_range("Component storage does not exist");
        }
        return componentStorage->components.at(entity);
    }

    template <typename First, typename... Rest, typename Callback>
    void each(Callback&& callback) {
        auto* firstStorage = findStorage<First>();
        if (firstStorage == nullptr) {
            return;
        }

        for (auto& [entity, first] : firstStorage->components) {
            if ((has<Rest>(entity) && ...)) {
                std::forward<Callback>(callback)(entity, first, get<Rest>(entity)...);
            }
        }
    }

private:
    struct IStorage {
        virtual ~IStorage() = default;
        virtual void erase(Entity entity) = 0;
    };

    template <typename Component>
    struct Storage final : IStorage {
        void erase(Entity entity) override {
            components.erase(entity);
        }

        std::unordered_map<Entity, Component> components;
    };

    template <typename Component>
    Storage<Component>& storage() {
        const std::type_index type = typeid(Component);
        auto [iterator, inserted] = storages_.try_emplace(type);
        if (inserted) {
            iterator->second = std::make_unique<Storage<Component>>();
        }
        return *static_cast<Storage<Component>*>(iterator->second.get());
    }

    template <typename Component>
    Storage<Component>* findStorage() {
        const auto iterator = storages_.find(typeid(Component));
        return iterator == storages_.end() ? nullptr : static_cast<Storage<Component>*>(iterator->second.get());
    }

    template <typename Component>
    const Storage<Component>* findStorage() const {
        const auto iterator = storages_.find(typeid(Component));
        return iterator == storages_.end() ? nullptr : static_cast<const Storage<Component>*>(iterator->second.get());
    }

    Entity nextEntity_ = 1;
    std::unordered_set<Entity> entities_;
    std::unordered_map<std::type_index, std::unique_ptr<IStorage>> storages_;
};

} // namespace pcolonist
