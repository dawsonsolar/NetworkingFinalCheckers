// =============================================================================
// bridge.js  —  WebSocket ↔ TCP bridge
// =============================================================================
// Serves index.html over HTTP and bridges each browser WebSocket connection
// to its own TCP connection to the C++ game server.
// Each browser tab = one TCP connection to the game server.
// Messages pass through unchanged — the protocol is identical.
// =============================================================================

const http    = require('http');
const fs      = require('fs');
const path    = require('path');
const net     = require('net');
const WebSocket = require('ws');

const HTTP_PORT        = process.env.PORT       || 8080;
const GAME_SERVER_HOST = process.env.GAME_HOST  || '127.0.0.1';
const GAME_SERVER_PORT = process.env.GAME_PORT  || 54000;

// ── HTTP server (serves index.html) ───────────────────────────────────────
const httpServer = http.createServer((req, res) => {
    const file = path.join(__dirname, 'index.html');
    fs.readFile(file, (err, data) => {
        if (err) {
            res.writeHead(404);
            res.end('index.html not found');
            return;
        }
        res.writeHead(200, { 'Content-Type': 'text/html' });
        res.end(data);
    });
});

// ── WebSocket server ───────────────────────────────────────────────────────
const wss = new WebSocket.Server({ server: httpServer });

wss.on('connection', (ws, req) => {
    const clientIp = req.socket.remoteAddress;
    console.log(`[Bridge] Browser connected from ${clientIp}`);

    // Open a dedicated TCP connection to the C++ server for this browser tab
    const tcp = new net.Socket();
    let tcpBuffer = '';
    let tcpConnected = false;

    tcp.connect(GAME_SERVER_PORT, GAME_SERVER_HOST, () => {
        tcpConnected = true;
        console.log(`[Bridge] TCP connection opened to game server for ${clientIp}`);
    });

    // ── TCP → WebSocket ────────────────────────────────────────────────────
    // The C++ server sends newline-terminated messages.
    // Buffer partial reads and forward complete lines to the browser.
    tcp.on('data', (data) => {
        tcpBuffer += data.toString();
        const lines = tcpBuffer.split('\n');
        tcpBuffer = lines.pop(); // Keep any incomplete trailing line
        for (const line of lines) {
            const trimmed = line.trim();
            if (trimmed && ws.readyState === WebSocket.OPEN) {
                ws.send(trimmed);
            }
        }
    });

    tcp.on('close', () => {
        console.log(`[Bridge] TCP closed for ${clientIp}`);
        if (ws.readyState === WebSocket.OPEN) ws.close();
    });

    tcp.on('error', (err) => {
        console.error(`[Bridge] TCP error for ${clientIp}:`, err.message);
        if (ws.readyState === WebSocket.OPEN) {
            ws.send('ERR|Could not connect to game server');
            ws.close();
        }
    });

    // ── WebSocket → TCP ────────────────────────────────────────────────────
    // Browser sends pipe-delimited messages without \n; add it for the server.
    ws.on('message', (msg) => {
        const str = msg.toString().trim();
        if (tcpConnected) {
            tcp.write(str + '\n');
        }
    });

    ws.on('close', () => {
        console.log(`[Bridge] Browser disconnected: ${clientIp}`);
        tcp.destroy();
    });

    ws.on('error', (err) => {
        console.error(`[Bridge] WebSocket error:`, err.message);
        tcp.destroy();
    });
});

// ── Start ──────────────────────────────────────────────────────────────────
httpServer.listen(HTTP_PORT, () => {
    console.log('=== Networked Checkers Web Bridge ===');
    console.log(`Open in browser: http://localhost:${HTTP_PORT}`);
    console.log(`Bridging to C++ server at ${GAME_SERVER_HOST}:${GAME_SERVER_PORT}`);
});
