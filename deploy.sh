# ============================================
# КОНФИГУРАЦИЯ - ДЛЯ ВАШЕГО VPS
# ============================================
VPS_HOST="194.48.143.99"      # Ваш VPS
VPS_USER="root"                 # Пользователь на VPS
VPS_PORT="22"                   # SSH порт (стандартный)
PROJECT_NAME="origo"            # Имя проекта
REMOTE_PATH="/opt/origo"        # Путь на VPS
IMAGE_NAME="origo-app"          # Имя Docker образа
CONTAINER_NAME="origo"          # Имя контейнера

# Читаем порт из конфига
CONFIG_FILE="origo.conf"
if [ -f "$CONFIG_FILE" ]; then
    # Ищем строку с "port" и извлекаем число
    PORT=$(grep -o '"port"[[:space:]]*:[[:space:]]*[0-9]*' "$CONFIG_FILE" | grep -o '[0-9]*$')
    if [ -z "$PORT" ]; then
        echo "⚠️  Не удалось прочитать порт из конфига, используем 8080"
        PORT=8080
    else
        echo "📡 Найден порт в конфиге: $PORT"
    fi
else
    echo "⚠️  Файл конфигурации не найден, используем порт 8080"
    PORT=8080
fi

# ============================================
# ЦВЕТА ДЛЯ ВЫВОДА
# ============================================
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# ============================================
# ФУНКЦИИ
# ============================================
print_status() {
    echo -e "${GREEN}[✓]${NC} $1"
}

print_error() {
    echo -e "${RED}[✗]${NC} $1"
}

print_info() {
    echo -e "${YELLOW}[→]${NC} $1"
}

print_header() {
    echo ""
    echo -e "${BLUE}========================================${NC}"
    echo -e "${BLUE}$1${NC}"
    echo -e "${BLUE}========================================${NC}"
}

# ============================================
# НАЧАЛО ДЕПЛОЯ
# ============================================
print_header "НАЧАЛО ДЕПЛОЯ ПРОЕКТА ${PROJECT_NAME}"

# Проверка Docker локально
print_info "Проверка Docker локально..."
if ! command -v docker &> /dev/null; then
    print_error "Docker не установлен локально"
    exit 1
fi
print_status "Docker найден"

# ============================================
# 1. СБОРКА DOCKER ОБРАЗА
# ============================================
print_header "1. СБОРКА DOCKER ОБРАЗА"
print_info "Сборка образа ${IMAGE_NAME}:latest..."

docker build -t ${IMAGE_NAME}:latest . 2>&1 | while read line; do
    echo "    $line"
done

if [ ${PIPESTATUS[0]} -ne 0 ]; then
    print_error "Ошибка сборки Docker образа"
    exit 1
fi
print_status "Образ собран: ${IMAGE_NAME}:latest"

# Размер образа
IMAGE_SIZE=$(docker images ${IMAGE_NAME}:latest --format "{{.Size}}")
print_info "Размер образа: ${IMAGE_SIZE}"

# ============================================
# 2. СОХРАНЕНИЕ ОБРАЗА В ФАЙЛ
# ============================================
print_header "2. ПОДГОТОВКА К ПЕРЕДАЧЕ"
print_info "Сохранение образа в архив..."

# Удаляем старый архив если есть
rm -f ${PROJECT_NAME}.tar.gz

docker save ${IMAGE_NAME}:latest | gzip > ${PROJECT_NAME}.tar.gz

if [ $? -ne 0 ]; then
    print_error "Ошибка сохранения образа"
    exit 1
fi

# Размер архива
ARCHIVE_SIZE=$(ls -lh ${PROJECT_NAME}.tar.gz | awk '{print $5}')
print_status "Образ сохранён: ${PROJECT_NAME}.tar.gz (${ARCHIVE_SIZE})"

# ============================================
# 3. КОПИРОВАНИЕ НА VPS
# ============================================
print_header "3. КОПИРОВАНИЕ НА VPS"
print_info "VPS: ${VPS_USER}@${VPS_HOST}:${VPS_PORT}"
print_info "Копирование файла..."

scp -P ${VPS_PORT} ${PROJECT_NAME}.tar.gz ${VPS_USER}@${VPS_HOST}:~/

if [ $? -ne 0 ]; then
    print_error "Ошибка копирования на VPS"
    rm -f ${PROJECT_NAME}.tar.gz
    exit 1
fi
print_status "Файл скопирован на VPS"

# ============================================
# 4. РАЗВЕРТЫВАНИЕ НА VPS
# ============================================
print_header "4. РАЗВЕРТЫВАНИЕ НА VPS"
print_info "Выполнение команд на удалённом сервере..."

ssh -p ${VPS_PORT} ${VPS_USER}@${VPS_HOST} << ENDSSH
    set -e  # Остановка при любой ошибке
    
    echo "    → Проверка Docker..."
    if ! command -v docker &> /dev/null; then
        echo "    ❌ Docker не установлен!"
        echo "    → Установка Docker..."
        apt-get update && apt-get install -y docker.io
        systemctl start docker
        systemctl enable docker
        echo "    ✓ Docker установлен"
    else
        echo "    ✓ Docker уже установлен"
    fi
    
    echo "    → Создание директории..."
    mkdir -p ${REMOTE_PATH}
    
    echo "    → Загрузка Docker образа..."
    docker load < ~/${PROJECT_NAME}.tar.gz
    
    echo "    → Остановка старого контейнера..."
    docker stop ${CONTAINER_NAME} 2>/dev/null || true
    docker rm ${CONTAINER_NAME} 2>/dev/null || true
    
    echo "    → Запуск нового контейнера..."
    docker run -d \
        --name ${CONTAINER_NAME} \
        --restart unless-stopped \
        -p ${PORT}:${PORT} \
        ${IMAGE_NAME}:latest
    
    echo "    → Очистка старых образов..."
    docker image prune -f
    
    echo "    → Удаление временных файлов..."
    rm -f ~/${PROJECT_NAME}.tar.gz
    
    echo "    → Проверка статуса контейнера..."
    docker ps --filter name=${CONTAINER_NAME}
    
    echo "    → Последние логи..."
    docker logs --tail 10 ${CONTAINER_NAME}
ENDSSH

if [ $? -ne 0 ]; then
    print_error "Ошибка деплоя на VPS"
    rm -f ${PROJECT_NAME}.tar.gz
    exit 1
fi

# ============================================
# 5. ОЧИСТКА ЛОКАЛЬНЫХ ФАЙЛОВ
# ============================================
print_header "5. ЗАВЕРШЕНИЕ"
rm -f ${PROJECT_NAME}.tar.gz
print_status "Локальная очистка завершена"

# ============================================
# 6. ИТОГИ
# ============================================
echo ""
print_header "ДЕПЛОЙ УСПЕШНО ЗАВЕРШЁН"
echo -e "${GREEN}✓${NC} Проект: ${PROJECT_NAME}"
echo -e "${GREEN}✓${NC} VPS: ${VPS_HOST}"
echo -e "${GREEN}✓${NC} Контейнер: ${CONTAINER_NAME}"
echo ""

print_info "Полезные команды для VPS:"
echo "  docker ps                    - список контейнеров"
echo "  docker logs ${CONTAINER_NAME} - логи приложения"
echo "  docker restart ${CONTAINER_NAME} - перезапуск"
echo "  docker stop ${CONTAINER_NAME}   - остановка"
echo ""

# Спросить о логах
read -p "Показать последние логи с VPS? (y/n): " -n 1 -r
echo
if [[ $REPLY =~ ^[Yy]$ ]]; then
    print_header "ЛОГИ ПРИЛОЖЕНИЯ"
    ssh -p ${VPS_PORT} ${VPS_USER}@${VPS_HOST} "docker logs --tail 30 ${CONTAINER_NAME}"
fi

print_header "ГОТОВО!"