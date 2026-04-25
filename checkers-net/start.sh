#!/bin/bash
# Start the C++ game server in the background on its internal port
./checkers-server $GAME_PORT &
CPP_PID=$!

echo "[start.sh] C++ server started (pid $CPP_PID) on port $GAME_PORT"

# Give it a moment to bind
sleep 1

# Start the Node.js bridge (foreground, so Docker tracks it)
echo "[start.sh] Starting Node bridge on port $PORT -> game server :$GAME_PORT"
cd web && node bridge.js

# If bridge exits, kill the server too
kill $CPP_PID
