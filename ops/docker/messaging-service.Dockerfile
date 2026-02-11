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

COPY services/messaging-service ./services/messaging-service
COPY libs ./libs

WORKDIR /src/services/messaging-service
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

COPY --from=build /src/services/messaging-service/build/messaging-service /app/
COPY services/messaging-service/config /app/config

RUN mkdir -p /app/logs

EXPOSE 8004 8005

ENTRYPOINT ["/app/messaging-service"]
