# Build stage
FROM gcc:15 AS build

# Install dependencies
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

# Copy source
COPY services/audit-service ./services/audit-service
COPY libs ./libs

# Build
WORKDIR /src/services/audit-service
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

# Copy binary and config
COPY --from=build /src/services/audit-service/build/audit-service /app/
COPY services/audit-service/config /app/config

RUN mkdir -p /app/logs

EXPOSE 8002

ENTRYPOINT ["/app/audit-service"]
