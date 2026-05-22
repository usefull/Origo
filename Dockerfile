# ============================================
# STAGE 1: Builder - сборка Release версии
# ============================================
FROM ubuntu:22.04 AS builder

# Установка инструментов сборки
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    git \
    ca-certificates \
    nlohmann-json3-dev \
    jq \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /build

# Копируем CMakeLists.txt
COPY CMakeLists.txt .
COPY copy_static_dirs.cmake .

# Создаем src директорию и копируем исходники
COPY src/ ./src/
COPY include/ ./include/

# Создаём директорию для сборки и запускаем cmake
RUN mkdir -p release && \
    cd release && \
    cmake .. -DCMAKE_BUILD_TYPE=Release && \
    cmake --build . --target origo -j $(nproc)

# Копируем весь контекст во временную папку внутри builder, чтобы было откуда брать папки
COPY . /tmp/context/

# 2. Парсим конфиг с помощью jq, создаем целевые папки и копируем их содержимое
RUN mkdir /build/static_dirs && \
    jq -r '.staticDirs | values[]' /tmp/context/origo.conf | while read -r dir; do \
        if [ -d "/tmp/context/$dir" ]; then \
            echo "Копирование папки: $dir" && \
            cp -r "/tmp/context/$dir" "/build/static_dirs/$dir"; \
        else \
            echo "Предупреждение: Папка $dir указана в конфиге, но отсутствует в контексте!"; \
        fi \
    done

# ============================================
# STAGE 2: Runtime - минимальный образ
# ============================================
FROM ubuntu:22.04 AS runtime

# Установка runtime-зависимостей (если нужны)
RUN apt-get update && apt-get install -y \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

# Создаём непривилегированного пользователя
RUN useradd -m -u 1000 -s /bin/bash origo

WORKDIR /app

# Копируем собранный бинарник из builder
COPY --from=builder /build/release/origo .

# Копируем конфигурационный файл
COPY origo.conf .

# --- ИЗМЕНЕНО: Копируем только те папки, которые отобрал jq ---
COPY --from=builder /build/static_dirs/ .

# Меняем владельца и делаем исполняемым
RUN chown origo:origo /app/origo && \
    chmod +x /app/origo

# Переключаемся на непривилегированного пользователя
USER origo

# Точка входа
ENTRYPOINT ["./origo"]