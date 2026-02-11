# ---------- build ----------
FROM ubuntu:24.04 AS build
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    git \
    libboost-system-dev \
    libssl-dev \
    nlohmann-json3-dev \
    libfmt-dev \
    libspdlog-dev \
    && rm -rf /var/lib/apt/lists/*
WORKDIR /src
COPY gateway ./gateway
COPY libs ./libs
WORKDIR /src/gateway
RUN cmake -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF
RUN cmake --build build -j$(nproc)

# ---------- runtime ----------
FROM ubuntu:24.04
RUN apt-get update && apt-get install -y \
    libssl3 \
    libboost-system1.83.0 \
    libfmt-dev \
    ca-certificates \
    curl \
    && rm -rf /var/lib/apt/lists/*
WORKDIR /app
COPY --from=build /src/gateway/build/gateway /app/gateway
COPY gateway/config /app/config
ENTRYPOINT ["/app/gateway"]
