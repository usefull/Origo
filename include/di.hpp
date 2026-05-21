#pragma once

#include <memory>
#include <unordered_map>
#include <typeindex>
#include <functional>
#include <stdexcept>
#include <iostream>
#include <any>

namespace origo {

    enum class Lifecycle {
        Transient,  // Новый экземпляр при каждом resolve
        Singleton   // Один экземпляр на весь контейнер
    };

    class DI {
    private:
        // Фабрики для создания объектов
        std::unordered_map<std::type_index, std::function<std::shared_ptr<void>()>> factories;
        
        // Синглтоны
        std::unordered_map<std::type_index, std::shared_ptr<void>> singletons;

        // Регистрация в контейнере необходимых сервисов
        void init(const char* configPath);

    public:
        DI (const char* configPath) {
            init(configPath);
        }
        
        // ============================================
        // 1. РЕГИСТРАЦИЯ ТИПА С ЗАВИСИМОСТЯМИ
        // ============================================
        
        // Регистрация типа с автоматическим внедрением зависимостей
        template<typename T, typename... Deps>
        void registerType(Lifecycle lifecycle = Lifecycle::Transient) {
            std::type_index type(typeid(T));
            
            factories[type] = [this, lifecycle]() -> std::shared_ptr<void> {
                if (lifecycle == Lifecycle::Singleton) {
                    auto it = singletons.find(typeid(T));
                    if (it != singletons.end()) {
                        return it->second;
                    }
                    
                    auto instance = createInstance<T, Deps...>();
                    singletons[typeid(T)] = instance;
                    return instance;
                } else {
                    return createInstance<T, Deps...>();
                }
            };
        }
        
        // ============================================
        // 2. РЕГИСТРАЦИЯ ИНТЕРФЕЙСА
        // ============================================
        
        // Регистрация интерфейса с конкретной реализацией
        template<typename Interface, typename Implementation, typename... Deps>
        void registerInterface(Lifecycle lifecycle = Lifecycle::Transient) {
            std::type_index type(typeid(Interface));
            
            factories[type] = [this, lifecycle]() -> std::shared_ptr<void> {
                if (lifecycle == Lifecycle::Singleton) {
                    auto it = singletons.find(typeid(Interface));
                    if (it != singletons.end()) {
                        return it->second;
                    }
                    
                    auto instance = createInstance<Implementation, Deps...>();
                    singletons[typeid(Interface)] = instance;
                    return instance;
                } else {
                    return createInstance<Implementation, Deps...>();
                }
            };
        }
        
        // ============================================
        // 3. РЕГИСТРАЦИЯ ЭКЗЕМПЛЯРА
        // ============================================
        
        // Регистрация готового экземпляра (всегда как синглтон)
        template<typename T>
        void registerInstance(std::shared_ptr<T> instance) {
            std::type_index type(typeid(T));
            singletons[type] = instance;
            
            // Также регистрируем фабрику, которая возвращает этот экземпляр
            factories[type] = [instance]() -> std::shared_ptr<void> {
                return instance;
            };
        }
        
        // Регистрация экземпляра по lvalue-ссылке
        template<typename T>
        void registerInstance(const T& instance) {
            registerInstance(std::make_shared<T>(instance));
        }
        
        // ============================================
        // 4. РЕГИСТРАЦИЯ ФАБРИКИ
        // ============================================
        
        // Регистрация пользовательской фабрики
        template<typename T>
        void registerFactory(std::function<std::shared_ptr<T>()> factory, Lifecycle lifecycle = Lifecycle::Transient) {
            std::type_index type(typeid(T));
            
            if (lifecycle == Lifecycle::Singleton) {
                factories[type] = [this, factory]() -> std::shared_ptr<void> {
                    auto it = singletons.find(typeid(T));
                    if (it != singletons.end()) {
                        return it->second;
                    }
                    
                    auto instance = factory();
                    singletons[typeid(T)] = instance;
                    return instance;
                };
            } else {
                factories[type] = [factory]() -> std::shared_ptr<void> {
                    return factory();
                };
            }
        }
        
        // ============================================
        // 5. РАЗРЕШЕНИЕ ЗАВИСИМОСТЕЙ
        // ============================================
        
        // Получение экземпляра типа
        template<typename T>
        std::shared_ptr<T> resolve() {
            std::type_index type(typeid(T));
            
            auto it = factories.find(type);
            if (it == factories.end()) {
                throw std::runtime_error(
                    std::string("Type not registered: ") + typeid(T).name()
                );
            }
            
            auto result = std::static_pointer_cast<T>(it->second());
            if (!result) {
                throw std::runtime_error(
                    std::string("Failed to resolve type: ") + typeid(T).name()
                );
            }
            
            return result;
        }
        
        // Получение экземпляра по ссылке (удобно для lvalue)
        template<typename T>
        T& resolveRef() {
            auto ptr = resolve<T>();
            if (!ptr) {
                throw std::runtime_error("Failed to resolve reference");
            }
            return *ptr;
        }
        
        // ============================================
        // 6. ВСПОМОГАТЕЛЬНЫЕ МЕТОДЫ
        // ============================================
        
        // Проверка, зарегистрирован ли тип
        template<typename T>
        bool isRegistered() const {
            return factories.find(typeid(T)) != factories.end();
        }
        
        // Очистка всех синглтонов
        void clearSingletons() {
            singletons.clear();
        }
        
        // Получение количества зарегистрированных типов
        size_t registeredCount() const {
            return factories.size();
        }

    private:
        // ============================================
        // 7. ВНУТРЕННИЕ МЕТОДЫ СОЗДАНИЯ
        // ============================================
        
        // Создание экземпляра с внедрением зависимостей
        // Поддерживает конструкторы с параметрами: T*, T&, shared_ptr<T>, const shared_ptr<T>&
        template<typename T, typename... Deps>
        std::shared_ptr<T> createInstance() {
            return std::make_shared<T>(getDependency<Deps>()...);
        }
        
        // Получение зависимости в нужном формате
        template<typename Dep>
        auto getDependency() {
            //using PureType = std::decay_t<Dep>;
            
            // Если ожидается shared_ptr<X>
            if constexpr (is_shared_ptr_v<Dep>) {
                using ElementType = typename Dep::element_type;
                return resolve<ElementType>();
            }
            // Если ожидается ссылка X& или const X&
            else if constexpr (std::is_reference_v<Dep>) {
                using ElementType = std::remove_reference_t<Dep>;
                auto ptr = resolve<ElementType>();
                if constexpr (std::is_const_v<std::remove_reference_t<Dep>>) {
                    return std::cref(*ptr);
                } else {
                    return std::ref(*ptr);
                }
            }
            // Если ожидается указатель X*
            else if constexpr (std::is_pointer_v<Dep>) {
                using ElementType = std::remove_pointer_t<Dep>;
                auto ptr = resolve<ElementType>();
                return ptr.get();
            }
            // Если ожидается сам объект X (по значению)
            else {
                auto ptr = resolve<Dep>();
                return *ptr;
            }
        }
        
        // Helper для проверки shared_ptr
        template<typename T>
        struct is_shared_ptr : std::false_type {};
        
        template<typename T>
        struct is_shared_ptr<std::shared_ptr<T>> : std::true_type {};
        
        template<typename T>
        static constexpr bool is_shared_ptr_v = is_shared_ptr<T>::value;
    };
}
