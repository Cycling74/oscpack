#pragma once
/*
    oscpack -- Open Sound Control (OSC) packet manipulation library
    http://www.rossbencina.com/code/oscpack

    Copyright (c) 2004-2013 Ross Bencina <rossb@audiomulch.com>

    Permission is hereby granted, free of charge, to any person obtaining
    a copy of this software and associated documentation files
    (the "Software"), to deal in the Software without restriction,
    including without limitation the rights to use, copy, modify, merge,
    publish, distribute, sublicense, and/or sell copies of the Software,
    and to permit persons to whom the Software is furnished to do so,
    subject to the following conditions:

    The above copyright notice and this permission notice shall be
    included in all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
    EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
    MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
    IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR
    ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
    CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
    WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

/*
    The text above constitutes the entire oscpack license; however,
    the oscpack developer(s) also make the following non-binding requests:

    Any person wishing to distribute modifications to the Software is
    requested to send the modifications to the original developer so that
    they can be incorporated into the canonical version. It is also
    requested that these non-binding requests be included whenever the
    above license is reproduced.
*/
#include <oscpack/ip/AbstractUdpSocket.h>
#include <atomic>
#include <pthread.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h> // for sockaddr_in

#include <signal.h>
#include <math.h>
#include <errno.h>
#include <string.h>

#include <algorithm>
#include <cassert>
#include <cstring> // for memset
#include <stdexcept>
#include <chrono>
#include <vector>

#include <oscpack/ip/PacketListener.h>
#include <oscpack/ip/TimerListener.h>

namespace oscpack
{

namespace posix
{

inline void SockaddrFromIpEndpointName( struct sockaddr_in& sockAddr, const IpEndpointName& endpoint )
{
  std::memset( (char *)&sockAddr, 0, sizeof(sockAddr ) );
  sockAddr.sin_family = AF_INET;

  sockAddr.sin_addr.s_addr =
      (endpoint.address == IpEndpointName::ANY_ADDRESS)
      ? INADDR_ANY
      : htonl( endpoint.address );

  sockAddr.sin_port =
      (endpoint.port == IpEndpointName::ANY_PORT)
      ? 0
      : htons( endpoint.port );
}

inline IpEndpointName IpEndpointNameFromSockaddr( const struct sockaddr_in& sockAddr )
{
  return IpEndpointName(
        (sockAddr.sin_addr.s_addr == INADDR_ANY)
        ? IpEndpointName::ANY_ADDRESS
        : ntohl( sockAddr.sin_addr.s_addr ),
        (sockAddr.sin_port == 0)
        ? IpEndpointName::ANY_PORT
        : ntohs( sockAddr.sin_port )
          );
}

class UdpSocketImplementation{
    bool isBound_{};
    bool isConnected_{};

    int socket_{};
    struct sockaddr_in connectedAddr_;
    struct sockaddr_in sendToAddr_;
    int localPort_{};

public:

    UdpSocketImplementation()
        : isBound_( false )
        , isConnected_( false )
        , socket_( -1 )
    {
        if( (socket_ = socket( AF_INET, SOCK_DGRAM, 0 )) == -1 ){
            throw std::runtime_error("unable to create udp socket\n");
        }

        std::memset( &sendToAddr_, 0, sizeof(sendToAddr_) );
        sendToAddr_.sin_family = AF_INET;
    }

    ~UdpSocketImplementation()
    {
        if (socket_ != -1) close(socket_);
    }

    void SetEnableBroadcast( bool enableBroadcast )
    {
        int broadcast = (enableBroadcast) ? 1 : 0; // int on posix
        setsockopt(socket_, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof(broadcast));
    }

    void SetAllowReuse( bool allowReuse )
    {
        int reuseAddr = (allowReuse) ? 1 : 0; // int on posix
        setsockopt(socket_, SOL_SOCKET, SO_REUSEADDR, &reuseAddr, sizeof(reuseAddr));

#ifdef __APPLE__
        // needed also for OS X - enable multiple listeners for a single port on same network interface
        int reusePort = (allowReuse) ? 1 : 0; // int on posix
        setsockopt(socket_, SOL_SOCKET, SO_REUSEPORT, &reusePort, sizeof(reusePort));
#endif
    }

    IpEndpointName LocalEndpointFor( const IpEndpointName& remoteEndpoint ) const
    {
        assert( isBound_ );

        // first connect the socket to the remote server

        struct sockaddr_in connectSockAddr;
        SockaddrFromIpEndpointName( connectSockAddr, remoteEndpoint );

        if (connect(socket_, (struct sockaddr *)&connectSockAddr, sizeof(connectSockAddr)) < 0) {
            throw std::runtime_error("unable to connect udp socket\n");
        }

        // get the address

        struct sockaddr_in sockAddr;
        std::memset( (char *)&sockAddr, 0, sizeof(sockAddr ) );
        socklen_t length = sizeof(sockAddr);
        if (getsockname(socket_, (struct sockaddr *)&sockAddr, &length) < 0) {
            throw std::runtime_error("unable to getsockname\n");
        }

        if( isConnected_ ){
            // reconnect to the connected address

            if (connect(socket_, (struct sockaddr *)&connectedAddr_, sizeof(connectedAddr_)) < 0) {
                throw std::runtime_error("unable to connect udp socket\n");
            }

        }else{
            // unconnect from the remote address

            struct sockaddr_in unconnectSockAddr;
            std::memset( (char *)&unconnectSockAddr, 0, sizeof(unconnectSockAddr ) );
            unconnectSockAddr.sin_family = AF_UNSPEC;
            // address fields are zero
            int connectResult = connect(socket_, (struct sockaddr *)&unconnectSockAddr, sizeof(unconnectSockAddr));
            if ( connectResult < 0 && errno != EAFNOSUPPORT ) {
                throw std::runtime_error("unable to un-connect udp socket\n");
            }
        }

        return IpEndpointNameFromSockaddr( sockAddr );
    }

    void Connect( const IpEndpointName& remoteEndpoint )
    {
        SockaddrFromIpEndpointName( connectedAddr_, remoteEndpoint );

        if (connect(socket_, (struct sockaddr *)&connectedAddr_, sizeof(connectedAddr_)) < 0) {
            throw std::runtime_error("unable to connect udp socket\n");
        }
        sockaddr_in local_sock;
        socklen_t len = sizeof(local_sock);
        getsockname(socket_, (struct sockaddr *) &local_sock, &len);
        if(len == sizeof(local_sock))
          localPort_ = ntohs(local_sock.sin_port);

        isConnected_ = true;
    }

    int LocalPort() const
    {
      return localPort_;
    }

    void Send( const char *data, std::size_t size )
    {
        assert( isConnected_ );

        send( socket_, data, size, 0 );
    }

    void SendTo( const IpEndpointName& remoteEndpoint, const char *data, std::size_t size )
    {
        sendToAddr_.sin_addr.s_addr = htonl( remoteEndpoint.address );
        sendToAddr_.sin_port = htons( remoteEndpoint.port );

        sendto( socket_, data, size, 0, (sockaddr*)&sendToAddr_, sizeof(sendToAddr_) );
    }

    void Bind( const IpEndpointName& localEndpoint )
    {
        struct sockaddr_in bindSockAddr;
        SockaddrFromIpEndpointName( bindSockAddr, localEndpoint );

        if (::bind(socket_, (struct sockaddr *)&bindSockAddr, sizeof(bindSockAddr)) < 0) {
            throw std::runtime_error("unable to bind udp socket\n");
        }

        isBound_ = true;
    }

    bool IsBound() const { return isBound_; }

    std::size_t ReceiveFrom( IpEndpointName& remoteEndpoint, char *data, std::size_t size )
    {
        assert( isBound_ );

        struct sockaddr_in fromAddr;
        socklen_t fromAddrLen = sizeof(fromAddr);

        ssize_t result = recvfrom(socket_, data, size, 0,
                    (struct sockaddr *) &fromAddr, (socklen_t*)&fromAddrLen);
        if( result < 0 )
            return 0;

        remoteEndpoint.address = ntohl(fromAddr.sin_addr.s_addr);
        remoteEndpoint.port = ntohs(fromAddr.sin_port);

        return (std::size_t)result;
    }

    int Socket() { return socket_; }
};



struct AttachedTimerListener{
    AttachedTimerListener( int id, int p, TimerListener *tl )
        : initialDelayMs( id )
        , periodMs( p )
        , listener( tl ) {}
    int initialDelayMs;
    int periodMs;
    TimerListener *listener;
};


inline bool CompareScheduledTimerCalls(
        const std::pair< double, AttachedTimerListener > & lhs, const std::pair< double, AttachedTimerListener > & rhs )
{
    return lhs.first < rhs.first;
}

template<typename UdpSocket_T>
class SocketReceiveMultiplexerImplementation
{
    std::vector< std::pair< PacketListener*, UdpSocket_T* > > socketListeners_;
    std::vector< AttachedTimerListener > timerListeners_;

    std::atomic_bool break_;
    int breakPipe_[2]; // [0] is the reader descriptor and [1] the writer

    double GetCurrentTimeMs() const
    {
      using namespace std::chrono;
      using clk = steady_clock;
      return duration_cast<milliseconds>(clk::now().time_since_epoch()).count();
    }

public:
    SocketReceiveMultiplexerImplementation()
    {
        if( pipe(breakPipe_) != 0 )
            throw std::runtime_error( "creation of asynchronous break pipes failed\n" );
    }

    ~SocketReceiveMultiplexerImplementation()
    {
        close( breakPipe_[0] );
        close( breakPipe_[1] );
    }

    void AttachSocketListener( UdpSocket_T *socket, PacketListener *listener )
    {
        assert( std::find( socketListeners_.begin(), socketListeners_.end(), std::make_pair(listener, socket) ) == socketListeners_.end() );
        // we don't check that the same socket has been added multiple times, even though this is an error
        socketListeners_.push_back( std::make_pair( listener, socket ) );
    }

    void DetachSocketListener( UdpSocket_T *socket, PacketListener *listener )
    {
        auto i = std::find( socketListeners_.begin(), socketListeners_.end(), std::make_pair(listener, socket) );
        assert( i != socketListeners_.end() );

        socketListeners_.erase( i );
    }

    void AttachPeriodicTimerListener( int periodMilliseconds, TimerListener *listener )
    {
        timerListeners_.push_back( AttachedTimerListener( periodMilliseconds, periodMilliseconds, listener ) );
    }

    void AttachPeriodicTimerListener( int initialDelayMilliseconds, int periodMilliseconds, TimerListener *listener )
    {
        timerListeners_.push_back( AttachedTimerListener( initialDelayMilliseconds, periodMilliseconds, listener ) );
    }

    void DetachPeriodicTimerListener( TimerListener *listener )
    {
        std::vector< AttachedTimerListener >::iterator i = timerListeners_.begin();
        while( i != timerListeners_.end() ){
            if( i->listener == listener )
                break;
            ++i;
        }

        assert( i != timerListeners_.end() );

        timerListeners_.erase( i );
    }

    void Run()
    {
        break_ = false;
        char *data = 0;

        try{

            // configure the master fd_set for select()

            fd_set masterfds, tempfds;
            FD_ZERO( &masterfds );
            FD_ZERO( &tempfds );

            // in addition to listening to the inbound sockets we
            // also listen to the asynchronous break pipe, so that AsynchronousBreak()
            // can break us out of select() from another thread.
            FD_SET( breakPipe_[0], &masterfds );
            int fdmax = breakPipe_[0];

            for( auto i = socketListeners_.begin();
                    i != socketListeners_.end(); ++i ){

                if( fdmax < i->second->Socket() )
                    fdmax = i->second->Socket();
                FD_SET( i->second->Socket(), &masterfds );
            }


            // configure the timer queue
            double currentTimeMs = GetCurrentTimeMs();

            // expiry time ms, listener
            std::vector< std::pair< double, AttachedTimerListener > > timerQueue_;
            for( auto i = timerListeners_.begin();
                    i != timerListeners_.end(); ++i )
                timerQueue_.push_back( std::make_pair( currentTimeMs + i->initialDelayMs, *i ) );
            std::sort( timerQueue_.begin(), timerQueue_.end(), CompareScheduledTimerCalls );

            const int MAX_BUFFER_SIZE = 4098;
            data = new char[ MAX_BUFFER_SIZE ];
            IpEndpointName remoteEndpoint;

            struct timeval timeout;

            while( !break_ ){
                tempfds = masterfds;

                struct timeval *timeoutPtr = 0;
                if( !timerQueue_.empty() ){
                    double timeoutMs = timerQueue_.front().first - GetCurrentTimeMs();
                    if( timeoutMs < 0 )
                        timeoutMs = 0;

                    long timoutSecondsPart = (long)(timeoutMs * .001);
                    timeout.tv_sec = (time_t)timoutSecondsPart;
                    // 1000000 microseconds in a second
                    timeout.tv_usec = (suseconds_t)((timeoutMs - (timoutSecondsPart * 1000)) * 1000);
                    timeoutPtr = &timeout;
                }

                if( select( fdmax + 1, &tempfds, 0, 0, timeoutPtr ) < 0 ){
                    if( break_ ){
                        break;
                    }else if( errno == EINTR ){
                        // on returning an error, select() doesn't clear tempfds.
                        // so tempfds would remain all set, which would cause read( breakPipe_[0]...
                        // below to block indefinitely. therefore if select returns EINTR we restart
                        // the while() loop instead of continuing on to below.
                        continue;
                    }else{
                        throw std::runtime_error("select failed\n");
                    }
                }

                if( FD_ISSET( breakPipe_[0], &tempfds ) ){
                    // clear pending data from the asynchronous break pipe
                    char c;
                    read( breakPipe_[0], &c, 1 );
                }

                if( break_ )
                    break;

                for( auto i = socketListeners_.begin();
                        i != socketListeners_.end(); ++i ){

                    if( FD_ISSET( i->second->Socket(), &tempfds ) ){

                        std::size_t size = i->second->ReceiveFrom( remoteEndpoint, data, MAX_BUFFER_SIZE );

                        if(size == 8 && strcmp(data, "__stop_") == 0)
                        {
                            break_ = true;
                            break;
                        }

                        if( size > 0 ){
                            i->first->ProcessPacket( data, (int)size, remoteEndpoint );
                            if( break_ )
                                break;
                        }
                    }
                }

                // execute any expired timers
                currentTimeMs = GetCurrentTimeMs();
                bool resort = false;
                for( std::vector< std::pair< double, AttachedTimerListener > >::iterator i = timerQueue_.begin();
                        i != timerQueue_.end() && i->first <= currentTimeMs; ++i ){

                    i->second.listener->TimerExpired();
                    if( break_ )
                        break;

                    i->first += i->second.periodMs;
                    resort = true;
                }
                if( resort )
                    std::sort( timerQueue_.begin(), timerQueue_.end(), CompareScheduledTimerCalls );
            }

            delete [] data;
        }catch(...){
            if( data )
                delete [] data;
            throw;
        }
    }

    void Break()
    {
        break_ = true;
    }

    void AsynchronousBreak()
    {
        break_ = true;

        // Send a termination message to the asynchronous break pipe, so select() will return
        write( breakPipe_[1], "!", 1 );
    }
};

struct Implementation
{
    using udp_socket_t = oscpack::posix::UdpSocketImplementation;
    using socket_multiplexer_t = oscpack::posix::SocketReceiveMultiplexerImplementation<udp_socket_t>;
};
}

}
