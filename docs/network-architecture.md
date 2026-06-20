# Network Architecture

This slice is a UDP relay, not an authoritative server.

Clients simulate and render optimistically. Each client owns one actor, sends compact snapshots of that actor every fixed tick, and applies remote snapshots as presentation state for peer actors. The server learns client endpoints through explicit connect packets, stores the latest accepted actor snapshot per client, and fans validated packets out to the other clients. There are no acks, no resends, no rollback, and no server-side combat decisions yet.
Clients now enter the relay through an explicit connect packet and leave through an explicit disconnect packet. Actor snapshots are accepted only from the endpoint that owns the snapshot client id.

## Current Flow

```text
client 1 fixed tick
  -> ConnectPacket once at client startup
  -> local actor transform
  -> bitpacked ActorSnapshot
  -> UDP relay server
  -> client 2 remote actor transform
  -> DisconnectPacket on shutdown

client 2 fixed tick
  -> ConnectPacket once at client startup
  -> local actor transform
  -> bitpacked ActorSnapshot
  -> UDP relay server
  -> client 1 remote actor transform
  -> DisconnectPacket on shutdown
```

The relay can track any number of connected client ids. In network mode, the viewer treats the bootstrap character instances as model templates. It creates one actor for the local client id and materializes remote actors only when server connect events or snapshots arrive. Disconnected peers are removed from the local presentation scene.

## Packet

Every packet starts with the same header:

```text
magic: 32 bits
version: 4 bits
kind: 4 bits
```

Packet kinds:

```text
1: ConnectPacket
2: DisconnectPacket
3: ActorSnapshot
4: ServerEventPacket
```

`ConnectPacket` and `DisconnectPacket`:

```text
client id: 8 bits
sequence: 16 bits
```

`ActorSnapshot`:

```text
client id: 8 bits
tick: 16 bits
flags: 8 bits
position x/y/z: 22 bits each, signed, centimeter scale
yaw: 16 bits, normalized 0..360 degrees
```

`ServerEventPacket`:

```text
event: 8 bits
client id: 8 bits
sequence: 16 bits
```

Server events currently announce peer connect and disconnect.

The codec writes fields manually through a bit writer. It does not send compiler-packed structs.

## Server World Cache

The relay keeps the latest accepted `ActorSnapshot` for each connected client. When a new client connects, the server sends:

```text
ServerEventPacket(clientConnected existing-peer)
ActorSnapshot(latest existing-peer state, if available)
```

to the new client for each existing peer. Existing peers receive `clientConnected` for the new client and then continue receiving normal snapshot fanout as soon as the new client publishes movement. Disconnect removes both the session and the cached snapshot for that client.

## Why This Shape

This keeps the transport separate from combat. The network library owns UDP sockets and packet encoding. The viewer owns temporary client wiring and actor mapping. The combat simulation receives typed movement tuning, input intent, and transforms; it still has no socket, JSON, or renderer dependency.

Later, the server can become authoritative by replacing relay fanout with command intake, fixed-tick simulation, state deltas, prediction correction, and eventually rollback or reconciliation. This first step is deliberately only enough to put two optimistic clients on the wire.

The relay session rules live in a pure `SnapshotRelay` core so unit tests can assert ingress behavior without real sockets. The UDP server wraps that core with endpoint serialization and datagram send/receive.

## Running

```powershell
.\tools\build.ps1
.\build\msvc-debug\udp_state_server.exe --bind 127.0.0.1:40000 --dump-packets 128
.\build\msvc-debug\vulkan_scene_viewer.exe --net-client-id 1 --net-server 127.0.0.1:40000
.\build\msvc-debug\vulkan_scene_viewer.exe --net-client-id 2 --net-server 127.0.0.1:40000
```

For automated regression coverage:

```powershell
.\tools\test.ps1
```
