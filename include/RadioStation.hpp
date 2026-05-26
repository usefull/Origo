#pragma once

#include <restinio/all.hpp>
#include <vector>
#include <mutex>
#include <thread>
#include <atomic>
#include <fstream>
#include <filesystem>
#include <chrono>

namespace origo {

    class RadioStation {
    public:
        using response_t = restinio::response_builder_t<restinio::chunked_output_t>;
        using client_ptr_t = std::shared_ptr<response_t>;

        RadioStation(std::filesystem::path music_dir) 
            : m_music_dir(std::move(music_dir)), m_running(false) {}

        ~RadioStation() { stop(); }

        // Добавление слушателя
        void add_client(client_ptr_t client) {
            std::lock_guard<std::mutex> lock(m_clients_mutex);
            m_clients.push_back(client);
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
            // Ждем указанное время или пока m_running не станет false
            return !m_cv.wait_for(lock, duration, [this] { return !m_running.load(); });
        }

        void broadcast_loop() {
            const size_t chunk_size = 2400; 
            std::vector<char> buffer(chunk_size);

            while (m_running) {
                std::vector<std::filesystem::path> playlist;
                for (const auto& entry : std::filesystem::directory_iterator(m_music_dir)) {
                    if (entry.is_regular_file() && entry.path().extension() == ".mp3") {
                        playlist.push_back(entry.path());
                    }
                }

                if (playlist.empty()) {
                    if (!interruptible_sleep(std::chrono::seconds(2))) break;
                    continue; 
                }

                for (const auto& file_path : playlist) {
                    if (!m_running) break;

                    std::ifstream file(file_path, std::ios::binary | std::ios::ate);
                    if (!file.is_open()) continue;

                    // Вычисляем реальный размер файла и его чистую длительность
                    std::streamsize file_size = file.tellg();
                    file.seekg(0, std::ios::beg);

                    // Пропускаем ID3v2 тег, если он есть, чтобы плеер не заикался при старте
                    char id3_header[10];
                    file.read(id3_header, 10);
                    std::streamsize audio_start_pos = 0;
                    
                    if (file.gcount() == 10 && id3_header[0] == 'I' && id3_header[1] == 'D' && id3_header[2] == '3') {
                        // Извлекаем размер ID3-тега (синхробезопасный синтаксис MP3)
                        uint32_t tag_size = ((id3_header[6] & 0x7F) << 21) |
                                            ((id3_header[7] & 0x7F) << 14) |
                                            ((id3_header[8] & 0x7F) << 7)  |
                                            (id3_header[9] & 0x7F);
                        audio_start_pos = tag_size + 10;
                    }
                    file.seekg(audio_start_pos, std::ios::beg);

                    // Расчет чистой длительности аудио (в секундах)
                    std::streamsize audio_size = file_size - audio_start_pos;
                    double track_duration_sec = static_cast<double>(audio_size) / 24000.0;

                    // Засекаем реальное время старта трека
                    auto track_start_time = std::chrono::steady_clock::now();
                    std::streamsize total_bytes_sent = 0;

                    std::cout << "Now playing: " << file_path << std::endl;

                    while (m_running && file.good()) {
                        file.read(buffer.data(), chunk_size);
                        auto bytes_read = file.gcount();

                        if (bytes_read > 0) {
                            send_chunk_to_all(buffer.data(), bytes_read);
                            total_bytes_sent += bytes_read;
                        }

                        // Динамический расчет задержки на основе переданных байт
                        // Сколько времени ДОЛЖНО БЫЛО пройти для этого объема байт:
                        double expected_elapsed_sec = static_cast<double>(total_bytes_sent) / 24000.0;
                        
                        // Сколько времени прошло НА САМОМ ДЕЛЕ:
                        auto actual_elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
                            std::chrono::steady_clock::now() - track_start_time);
                        double actual_elapsed_sec = actual_elapsed.count() / 1000000.0;

                        // Если мы бежим впереди паровоза — спим разницу
                        if (expected_elapsed_sec > actual_elapsed_sec) {
                            double sleep_time_sec = expected_elapsed_sec - actual_elapsed_sec;
                            auto sleep_duration = std::chrono::microseconds(static_cast<int64_t>(sleep_time_sec * 1000000.0));
                            
                            // Заменяем sleep_for на прерываемую функцию
                            if (!interruptible_sleep(sleep_duration)) {
                                break;
                            }
                        }
                    }

                    if (!m_running) break;

                    // Жесткое выравнивание в конце трека: если прочитали файл быстрее, 
                    // чем длится трек по таймеру, удерживаем поток до честного завершения времени
                    auto total_elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
                        std::chrono::steady_clock::now() - track_start_time);
                    double total_elapsed_sec = total_elapsed.count() / 1000000.0;
                    
                    if (total_elapsed_sec < track_duration_sec) {
                        double final_sleep = track_duration_sec - total_elapsed_sec;
                        auto final_duration = std::chrono::microseconds(static_cast<int64_t>(final_sleep * 1000000.0));
                        
                        if (!interruptible_sleep(final_duration)) {
                            break;
                        }
                    }
                }
            }
        }

        void send_chunk_to_all(const char* data, size_t size) {
            std::lock_guard<std::mutex> lock(m_clients_mutex);
            
            for (auto it = m_clients.begin(); it != m_clients.end();) {
                try {
                    // Отправляем чанк данных клиенту
                    (*it)->append_chunk(restinio::string_view_t{data, size});
                    (*it)->flush();
                    ++it;
                }
                catch (const std::exception&) {
                    // Если клиент отключился, удаляем его из списка вещания
                    it = m_clients.erase(it);
                }
            }
        }

        std::filesystem::path m_music_dir;
        std::atomic<bool> m_running;
        std::thread m_broadcaster_thread;
        
        std::vector<client_ptr_t> m_clients;
        std::mutex m_clients_mutex;

        // Новые примитивы для синхронизации прерываний
        std::condition_variable m_cv;
        std::mutex m_cv_mutex;
    };
}
