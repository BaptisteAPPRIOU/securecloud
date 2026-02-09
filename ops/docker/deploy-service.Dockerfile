# Build stage
FROM gcc:15 AS build

RUN apt-get update && apt-get install -y \
    cmake \
    git \
    libboost-all-dev \
    libssl-dev \
    nlohmann-json3-dev \
    libfmt-dev \
    libspdlog-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src

COPY services/deploy-serrvice ./services/deploy-serrvice
COPY libs ./libs

WORKDIR /src/services/deploy-serrvice
RUN cmake -B build -DCMAKE_BUILD_TYPE=Release
RUN cmake --build build -j$(nproc)

# Runtime stage
FROM ubuntu:24.04

RUN apt-get update && apt-get install -y \
    libssl3 \
    libboost-system1.83.0 \
    ca-certificates \
    curl \
    docker.io \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

COPY --from=build /src/services/deploy-serrvice/build/deploy-service /app/
COPY services/deploy-serrvice/config /app/config

RUN mkdir -p /app/logs

EXPOSE 8006

ENTRYPOINT ["/app/deploy-service"]
