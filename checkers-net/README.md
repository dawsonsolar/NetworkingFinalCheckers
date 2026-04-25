# Networked Checkers | LEFT OVER FROM FIRST ITERATION! THIS IS ENTIRELY INACCURATE NOW!

A turn-based multiplayer checkers game over TCP, demonstrating:
- **Authoritative server** — all move validation happens server-side
- **Session persistence** — clients get a session token and can reconnect mid-game
- **Spectator mode** — additional clients can watch any active game
- **Mandatory captures & multi-jump** — full standard English checkers rules

---

## Requirements

- Linux or macOS (uses POSIX sockets)
- g++ with C++17 support (`g++ --version`)
- `make`

> **Windows:** Use WSL (Windows Subsystem for Linux) or MSYS2/MinGW.

---

## Build

```bash
git clone <your-repo-url>
cd checkers-net
make
```

Produces two binaries: `checkers-server` and `checkers-client`.

---

## Run

### 1. Start the server

```bash
./checkers-server            # defaults to port 54000
./checkers-server 8080       # custom port
```

### 2. Connect two clients (in separate terminals)

```bash
./checkers-client            # connects to localhost:54000
./checkers-client 192.168.1.5 54000   # remote host + port
```

Each client is prompted for a name. The first two players to join a room are automatically matched. Further clients can join as **spectators**.

### 3. Playing

When it's your turn you'll see:

```
▶  YOUR TURN!
Enter move (fromRow fromCol toRow toCol), or 'q' to quit:
> 5 0 4 1
```

The board uses 0-indexed rows and columns (row 0 = top, row 7 = bottom).

| Symbol | Meaning |
|--------|---------|
| `●`    | Player 1 piece |
| `○`    | Player 2 piece |
| `♛`    | King (either player) |

---

## Reconnection

If you disconnect, your **session ID** is printed on the waiting screen and again on disconnect. To rejoin:

```bash
./checkers-client
# Enter name: Alice
# Reconnect with session ID? (leave blank for new game): a1b2c3d4
```

The server holds your session open indefinitely; your opponent sees "Opponent disconnected" and waits.

---

## Testing multiplayer locally

```bash
# Terminal 1
./checkers-server

# Terminal 2
./checkers-client 127.0.0.1 54000

# Terminal 3
./checkers-client 127.0.0.1 54000

# Terminal 4 (spectator)
./checkers-client 127.0.0.1 54000
# Answer 'n' to name and then type SPECTATE when prompted
```

---

## Protocol

Messages are newline-terminated, pipe-delimited ASCII strings sent over TCP.

| Direction | Message | Description |
|-----------|---------|-------------|
| C→S | `CONNECT\|name` | Initial connection |
| C→S | `RECONNECT\|session_id` | Resume after disconnect |
| C→S | `MOVE\|fr\|fc\|tr\|tc` | Submit a move |
| C→S | `SPECTATE` | Join as observer |
| S→C | `WELCOME\|sid\|pnum\|room` | Assigned session |
| S→C | `START\|opponent` | Game beginning |
| S→C | `STATE\|board64\|turn\|p1\|p2` | Full board state |
| S→C | `YOUR_TURN` | Prompt for input |
| S→C | `GAME_OVER\|winner\|reason` | Game ended |
| S→C | `OPPONENT_DC` | Opponent disconnected |
| S→C | `REJOINED\|pnum\|board64\|...` | Reconnection confirmed |

The `board64` field is a 64-character string encoding all 8×8 cells left-to-right, top-to-bottom:
`0`=empty, `1`=P1, `2`=P2, `3`=P1 King, `4`=P2 King.

---

## Architecture

```
┌──────────────┐   TCP/54000   ┌─────────────────────────┐   TCP/54000   ┌──────────────┐
│   Client P1  │◄─────────────►│       GameServer        │◄─────────────►│   Client P2  │
│              │               │  ┌──────────────────┐   │               │              │
│  select() on │               │  │    GameRoom R1   │   │               │  select() on │
│  sock+stdin  │               │  │  ┌────────────┐  │   │               │  sock+stdin  │
└──────────────┘               │  │  │CheckersLogic│  │   │               └──────────────┘
                               │  │  └────────────┘  │   │
┌──────────────┐               │  │  sessions map    │   │
│  Spectator   │◄─────────────►│  │  mutex-protected │   │
│  (read-only) │               │  └──────────────────┘   │
└──────────────┘               └─────────────────────────┘

Thread model: one thread per client connection (std::thread, detached)
Room state:   protected by std::mutex inside GameRoom
Sessions:     stored by ID for reconnection (never deleted during a game)
```

---

## Project structure

```
checkers-net/
├── shared/
│   ├── Protocol.h          # Wire format constants & helpers
│   ├── CheckersLogic.h     # Game rules (header)
│   └── CheckersLogic.cpp   # Game rules (implementation)
├── server/
│   ├── Session.h           # Per-client state struct
│   ├── GameRoom.h/.cpp     # One active game (2 players + spectators)
│   ├── GameServer.h/.cpp   # TCP listener, session manager, thread spawner
│   └── main.cpp
├── client/
│   └── main.cpp            # Terminal UI with select()-based I/O
├── Makefile
└── README.md
```
