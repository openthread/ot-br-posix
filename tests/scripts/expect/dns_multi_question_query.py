#!/usr/bin/env python3
#
#  Copyright (c) 2026, The OpenThread Authors.
#  All rights reserved.
#
#  Redistribution and use in source and binary forms, with or without
#  modification, are permitted provided that the following conditions are met:
#  1. Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer.
#  2. Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in the
#     documentation and/or other materials provided with the distribution.
#  3. Neither the name of the copyright holder nor the
#     names of its contributors may be used to endorse or promote products
#     derived from this software without specific prior written permission.
#
#  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
#  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
#  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
#  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
#  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
#  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
#  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
#  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
#  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
#  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
#  POSSIBILITY OF SUCH DAMAGE.
#

import socket
import sys
import struct

def build_query(domain):
    # Header: ID=0x1234, Flags=0x0100 (RD=1), QDCOUNT=2, ANCOUNT=0, NSCOUNT=0, ARCOUNT=0
    header = struct.pack('!HHHHHH', 0x1234, 0x0100, 2, 0, 0, 0)
    
    # Question 1: domain, QTYPE=SRV (33), QCLASS=IN (1)
    parts = domain.strip('.').split('.')
    q1_name = b''
    for part in parts:
        q1_name += bytes([len(part)]) + part.encode('ascii')
    q1_name += b'\x00'
    q1 = q1_name + struct.pack('!HH', 33, 1)
    
    # Question 2: domain (compressed pointing to offset 12), QTYPE=TXT (16), QCLASS=IN (1)
    q2 = struct.pack('!H', 0xc00c) + struct.pack('!HH', 16, 1)
    
    return header + q1 + q2

def parse_response(data):
    if len(data) < 12:
        return 'INVALID_LENGTH', 0
    id_val, flags, qdcount, ancount, nscount, arcount = struct.unpack('!HHHHHH', data[:12])
    rcode = flags & 0x000f
    return rcode, ancount

def main():
    if len(sys.argv) < 3:
        print("Usage: dns_multi_question_query.py <dns_server_ip> <domain>")
        sys.exit(1)
        
    server_ip = sys.argv[1]
    domain = sys.argv[2]
    
    query = build_query(domain)
    
    # Send via UDP to port 53
    s = socket.socket(socket.AF_INET6, socket.SOCK_DGRAM)
    s.settimeout(5)
    try:
        s.sendto(query, (server_ip, 53))
        resp, addr = s.recvfrom(512)
    except Exception as e:
        print(f"ERROR: {e}")
        sys.exit(1)
        
    rcode, ancount = parse_response(resp)
    print(f"RCODE:{rcode}")
    print(f"ANCOUNT:{ancount}")
    
    # Pass if RCODE is 1 (FormErr) or RCODE is 0 (NoError)
    if rcode == 1:
        print("SUCCESS")
        sys.exit(0)
    elif rcode == 0:
        if ancount == 2:
            print("SUCCESS")
            sys.exit(0)
        else:
            print(f"FAILURE (Expected 2 answers, got {ancount})")
            sys.exit(1)
    else:
        print(f"FAILURE (RCODE={rcode})")
        sys.exit(1)

if __name__ == '__main__':
    main()
