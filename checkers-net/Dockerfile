# ── Stage 1: Build the C++ game server ────────────────────────────────────
FROM ubuntu:24.04 AS builder

RUN apt-get update && apt-get install -y \
    g++ cmake make \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY . .

RUN cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
 && cmake --build build --target checkers-server -- -j$(nproc)

# ── Stage 2: Runtime (server binary + Node.js bridge) ─────────────────────
FROM ubuntu:24.04 AS runtime

# Node.js 20 LTS
RUN apt-get update && apt-get install -y curl \
 && curl -fsSL https://deb.nodesource.com/setup_20.x | bash - \
 && apt-get install -y nodejs \
 && rm -rf /var/lib/apt/lists/*

WORKDIR /app

# Copy compiled server binary
COPY --from=builder /app/build/checkers-server ./checkers-server

# Copy Node bridge
COPY web/package.json web/package-lock.json ./web/
RUN cd web && npm ci --omit=dev

COPY web/bridge.js  ./web/bridge.js
COPY web/index.html ./web/index.html

# Startup script
COPY start.sh ./start.sh
RUN chmod +x ./start.sh

# Render assigns $PORT for external traffic; bridge listens on it
ENV PORT=8080
ENV GAME_PORT=54000

EXPOSE 8080

CMD ["./start.sh"]
