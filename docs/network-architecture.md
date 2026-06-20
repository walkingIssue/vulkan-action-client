# Network Architecture

This slice is a UDP relay, not an authoritative server.

Clients simulate and render optimistically. Each client owns one actor, sends compact snapshots of that actor every fixed tick, and applies remote snapshots as presentation state for the other actor. The server only learns client endpoints and fans validated packets out to the other clients. There are no acks, no resends, no rollback, and no server-side combat decisions yet.

## Current Flow

```text
client 1 fixed tick
  -> local actor transform
  -> bitpacked ActorSnapshot
  -> UDP relay server
  -> client 2 remote actor transform

client 2 fixed tick
  -> local actor transform
  -> bitpacked ActorSnapshot
  -> UDP relay server
  -> client 1 remote actor transform
```

Client id `1` owns `player_preview`. Client id `2` owns `sparring_partner`. In network mode, the old local arrow-key sparring control is disabled so the remote actor is driven by received packets.

## Packet

The first packet type is `ActorSnapshot`.

```text
magic: 32 bits
version: 4 bits
kind: 4 bits
client id: 8 bits
tick: 16 bits
flags: 8 bits
position x/y/z: 22 bits each, signed, centimeter scale
yaw: 16 bits, normalized 0..360 degrees
```

The codec writes fields manually through a bit writer. It does not send compiler-packed structs.

## Why This Shape

This keeps the transport separate from combat. The network library owns UDP sockets and packet encoding. The viewer owns temporary client wiring and actor mapping. The combat simulation receives typed movement tuning, input intent, and transforms; it still has no socket, JSON, or renderer dependency.

Later, the server can become authoritative by replacing relay fanout with command intake, fixed-tick simulation, state deltas, prediction correction, and eventually rollback or reconciliation. This first step is deliberately only enough to put two optimistic clients on the wire.

## Running

```powershell
.\build\msvc-debug\udp_state_server.exe --bind 127.0.0.1:40000 --dump-packets 128
.\build\msvc-debug\vulkan_scene_viewer.exe --net-client-id 1 --net-server 127.0.0.1:40000
.\build\msvc-debug\vulkan_scene_viewer.exe --net-client-id 2 --net-server 127.0.0.1:40000
```

For finite smoke tests, add `--frames 180` to each viewer and `--max-packets 128` to the server.
