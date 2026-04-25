# Networked Checkers

A multiplayer checkers game using an authoritative C++ game server and a browser-based client. Two players connect through a web page and the server handles all game logic and move validation.

**Live demo:** https://networkingfinalcheckers.itch.io

---

## Architecture

```
Browser (itch.io)
    | WebSocket (WSS)
Node.js bridge  <-- same Docker container
    | TCP :54000
C++ game server
```

The C++ server handles all game state and validates every move. The Node.js bridge translates between WebSocket (browser) and raw TCP (server). The browser client is a single HTML file.

---

## Running Locally

### Requirements
- Linux, macOS, or Windows with WSL
- g++ with C++17, CMake, make
- Node.js 20+

### Build and run

```bash
# Build
cd checkers-net
mkdir build && cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build

# Terminal 1 - game server
./build/checkers-server

# Terminal 2 - web bridge
cd web && npm install && node bridge.js

# Open two browser tabs at http://localhost:8080
```

### Terminal client (optional)

```bash
./build/checkers-client 127.0.0.1 54000
# Input: fromRow fromCol toRow toCol (e.g. "5 0 4 1")
```

---

## Deployment (Docker + Render)

The `Dockerfile` builds the C++ server and bundles it with the Node bridge into a single container.

```bash
# Test locally
docker build -t checkers-net .
docker run -p 8080:8080 checkers-net
```

For cloud deployment, connect this repo to [Render.com](https://render.com), set root directory to `checkers-net`, and deploy. After deploying, update `web/index.html`:

```javascript
const WS_SERVER = 'wss://your-app-name.onrender.com';
```

---

## Protocol

Newline-terminated, pipe-delimited messages over TCP:

| Direction | Message | Meaning |
|-----------|---------|---------|
| C → S | `CONNECT\|name` | Join as a new player |
| C → S | `RECONNECT\|session_id` | Rejoin after disconnect |
| C → S | `MOVE\|fr\|fc\|tr\|tc` | Submit a move |
| S → C | `WELCOME\|sid\|pnum\|room` | Session assigned |
| S → C | `STATE\|board64\|turn\|p1\|p2` | Board update |
| S → C | `YOUR_TURN` / `WAIT_TURN` | Turn notification |
| S → C | `GAME_OVER\|winner\|reason` | Game ended |
| S → C | `OPPONENT_DC` | Opponent disconnected |

`board64` is a 64-character string encoding the board: `0`=empty, `1`=P1, `2`=P2, `3`=P1 King, `4`=P2 King.

---

## File Structure

```
checkers-net/
├── shared/
│   ├── Protocol.h           # Message format constants and helpers
│   ├── CheckersLogic.h/.cpp # Game rules (mandatory capture, multi-jump, kings)
├── server/
│   ├── Session.h            # Per-client state (persists through disconnects)
│   ├── GameRoom.h/.cpp      # One game instance, 2 players + spectators
│   ├── GameServer.h/.cpp    # TCP listener, session manager, thread per client
│   └── main.cpp
├── client/
│   └── main.cpp             # Terminal client for local testing
├── web/
│   ├── bridge.js            # WebSocket <-> TCP bridge
│   ├── index.html           # Browser client with protocol log
│   └── package.json
├── Dockerfile
├── start.sh
├── CMakeLists.txt
└── Makefile
```
