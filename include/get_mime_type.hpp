#include <iostream>
#include <string>
#include <unordered_map>
#include <algorithm>
#include <filesystem>

namespace origo {
    inline std::string_view get_mime_type(const std::filesystem::path& path) {
        // Статическая таблица соответствия расширений и MIME-типов
        static const std::unordered_map<std::string_view, std::string_view> mime_map = {
            {".html", "text/html"},
            {".htm", "text/html"},
            {".css", "text/css"},
            {".js", "application/javascript"},
            {".json", "application/json"},
            {".xml", "application/xml"},
            {".txt", "text/plain"},
            {".png", "image/png"},
            {".jpg", "image/jpeg"},
            {".jpeg", "image/jpeg"},
            {".gif", "image/gif"},
            {".svg", "image/svg+xml"},
            {".ico", "image/vnd.microsoft.icon"},
            {".pdf", "application/pdf"},
            // Добавьте сюда нужные вам расширения
        };

        // 1. Извлекаем расширение файла (оно возвращается с точкой, например, ".txt")
        std::string extension = path.extension().string();
        
        // 2. Приводим расширение к нижнему регистру для case-insensitive поиска
        std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);
        
        // 3. Выполняем поиск в таблице
        auto it = mime_map.find(extension);
        
        // 4. Если нашли — возвращаем MIME-тип, иначе возвращаем значение по умолчанию
        if (it != mime_map.end()) {
            return it->second;
        }
        
        // Значение по умолчанию для неизвестных типов файлов
        return "application/octet-stream";
    }
}
