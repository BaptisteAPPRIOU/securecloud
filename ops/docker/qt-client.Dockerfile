# ---------- build ----------
FROM ubuntu:24.04 AS build

RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    ninja-build \
    qt6-base-dev \
    qt6-base-dev-tools \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY client/qt-app ./client/qt-app

WORKDIR /src/client/qt-app
RUN cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=OFF
RUN cmake --build build -j"$(nproc)" --target MSF_Login

# ---------- runtime ----------
FROM ubuntu:24.04

RUN apt-get update && apt-get install -y \
    ca-certificates \
    fonts-dejavu-core \
    libgl1 \
    qt6-base-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY --from=build /src/client/qt-app/build/MSF_Login /app/MSF_Login

RUN mkdir -p /app/logs

ENV SECURECLOUD_API_BASE=https://gateway:8443
ENV SECURECLOUD_CLIENT_LOG=/app/logs/qt-client.log
ENV SECURECLOUD_DEV_ALLOW_SELF_SIGNED=false
ENV SECURECLOUD_TLS_CA_FILE=/app/certs/dev-cert.pem
ENV QT_QPA_PLATFORM=offscreen

ENTRYPOINT ["/app/MSF_Login"]
