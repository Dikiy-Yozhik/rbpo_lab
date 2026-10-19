FROM ubuntu:22.04

# Чтобы apt-get не зависал на интерактивных вопросах
ENV DEBIAN_FRONTEND=noninteractive

# Устанавливаем базовые инструменты для сборки
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    wget \
    && rm -rf /var/lib/apt/lists/*

# --- ЭТАП 1: Сборка уязвимой libjpeg 9c ---
WORKDIR /tmp
RUN wget https://www.ijg.org/files/jpegsrc.v9c.tar.gz && \
    tar -xzf jpegsrc.v9c.tar.gz && \
    cd jpeg-9c && \
    ./configure --prefix=/usr/local && \
    make && \
    make install && \
    ldconfig

# --- ЭТАП 2: Сборка нашего приложения ---
WORKDIR /app

# Копируем исходники
COPY main.cpp .
COPY CMakeLists.txt .

# Собираем
RUN mkdir build && cd build && cmake .. && make

# Создаем папку для вывода (на случай, если volume не проброшен)
RUN mkdir -p /app/output

# Точка входа
ENTRYPOINT ["/app/build/jpeg_parser"]