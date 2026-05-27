#pragma once

#include <restinio/all.hpp>
#include <vector>
#include <mutex>
#include <thread>
#include <atomic>
#include <fstream>
#include <filesystem>
#include <chrono>
#include <set>
#include <deque> // Для истории треков
#include <random> // Для генерации случайных чисел
#include <algorithm> // Для std::remove_if

namespace origo {

    class RadioStation {
    public:
        using response_t = restinio::response_builder_t<restinio::chunked_output_t>;
        using client_ptr_t = std::shared_ptr<response_t>;
        using client_map_t = std::map<restinio::connection_id_t, client_ptr_t>;

        RadioStation(std::filesystem::path music_dir, size_t history_size = 5) 
            : m_music_dir(std::move(music_dir)), m_history_size(history_size), m_running(false) {
            
            // Загружаем плейлист один раз при инициализации
            load_playlist();
        }

        ~RadioStation() { stop(); }

        void add_client(restinio::connection_id_t conn_id, client_ptr_t client) {
            std::lock_guard<std::mutex> lock(m_clients_mutex);
    
            if (m_pending_removals.find(conn_id) != m_pending_removals.end()) {
                m_pending_removals.erase(conn_id);
                return;
            }
            
            m_clients[conn_id] = std::move(client);
            std::cout << "[Radio] Client added. Total current: " << m_clients.size() << std::endl;
        }

        void remove_client(restinio::connection_id_t conn_id) {
            std::lock_guard<std::mutex> lock(m_clients_mutex);
            auto it = m_clients.find(conn_id);
            if (it != m_clients.end()) {
                try {
                    it->second->append_chunk(restinio::string_view_t{});
                    it->second->flush();
                    it->second->done();
                } catch (...) {}
                m_clients.erase(it);
                std::cout << "[Radio] Client removed. Total current: " << m_clients.size() << std::endl;
            } else {
                m_pending_removals.insert(conn_id);
            }
        }

        void start() {
            m_running = true;
            m_broadcaster_thread = std::thread(&RadioStation::broadcast_loop, this);
        }

        void stop() {
            m_running = false;
            m_cv.notify_all();
            if (m_broadcaster_thread.joinable()) {
                m_broadcaster_thread.join();
            }
            std::lock_guard<std::mutex> lock(m_clients_mutex);
            m_clients.clear();
        }

    private:
        bool interruptible_sleep(std::chrono::microseconds duration) {
            std::unique_lock<std::mutex> lock(m_cv_mutex);
            return !m_cv.wait_for(lock, duration, [this] { return !m_running.load(); });
        }

        void broadcast_loop() {
            const size_t chunk_size = 2400;
            std::vector<char> buffer(chunk_size);

            while (m_running) {
                auto next_track_opt = get_next_track();
                if (!next_track_opt) {
                    if (!interruptible_sleep(std::chrono::seconds(2))) break;
                    continue;
                }

                std::ifstream file(*next_track_opt, std::ios::binary | std::ios::ate);
                if (!file.is_open()) continue;

                std::streamsize file_size = file.tellg();
                file.seekg(0, std::ios::beg);

                // --- Пропуск ID3v2 тега ---
                char id3_header[10];
                file.read(id3_header, 10);
                std::streamsize audio_start_pos = 0;
                
                if (file.gcount() == 10 && id3_header[0] == 'I' && id3_header[1] == 'D' && id3_header[2] == '3') {
                    uint32_t tag_size = ((id3_header[6] & 0x7F) << 21) |
                                        ((id3_header[7] & 0x7F) << 14) |
                                        ((id3_header[8] & 0x7F) << 7)  |
                                        (id3_header[9] & 0x7F);
                    audio_start_pos = tag_size + 10;
                }
                file.seekg(audio_start_pos, std::ios::beg);

                std::streamsize audio_size = file_size - audio_start_pos;
                double track_duration_sec = static_cast<double>(audio_size) / 24000.0; // 192 kbps -> 24000 байт/сек

                auto track_start_time = std::chrono::steady_clock::now();
                std::streamsize total_bytes_sent = 0;

                std::cout << "Now playing: " << *next_track_opt << std::endl;

                while (m_running && file.good()) {
                    file.read(buffer.data(), chunk_size);
                    auto bytes_read = file.gcount();

                    if (bytes_read > 0) {
                        send_chunk_to_all(buffer.data(), bytes_read);
                        total_bytes_sent += bytes_read;
                    }

                    // Динамический расчет задержки для поддержания точного битрейта
                    double expected_elapsed_sec = static_cast<double>(total_bytes_sent) / 24000.0;
                    auto actual_elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
                        std::chrono::steady_clock::now() - track_start_time);
                    double actual_elapsed_sec = actual_elapsed.count() / 1000000.0;

                    if (expected_elapsed_sec > actual_elapsed_sec) {
                        double sleep_time_sec = expected_elapsed_sec - actual_elapsed_sec;
                        auto sleep_duration = std::chrono::microseconds(static_cast<int64_t>(sleep_time_sec * 1000000.0));
                        if (!interruptible_sleep(sleep_duration)) break;
                    }
                }

                if (!m_running) break;

                // Финальное выравнивание в конце трека
                auto total_elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
                    std::chrono::steady_clock::now() - track_start_time);
                double total_elapsed_sec = total_elapsed.count() / 1000000.0;
                
                if (total_elapsed_sec < track_duration_sec) {
                    double final_sleep = track_duration_sec - total_elapsed_sec;
                    auto final_duration = std::chrono::microseconds(static_cast<int64_t>(final_sleep * 1000000.0));
                    if (!interruptible_sleep(final_duration)) break;
                }
            }
        }

        void send_chunk_to_all(const char* data, size_t size) {
            std::vector<client_ptr_t> local_copy;
            {
                std::lock_guard<std::mutex> lock(m_clients_mutex);
                if(m_clients.empty()) return;
                local_copy.reserve(m_clients.size());
                for (const auto& [id, client] : m_clients) {
                    local_copy.push_back(client);
                }
            }

            for (const auto& client : local_copy) {
                try {
                    client->append_chunk(restinio::string_view_t{data, size});
                    client->flush();
                } catch (...) {}
            }
        }

        void load_playlist() {
            for (const auto& entry : std::filesystem::directory_iterator(m_music_dir)) {
                if (entry.is_regular_file() && entry.path().extension() == ".mp3") {
                    m_full_playlist.push_back(entry.path());
                }
            }
            if (m_full_playlist.empty()) {
                std::cerr << "[Warning] No MP3 files found in directory: " << m_music_dir << std::endl;
            } else {
                std::cout << "[Playlist] Loaded " << m_full_playlist.size() << " tracks." << std::endl;
            }
        }

        std::optional<std::filesystem::path> get_next_track() {
            if (m_full_playlist.empty()) {
                return std::nullopt;
            }

            // Создаем список кандидатов, исключая недавно сыгранные
            std::vector<std::filesystem::path> candidates = m_full_playlist;
            candidates.erase(
                std::remove_if(candidates.begin(), candidates.end(),
                    [this](const std::filesystem::path& track_path) {
                        return std::find(m_recently_played.begin(), m_recently_played.end(), track_path)
                                != m_recently_played.end();
                    }),
                candidates.end()
            );

            // Если все треки попали в историю, сбрасываем её
            if (candidates.empty()) {
                m_recently_played.clear();
                candidates = m_full_playlist;
            }

            // Выбираем случайный трек из кандидатов
            thread_local std::mt19937 gen{std::random_device{}()};
            std::uniform_int_distribution<> dis(0, candidates.size() - 1);
            std::filesystem::path next_track = candidates[dis(gen)];

            // Обновляем историю
            m_recently_played.push_front(next_track);
            if (m_recently_played.size() > m_history_size) {
                m_recently_played.pop_back();
            }

            return next_track;
        }

        // --- Члены данных ---
        std::filesystem::path m_music_dir;
        std::atomic<bool> m_running;
        std::thread m_broadcaster_thread;
        
        client_map_t m_clients;
        std::mutex m_clients_mutex;
        std::set<restinio::connection_id_t> m_pending_removals;

        std::condition_variable m_cv;
        std::mutex m_cv_mutex;

        // НОВЫЕ ЧЛЕНЫ ДАННЫХ
        std::vector<std::filesystem::path> m_full_playlist; // Полный статический плейлист
        std::deque<std::filesystem::path> m_recently_played; // История последних N треков
        const size_t m_history_size; // Размер истории
    };
}