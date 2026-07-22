#pragma once
// ============================================================================
// packet.h — Synthetic packet builder. PROVIDED AS TOOLING so you can test
// the VM immediately without touching pcap files.
//
// Builds a minimal-but-correctly-laid-out Ethernet + IPv4 + TCP/UDP frame
// whose header fields land at exactly the offsets in bytecode.h's
// field_offsets. (Checksums are left zero — the filter never reads them.)
//
// Stage 7 replaces/augments this with a real pcap file parser you write:
// the pcap format is a 24-byte global header then per-packet
// [16-byte record header][raw frame bytes]. See README roadmap.
// ============================================================================

#include <cstdint>
#include <cstring>
#include <vector>

namespace pfc {

struct PacketSpec {
    uint32_t src_ip   = 0x0A000001;   // 10.0.0.1
    uint32_t dst_ip   = 0x0A000002;   // 10.0.0.2
    uint16_t src_port = 12345;
    uint16_t dst_port = 80;
    uint8_t  proto    = 6;            // 6 = TCP, 17 = UDP
    size_t   payload_len = 16;
};

inline std::vector<uint8_t> build_packet(const PacketSpec& s) {
    // 14 (eth) + 20 (ipv4) + 20 (tcp-sized L4 header) + payload
    std::vector<uint8_t> p(14 + 20 + 20 + s.payload_len, 0);

    auto put16 = [&](size_t off, uint16_t v) {
        p[off] = static_cast<uint8_t>(v >> 8);
        p[off + 1] = static_cast<uint8_t>(v & 0xFF);
    };
    auto put32 = [&](size_t off, uint32_t v) {
        p[off]     = static_cast<uint8_t>(v >> 24);
        p[off + 1] = static_cast<uint8_t>(v >> 16);
        p[off + 2] = static_cast<uint8_t>(v >> 8);
        p[off + 3] = static_cast<uint8_t>(v & 0xFF);
    };

    // Ethernet: MACs left zero, ethertype = IPv4
    put16(12, 0x0800);
    // IPv4: version=4, IHL=5 -> 0x45; total length; proto; addresses
    p[14] = 0x45;
    put16(16, static_cast<uint16_t>(p.size() - 14));
    p[23] = s.proto;
    put32(26, s.src_ip);
    put32(30, s.dst_ip);
    // L4 ports (same layout for TCP and UDP first 4 bytes)
    put16(34, s.src_port);
    put16(36, s.dst_port);
    return p;
}

// Handy for tests: pack "a.b.c.d" -> u32 at compile time-ish.
constexpr uint32_t ip(uint8_t a, uint8_t b, uint8_t c, uint8_t d) {
    return (uint32_t(a) << 24) | (uint32_t(b) << 16) | (uint32_t(c) << 8) | d;
}

} // namespace pfc
