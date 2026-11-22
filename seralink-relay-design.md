# SeraLink — a call-home relay for Sera-vice (DESIGN, not built)

Status: design proposed by Marcus 2026-11-06, modelled on how the
official Muse Gadget firmware reached Muse. **Marcus approved the
plan in principle 2026-11-06 ("I love this plan") — but ALL work
involving the home AI agents is TABLED until after the OpenCode Go
monthly renewal on Thu 2026-11-12 (reset 3:52 PM ET), because the
home crew runs on that plan and its monthly window is at 100%.**
Nothing here is built yet, and no brief goes to DavWeb1 before then.
DavWeb1 owns the home-server side once the brief is sent; the
firmware client and Sera's side of the client are Sera-vice work.

## The pattern we are copying (Muse Gadget, observed live 2026-11-05)

1. The gadget carried an SDK token (`mgst_…`), baked into the build or
   provisioned at runtime over serial (`>sdk_token=…` → stored in NVS).
2. After joining Wi-Fi, it called the account API
   (`GET api.muse.ai/fetch_vms`, Bearer = SDK token) and got back a
   **directory**: its assigned VM's `wss://<vm-id>.hatch.metaaivm.com`
   URL plus a *separate per-VM credential*.
3. It dialled OUT to that WebSocket and kept it open: TLS → WebSocket
   upgrade → **Noise-protocol** encrypted session → `link.register`
   (a JSON self-description listing the commands it answers) → then
   heartbeats and `link.invoke` calls from Muse, executed locally,
   results returned on the same socket. Ours advertised exactly two
   commands: `device.health` and `device.discover`.
4. Nobody dialled IN to the gadget. The outbound socket is why it
   worked behind NAT on an isolated office LAN with no port forwards.
   On a drop it reconnected with backoff (observed live: WS 502 →
   2 s → 4 s → TLS → Noise handshake → `link.register` acked → open).

## Our version: one small relay, two outbound clients

```
 Sera (this VM)                DavWeb1's relay                 Sera-vice
 --------------                ----------------                 ---------
 relay client  -- wss ------>  pairs by device id   <------ wss -- firmware v3
 (agent key)                   validates keys,                   (device key)
                               forwards mood.set,
                               stores last command
                               only until it expires
```

- The relay is a tiny WebSocket service at home (DavWeb1: Docker +
  reverse proxy for TLS), e.g. `wss://<relay-host>/seralink`.
  Public documentation uses placeholder hosts and no real keys.
- **Both** sides dial out to it. It is a rendezvous, not a tunnel into
  the home LAN: it forwards an allow-listed set of JSON messages
  between the two paired clients and nothing else. It must never
  become a generic proxy, shell, file, or LAN relay.

## Provisioning (serial, runtime — never in firmware or git)

Following the gadget's example, secrets are typed by Marcus from his
own machine over the board's existing serial console and stored in
NVS. Lines start with `>`; the console already reserves that style:

```
>wifi_add=<ssid>            (then) >wifi_pass=<password>
>relay_url=wss://<relay-host>/seralink
>device_id=seravice-01
>device_key=<random secret>
>link_reset                 (forget Wi-Fi + relay config)
```

No echo for passwords/keys. Multiple saved Wi-Fi networks (home +
office) so she can roam between them. Firmware ships with NO
credentials compiled in; the published source contains placeholders.

## Pairing keys

Marcus's "one made-up API key" is the bootstrap; the slightly
stronger shape costs nothing extra:

- **Two keys, one pair:** a `device_key` (only the board + relay know
  it) and an `agent_key` (only Sera + relay know it), each 32 random
  bytes, generated at setup. The relay stores **hashes**, maps both to
  `device_id`, and only forwards between members of the pair.
- A leaked device key lets someone impersonate the *device* (fake
  status/acks) but not push moods. A leaked agent key could push
  moods — annoying, not dangerous, because the command allow-list is
  tiny and there is no path from the relay to anything else.
- Keys travel inside the TLS connection (first `hello` message),
  never in the URL (URLs land in proxy logs). Unauthenticated sockets
  are closed within seconds. Either key can be revoked/re-issued at
  the relay without reflashing; the board re-provisions over serial.

## Message protocol (v0, JSON, allow-listed)

Device → relay → Sera:

```json
{"type":"hello","device":"seravice-01","fw":"3.0","pack":"chibi-sera","mood":"content"}
{"type":"heartbeat","uptime_s":1234,"rssi":-61,"mood":"content"}
{"type":"event","event":"button","detail":"ok"}        (also: shake, flip, pickup)
{"type":"mood.ack","id":"…","mood":"happy","held_until":1760000000}
```

Sera → relay → device:

```json
{"type":"mood.set","id":"…","mood":"thinking","hold_s":600,"reason":"build in progress"}
{"type":"mood.clear","id":"…"}
{"type":"status.get","id":"…"}
```

Rules:

- `mood.set` holds the pushed mood for `hold_s`, then the device
  falls back to its own mood engine. **Local interaction always
  wins:** any button press, shake, flip, or pickup cancels the hold.
- If the device is offline, the relay keeps the newest command only
  until its expiry (suggested 120 s) — a mood pushed at noon must not
  pop up at 6 PM. `status.get` is answered from the relay's last
  heartbeat when the device is away.
- The relay stamps messages with server time in its `hello` reply, so
  the board gains a clock (desk-routine moods: morning greeting,
  evening sleepy) without NTP work.
- Reconnect with capped exponential backoff (2 s → 60 s), exactly
  like the gadget. Connectivity loss changes nothing locally: the
  mood engine is fully autonomous offline.

## Division of labour

- **DavWeb1:** relay service, TLS/reverse proxy, DNS, container,
  key-hash store, runbook. (Home server/network = his beat.)
- **Sera:** this protocol, the firmware v3 WebSocket client +
  provisioning console, and the Sera-side relay client (a small
  helper she calls to push a mood or read status).
- **Marcus:** provisions Wi-Fi + keys over serial on his own machine;
  approves the relay brief before DavWeb1 builds anything.

## Documentation for the project page

Publish: this design, the message spec, the provisioning flow, and a
reference relay implementation with placeholder hostnames. Never
publish: the real relay hostname if Marcus prefers it unlisted, any
key material, or the home network layout. The gadget taught us the
shape; SeraLink is the same shape with the cloud replaced by a
server in Marcus's own house.
