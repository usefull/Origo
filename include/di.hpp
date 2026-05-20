#pragma once
#include <memory>
#include <unordered_map>
#include <typeindex>
#include <functional>
#include <stdexcept>

#include <restinio/all.hpp>

#include "./config.hpp"

namespace origo {

    enum class Lifecycle {
        Transient,
        Singleton
    };

    class DI {
    private:
        // Хранилище фабрик для создания объектов
        std::unordered_map<std::type_index, std::function<void*()>> factories;
        
        // Хранилище синглтонов
        std::unordered_map<std::type_index, std::shared_ptr<void>> singletons;

        // Регистрация в контейнере необходимых сервисов
        void init(const char* configPath);
        
    public:
        DI (const char* configPath) {
            init(configPath);
        }
        
        // Регистрация типа с указанием жизненного цикла
        template<typename T, typename... Deps>
        void registerType(Lifecycle lifecycle = Lifecycle::Transient) {
            std::type_index type(typeid(T));
            
            factories[type] = [this, lifecycle]() -> void* {
                if (lifecycle == Lifecycle::Singleton) {
                    // Проверяем, есть ли уже синглтон
                    auto it = singletons.find(typeid(T));
                    if (it != singletons.end()) {
                        return it->second.get();
                    }
                    
                    // Создаем новый синглтон
                    auto instance = createInstance<T, Deps...>();
                    singletons[typeid(T)] = instance;
                    return instance.get();
                } 
                else { // Transient
                    auto instance = createInstance<T, Deps...>();
                    // Для transient нужно сохранить в shared_ptr, но вернуть сырой указатель
                    // Временно храним в локальном shared_ptr, который умрет после возврата
                    auto* ptr = instance.get();
                    m_transientStorage[typeid(T)] = std::move(instance);
                    return ptr;
                }
            };
        }
        
        // Регистрация интерфейса с реализацией
        template<typename Interface, typename Implementation, typename... Deps>
        void registerInterface(Lifecycle lifecycle = Lifecycle::Transient) {
            std::type_index type(typeid(Interface));
            
            factories[type] = [this, lifecycle]() -> void* {
                if (lifecycle == Lifecycle::Singleton) {
                    auto it = singletons.find(typeid(Interface));
                    if (it != singletons.end()) {
                        return it->second.get();
                    }
                    
                    auto instance = createInstance<Implementation, Deps...>();
                    singletons[typeid(Interface)] = instance;
                    return instance.get();
                }
                else {
                    auto instance = createInstance<Implementation, Deps...>();
                    m_transientStorage[typeid(Interface)] = std::move(instance);
                    return instance.get();
                }
            };
        }
        
        // Регистрация готового экземпляра (как синглтон)
        template<typename T>
        void registerInstance(std::shared_ptr<T> instance) {
            singletons[typeid(T)] = instance;
            
            // Также регистрируем фабрику, которая возвращает этот экземпляр
            factories[typeid(T)] = [instance]() -> void* {
                return instance.get();
            };
        }
        
        // Разрешение зависимости
        template<typename T>
        std::shared_ptr<T> resolve() {
            std::type_index type(typeid(T));
            
            auto it = factories.find(type);
            if (it == factories.end()) {
                throw std::runtime_error("Type not registered: " + std::string(typeid(T).name()));
            }
            
            //void* rawPtr = it->second();
            
            // Для синглтонов - берем из хранилища
            auto singletonIt = singletons.find(type);
            if (singletonIt != singletons.end()) {
                return std::static_pointer_cast<T>(singletonIt->second);
            }
            
            // Для transient - ищем во временном хранилище
            auto transientIt = m_transientStorage.find(type);
            if (transientIt != m_transientStorage.end()) {
                auto result = std::static_pointer_cast<T>(transientIt->second);
                m_transientStorage.erase(transientIt); // Удаляем из временного хранилища
                return result;
            }
            
            return nullptr;
        }
        
        // Очистка всех transient объектов (вызывать после каждого "запроса" если нужно)
        void clearTransients() {
            m_transientStorage.clear();
        }
        
    private:
        std::unordered_map<std::type_index, std::shared_ptr<void>> m_transientStorage;
        
        // Создание экземпляра с автоматическим разрешением зависимостей
        template<typename T, typename... Deps>
        std::shared_ptr<T> createInstance() {
            return std::make_shared<T>(resolve<Deps>()...);
        }
    };
}