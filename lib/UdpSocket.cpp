/*
   Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008, 2009 Her Majesty the
   Queen in Right of Canada (Communications Research Center Canada)

   Copyright (C) 2017
   Matthias P. Braendli, matthias.braendli@mpb.li

    http://www.opendigitalradio.org
   */
/*
   This file is part of ODR-DabMux.

   ODR-DabMux is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as
   published by the Free Software Foundation, either version 3 of the
   License, or (at your option) any later version.

   ODR-DabMux is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with ODR-DabMux.  If not, see <http://www.gnu.org/licenses/>.
   */

#include "UdpSocket.h"
#include "Utils.h"

#include <iostream>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>

using namespace std;

UdpSocket::UdpSocket() :
    listenSocket(INVALID_SOCKET)
{
    reinit(0, "");
}

UdpSocket::UdpSocket(int port) :
    listenSocket(INVALID_SOCKET)
{
    reinit(port, "");
}

UdpSocket::UdpSocket(int port, const std::string& name) :
    listenSocket(INVALID_SOCKET)
{
    reinit(port, name);
}


int UdpSocket::setBlocking(bool block)
{
    int res;
    if (block)
        res = fcntl(listenSocket, F_SETFL, 0);
    else
        res = fcntl(listenSocket, F_SETFL, O_NONBLOCK);
    if (res == SOCKET_ERROR) {
        setInetError("Can't change blocking state of socket");
        return -1;
    }
    return 0;
}

int UdpSocket::reinit(int port, const std::string& name)
{
    if (listenSocket != INVALID_SOCKET) {
        ::close(listenSocket);
    }

    if ((listenSocket = socket(PF_INET, SOCK_DGRAM, 0)) == INVALID_SOCKET) {
        setInetError("Can't create socket");
        return -1;
    }
    reuseopt_t reuse = 1;
    if (setsockopt(listenSocket, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse))
            == SOCKET_ERROR) {
        setInetError("Can't reuse address");
        return -1;
    }

    if (port) {
        address.setAddress(name);
        address.setPort(port);

        if (::bind(listenSocket, address.getAddress(), sizeof(sockaddr_in)) == SOCKET_ERROR) {
            setInetError("Can't bind socket");
            ::close(listenSocket);
            listenSocket = INVALID_SOCKET;
            return -1;
        }
    }
    return 0;
}

int UdpSocket::close()
{
    if (listenSocket != INVALID_SOCKET) {
        ::close(listenSocket);
    }

    listenSocket = INVALID_SOCKET;

    return 0;
}

UdpSocket::~UdpSocket()
{
    if (listenSocket != INVALID_SOCKET) {
        ::close(listenSocket);
    }
}

static inline bool wait_for_recv_ready(int sock_fd, const size_t timeout_ms)
{
    //setup timeval for timeout
    timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = timeout_ms*1000;

    //setup rset for timeout
    fd_set rset;
    FD_ZERO(&rset);
    FD_SET(sock_fd, &rset);

    return ::select(sock_fd+1, &rset, NULL, NULL, &tv) > 0;
}

int UdpSocket::receive(UdpPacket& packet)
{
    bool ready = wait_for_recv_ready(listenSocket, 2000);

    if (ready) {
        socklen_t addrSize;
        addrSize = sizeof(*packet.getAddress().getAddress());
        ssize_t ret = recvfrom(listenSocket,
                packet.getData(),
                packet.getSize(),
                0,
                packet.getAddress().getAddress(),
                &addrSize);

        if (ret == SOCKET_ERROR) {
            packet.setSize(0);
            if (errno == EAGAIN) {
                return 0;
            }
            setInetError("Can't receive UDP packet");
            return -1;
        }
        packet.setSize(ret);
        return 0;
    }
    else {
        packet.setSize(0);
        return 0;
    }
}

int UdpSocket::send(UdpPacket& packet)
{
    int ret = sendto(listenSocket, packet.getData(), packet.getSize(), 0,
            packet.getAddress().getAddress(), sizeof(*packet.getAddress().getAddress()));
    if (ret == SOCKET_ERROR && errno != ECONNREFUSED) {
        setInetError("Can't send UDP packet");
        return -1;
    }
    return 0;
}


int UdpSocket::send(const std::vector<uint8_t>& data, InetAddress destination)
{
    int ret = sendto(listenSocket, &data[0], data.size(), 0,
            destination.getAddress(), sizeof(*destination.getAddress()));
    if (ret == SOCKET_ERROR && errno != ECONNREFUSED) {
        setInetError("Can't send UDP packet");
        return -1;
    }
    return 0;
}


/**
 *  Must be called to receive data on a multicast address.
 *  @param groupname The multicast address to join.
 *  @return 0 if ok, -1 if error
 */
int UdpSocket::joinGroup(const char* groupname, const char* if_addr)
{
    ip_mreqn group;
    if ((group.imr_multiaddr.s_addr = inet_addr(groupname)) == INADDR_NONE) {
        setInetError(groupname);
        return -1;
    }
    if (!IN_MULTICAST(ntohl(group.imr_multiaddr.s_addr))) {
        setInetError("Not a multicast address");
        return -1;
    }
    group.imr_address.s_addr = inet_addr(if_addr);
    group.imr_ifindex = 0;
    if (setsockopt(listenSocket, IPPROTO_IP, IP_ADD_MEMBERSHIP, &group, sizeof(group))
            == SOCKET_ERROR) {
        setInetError("Can't join multicast group");
    }
    return 0;
}

int UdpSocket::setMulticastTTL(int ttl)
{
    if (setsockopt(listenSocket, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(ttl))
            == SOCKET_ERROR) {
        setInetError("Can't set ttl");
        return -1;
    }

    return 0;
}

int UdpSocket::setMulticastSource(const char* source_addr)
{
    struct in_addr addr;
    if (inet_aton(source_addr, &addr) == 0) {
        setInetError("Can't parse source address");
        return -1;
    }

    if (setsockopt(listenSocket, IPPROTO_IP, IP_MULTICAST_IF, &addr, sizeof(addr))
            == SOCKET_ERROR) {
        setInetError("Can't set source address");
        return -1;
    }

    return 0;
}

UdpPacket::UdpPacket() { }

UdpPacket::UdpPacket(size_t initSize) :
    m_buffer(initSize)
{ }


void UdpPacket::setSize(size_t newSize)
{
    m_buffer.resize(newSize);
}


uint8_t* UdpPacket::getData()
{
    return &m_buffer[0];
}


void UdpPacket::addData(const void *data, size_t size)
{
    uint8_t *d = (uint8_t*)data;
    std::copy(d, d + size, std::back_inserter(m_buffer));
}

size_t UdpPacket::getSize()
{
    return m_buffer.size();
}

InetAddress UdpPacket::getAddress()
{
    return address;
}

UdpReceiver::~UdpReceiver() {
    m_stop = true;
    m_sock.close();
    if (m_thread.joinable()) {
        m_thread.join();
    }
}

void UdpReceiver::start(int port, std::string& bindto, std::string& mcastaddr, size_t max_packets_queued) {
    m_port = port;
    m_bindto = bindto;
    m_mcastaddr = mcastaddr;
    m_max_packets_queued = max_packets_queued;
    m_thread = std::thread(&UdpReceiver::m_run, this);
}

std::vector<uint8_t> UdpReceiver::get_packet_buffer()
{
    if (m_stop) {
        throw runtime_error("UDP Receiver not running");
    }

    UdpPacket p;
    m_packets.wait_and_pop(p);

    return p.getBuffer();
}

void UdpReceiver::m_run()
{
    // Ensure that stop is set to true in case of exception or return
    struct SetStopOnDestruct {
        SetStopOnDestruct(atomic<bool>& stop) : m_stop(stop) {}
        ~SetStopOnDestruct() { m_stop = true; }
        private: atomic<bool>& m_stop;
    } autoSetStop(m_stop);

    set_thread_name("udp_rx");

    if (IN_MULTICAST(ntohl(inet_addr(m_mcastaddr.c_str())))) {
        m_sock.reinit(m_port, m_mcastaddr);
        m_sock.setMulticastSource(m_bindto.c_str());
        m_sock.joinGroup(m_mcastaddr.c_str(), m_bindto.c_str());
    }
    else {
        m_sock.reinit(m_port, m_bindto);
    }

    const size_t packsize = 8192;
    UdpPacket packet(packsize);

    while (not m_stop) {
        int ret = m_sock.receive(packet);
        if (ret == 0) {
            if (packet.getSize() == packsize) {
                // TODO replace fprintf
                fprintf(stderr, "Warning, possible UDP truncation\n");
            }

            // If this blocks, the UDP socket will lose incoming packets
            m_packets.push_wait_if_full(packet, m_max_packets_queued);
        }
        else {
            if (inetErrNo != EINTR) {
                // TODO replace fprintf
                fprintf(stderr, "Socket error: %s\n", inetErrMsg);
            }
            m_stop = true;
        }
    }
}

