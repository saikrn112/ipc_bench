#include <iostream>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <unistd.h>
#include <fcntl.h>
#include <chrono>
#include <vector>
#include <sstream>
#include <cmath>
#include "SignalHandler.h"

#define NOW std::chrono::system_clock::now().time_since_epoch().count()/1000;

enum class Type
{
    ACCEPT = 1,    
    RECV = 2,
    RECV_FD = 3,
    EVENT = 4,
};

inline std::ostream& operator << (std::ostream& os, const Type& loc)
{
    switch(loc)
    {
        case Type::ACCEPT:
        {
            return os  << "ACCEPT";
        }
        case Type::RECV:
        {
            return os << "RECV";
        }
        case Type::RECV_FD:
        {
            return os << "RECV_FD";
        }
        case Type::EVENT:
        {
            return os << "EVENT";
        }
        default:
        {
            return os << "UNKNOWN";
        }
    }
    return os;
}

enum class Loc
{
    CLIENT = 1,
    SERVER = 2,
};

inline std::ostream& operator << (std::ostream& os, const Loc& loc)
{
    switch(loc)
    {
        case Loc::CLIENT:
        {
            return os  << "CLIENT";
        }
        case Loc::SERVER:
        {
            return os << "SERVER";
        }
        default:
        {
            return os << "UNKNOWN";
        }
    }
    return os;
}

struct IO
{
    Type type;
    int socket;
    int fd;
};

inline bool setNonblock(int fd)
{
    int flags;
    flags = fcntl(fd, F_GETFL, 0);
    
    if (flags < 0)
        return false;

    flags |= O_NONBLOCK;
    return fcntl(fd, F_SETFL, flags) == 0;
}


int sendPacket(int fd)
{
    size_t lat;
    lat = NOW;
    return send(fd, (void*)&lat, sizeof(size_t), 0);
}

class LatMeasure
{
private:
    std::vector<size_t> latPackets;
    size_t target;
    size_t min;
    size_t max;
public:
    LatMeasure(const size_t& size)
    : latPackets()
    , target(size)
    , min(std::numeric_limits<size_t>::max())
    , max(std::numeric_limits<size_t>::min())
    {
        latPackets.reserve(size);
    }
    void record(const size_t& latPacket)
    {
        auto now = NOW;
        size_t lat = now - latPacket;
        latPackets.push_back(lat);
        if (lat < min)
        {
            min = lat;
        }
        if (lat > max)
        {
            max = lat;
        }
    }
    void stats() const
    {
        if (latPackets.empty())
        {
            return;
        }
        uint32_t latsumUs = 0;
        for (uint32_t ii=0; ii < latPackets.size(); ++ii) 
        {
            latsumUs += latPackets[ii];
        }
        uint32_t mean = latsumUs/latPackets.size();
        int64_t sumdiffsq = 0;
        for (uint32_t ii=0; ii < latPackets.size(); ++ii) 
        {
            uint32_t diff = latPackets[ii]-mean;
            sumdiffsq += diff*diff;
        }
        int32_t var = sumdiffsq/(latPackets.size()-1);
        int32_t std = std::sqrt(var);
        std::cout << "server count: " << latPackets.size() 
            << " min=" << min 
            << "us mean=" << mean 
            << "us max=" << max 
            << "us std=" << std << "us" << std::endl;
    }
    bool reached() const
    {
        return latPackets.size() == target;
    }
};

class Epoll
{
private:
    int m_epoll;
    struct epoll_event m_events[1024];
    LatMeasure& m_latMeasure;
public:

    Epoll(LatMeasure& latMeasure)
    : m_epoll(-1)
    , m_latMeasure(latMeasure)
    {
        m_epoll = epoll_create(1024);
        if (m_epoll < 0 )
        {
            std::stringstream ss;
            ss << "ERROR["  << errno << "] mean["
                    << strerror(errno) << "] during epoll_create call";
            std::cout << ss.str() << std::endl;
            throw std::runtime_error(ss.str());
        }
    }
    
    int registerTransport(int socket, Type type)
    {
        std::cout << "fd[" << socket << "] type[" << type << "]" << std::endl;
        struct epoll_event ev;
        ev.events = EPOLLIN
                // | EPOLLOUT
                // | EPOLLPRI
                | EPOLLERR
                | EPOLLHUP
                | EPOLLET;

        IO* io = new IO();
        io->type = type;
        io->socket = socket;
        ev.data.ptr = (void*)io;

        int ret = epoll_ctl(m_epoll, EPOLL_CTL_ADD, socket, &ev);
        if (ret < 0)
        {
            std::cout << "ERROR[" << errno << "]mean["
                << strerror(errno) << "] during epoll_ctl add call" << std::endl;
        }
        return ret;
    }

    int deregisterTransport(int socket)
    {
        struct epoll_event ev;
        ev.events = EPOLLIN 
                    | EPOLLOUT
                    | EPOLLPRI 
                    | EPOLLERR
                    | EPOLLHUP
                    | EPOLLET;

        ev.data.fd = socket;

        errno = 0;
        int ret = epoll_ctl(m_epoll, EPOLL_CTL_DEL, socket, &ev);
        if (ret < 0)
        {
            std::cout << "ERROR[" << errno << "]mean["
                << strerror(errno) << "] during epoll_ctl del call" << std::endl;
        }
        return ret;
    }

    bool runOnce()
    {
        auto ret = epoll_wait(m_epoll, m_events, sizeof(m_events)/sizeof(m_events[0]), 0);

        if (ret < 0 && errno == EINTR)
        {
            std::cout << "ERROR[epoll interrupted]" << std::endl;
            return true;
        }
        if (ret < 0 )
        {
            std::cout << "ERROR["  << errno << "] mean["
                    << strerror(errno) << "] during epoll" << std::endl;
            return false;
        }

        for (int i = 0; i < ret; i++)
        {
            IO* io  = (IO*)m_events[i].data.ptr;
            if (!io)
            {
                std::cout << "something wrong i[" << i << "]" << std::endl;
                continue;
            }
            // std::cout << "Nevents[" << ret 
            //     << "] io[" << io->type 
            //     << "] event[" << m_events[i].events
            //     << "]" << std::endl;
            switch (io->type)
            {
            case Type::ACCEPT:
            {
                std::cout << "new connection" << std::endl;
                struct sockaddr_in addr;
                memset(&addr, 0, sizeof(addr));
                socklen_t addrlen = sizeof(addr);
                auto c_fd = ::accept(io->socket, (struct sockaddr*)&addr, &addrlen);
                if (c_fd < 0)
                {
                    std::cout << "ERROR["  << errno << "] mean["
                            << strerror(errno) << "] during accept call" << std::endl;
                    continue;
                }
                registerTransport(c_fd,Type::RECV_FD); 
                break;
            }
            case Type::RECV:
            {
                if (m_events[i].events & EPOLLERR
                    || m_events[i].events & EPOLLHUP)
                {
                    std::cout << "deregistering" << std::endl;
                    deregisterTransport(io->socket);
                    ::close(io->socket);
                    break;
                }
                size_t lat;
                size_t rec = recv(io->socket, &lat, sizeof(size_t), 0);
                if (rec < 0)
                {
                    std::cout << "ERROR["  << errno << "] mean["
                            << strerror(errno) << "] during recv call" << std::endl;
                }
                m_latMeasure.record(lat);
                sendPacket(io->socket);
                break;
            }
            case Type::RECV_FD:
            {
                if (m_events[i].events & EPOLLERR
                    || m_events[i].events & EPOLLHUP)
                {
                    std::cout << "deregistering" << std::endl;
                    deregisterTransport(io->socket);
                    ::close(io->socket);
                    break;
                }
                int event_fd;
                char buf[CMSG_SPACE(sizeof(event_fd))];
                struct msghdr msg;
                memset((void*)&msg,0,sizeof(msghdr));
                msg.msg_control = buf;
                msg.msg_controllen = sizeof(buf);

                size_t rec = recvmsg(io->socket, &msg, 0);
                if (rec < 0)
                {
                    std::cout << "ERROR["  << errno << "] mean["
                            << strerror(errno) << "] during recvmsg call" << std::endl;
                }
                if (msg.msg_controllen <= 0)
                {
                    std::cout << "nothing to act on" << std::endl;
                    break;
                }

                struct cmsghdr* cmsg = CMSG_FIRSTHDR(&msg);
                int* fdptr = (int*)CMSG_DATA(cmsg);
                memcpy((void*)&event_fd,(void*)fdptr,sizeof(event_fd));
                registerTransport(event_fd,Type::EVENT);
                
                break;
            }
            case Type::EVENT:
            {
                eventfd_t val = 0;
                eventfd_read(io->socket,&val);
                std::cout << "received event[" << val << "]\n";
                break;
            }
                
            }
        }
        return true;
    }

};