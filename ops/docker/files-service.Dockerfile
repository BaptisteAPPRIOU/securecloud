# Build stage
FROM gcc:15 AS build

RUN apt-get update && apt-get install -y \
    cmake \
    git \
    libboost-system-dev \
    libssl-dev \
    nlohmann-json3-dev \
    libfmt-dev \
    libspdlog-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src

COPY services/files-service ./services/files-service
COPY libs ./libs

WORKDIR /src/services/files-service
RUN cmake -B build -DCMAKE_BUILD_TYPE=Release
RUN cmake --build build -j$(nproc)

# Runtime stage
FROM ubuntu:24.04

RUN apt-get update && apt-get install -y \
    libssl3 \
    libboost-system1.83.0 \
    ca-certificates \
    curl \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

COPY --from=build /src/services/files-service/build/files-service /app/
COPY services/files-service/config /app/config

RUN mkdir -p /app/logs /data/files

EXPOSE 8003

ENTRYPOINT ["/app/files-service"]
