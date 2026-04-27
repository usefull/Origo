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
    && rm -rf /var/lib/apt/lists/*

WORKDIR /build

# Копируем CMakeLists.txt
COPY CMakeLists.txt .

# Создаем src директорию и копируем исходники
COPY src/ ./src/

# Создаём директорию для сборки и запускаем cmake
RUN mkdir -p release && \
    cd release && \
    cmake .. -DCMAKE_BUILD_TYPE=Release && \
    cmake --build . --target origo -j $(nproc)

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

# Меняем владельца и делаем исполняемым
RUN chown origo:origo /app/origo && \
    chmod +x /app/origo

# Открываем порт для HTTP сервера
EXPOSE 8080

# Переключаемся на непривилегированного пользователя
USER origo

# Точка входа
ENTRYPOINT ["./origo"]