# ---------- build ----------
FROM ubuntu:24.04 AS build
RUN apt-get update && apt-get install -y build-essential cmake git ca-certificates && rm -rf /var/lib/apt/lists/*
WORKDIR /src
COPY . .
RUN ./tools/vcpkg/bootstrap-vcpkg.sh
RUN cmake -S gateway -B build -DCMAKE_BUILD_TYPE=Release \
-DCMAKE_TOOLCHAIN_FILE=/src/tools/vcpkg/scripts/buildsystems/vcpkg.cmake
RUN cmake --build build -j


# ---------- runtime ----------
FROM ubuntu:24.04
RUN apt-get update && apt-get install -y ca-certificates && rm -rf /var/lib/apt/lists/*
WORKDIR /app
COPY --from=build /src/build/gateway /app/gateway
COPY gateway/config /app/config
ENTRYPOINT ["/app/gateway"]