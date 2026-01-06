# Build stage
FROM gcc:13 AS build

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
COPY services/auth-service ./services/auth-service
COPY libs ./libs

# Build
WORKDIR /src/services/auth-service
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
COPY --from=build /src/services/auth-service/build/auth-service /app/
COPY services/auth-service/config /app/config

# Create logs directory
RUN mkdir -p /app/logs

EXPOSE 8001

ENTRYPOINT ["/app/auth-service"]
