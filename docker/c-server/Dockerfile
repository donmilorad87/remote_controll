FROM debian:bookworm-slim AS builder

RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    cmake \
    pkg-config \
    libmicrohttpd-dev \
    libpq-dev \
    libssl-dev \
    libjansson-dev \
    libcurl4-openssl-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY backend/ /app/

RUN mkdir -p build && cd build \
    && cmake .. -DCMAKE_BUILD_TYPE=Release \
    && make -j$(nproc)

FROM debian:bookworm-slim

RUN apt-get update && apt-get install -y --no-install-recommends \
    libmicrohttpd12 \
    libpq5 \
    libssl3 \
    libjansson4 \
    libcurl4 \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

COPY --from=builder /app/build/remotecontrol-server /app/
COPY --from=builder /app/migrations/ /app/migrations/

EXPOSE 7481/tcp
EXPOSE 7482/udp

CMD ["/app/remotecontrol-server"]
