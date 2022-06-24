#include "lat_utils.h"
#include <sys/uio.h>

int main(int argc, char** argv)
{
    if (argc < 4)
    {
        std::cout << "bin <ip> <port> <count>" << std::endl;
        return EXIT_FAILURE;
    }
    std::string ip = argv[1];
    std::string port = argv[2];
    std::string count = argv[3];
    size_t target;
    std::stringstream ss;
    ss << count; ss >> target;

    auto fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0)
    {
        std::cout << "ERROR["  << errno << "] mean["
                << strerror(errno) << "]during socket call" << std::endl;
        return EXIT_FAILURE;
    }
    
    int ret = 1;
    int on = 1;
    ret = setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &on, sizeof(on));
    if (ret < 0)
    {
        std::cout << "ERROR["  << errno << "] mean["
                << strerror(errno) << "] during setsockopt call" << std::endl;
        return EXIT_FAILURE;
    }

    // ret = setNonblock(fd);
    if (ret < 0)
    {
        std::cout << "ERROR["  << errno << "] mean["
                << strerror(errno) << "] during setsockopt call" << std::endl;
        return EXIT_FAILURE;
    }
    struct addrinfo hint;
    struct addrinfo *result, *rp;

    memset(&hint, 0, sizeof(hint));
    hint.ai_flags = 0;
    hint.ai_family = AF_INET;
    hint.ai_socktype = SOCK_STREAM;
    hint.ai_canonname = nullptr;
    hint.ai_addr = nullptr;
    hint.ai_next = nullptr;

    ret = getaddrinfo(ip.c_str(), port.c_str(), &hint, &result);
    if (ret < 0)
    {
        std::cout << "ERROR["  << ret << "] mean["
                << gai_strerror(ret) << "]during getaddrinfo call" << std::endl;
        return EXIT_FAILURE;
    }

    errno = 0;
    for (rp = result; rp != NULL; rp = rp->ai_next)
    {
        ret = connect(fd,rp->ai_addr,rp->ai_addrlen);
        std::cout << "ret[" << ret 
            << "] ai_type[" << rp->ai_socktype 
            << "] ai_protcol[" << rp->ai_protocol
            << "] rp[" << !rp 
            << "]" << std::endl;
        if (ret == 0)
        {
            break;
        }
    }
    if (!rp)
    {
        std::cout << "ERROR["  << errno << "] mean["
                << strerror(errno) << "]during connect call" << std::endl;
        return EXIT_FAILURE;
    }

    LatMeasure latMeasure(target);
    
    Epoll epoll(latMeasure);
    
    ret = epoll.registerTransport(fd, Type::RECV);
    if (ret < 0 )
    {
        std::cout << "ERROR["  << errno << "] mean["
                << strerror(ret) << "] during epoll_create call" << std::endl;
        return EXIT_FAILURE;
    }

    sendPacket(fd);
    std::cout << "ret[" << ret << "]" << std::endl;
    if (ret < 0)
    {
        std::cout << "ERROR["  << errno << "] mean["
                << strerror(errno) << "] during send call" << std::endl;
        return EXIT_FAILURE;
    }

    auto* sh = qb::SignalHandler::getInstance();
    while (true)
    {
        if (sh->gotSignal() && sh->isExitSignal())
        {
            std::cout << "recieved kill" << std::endl;
            break;
        }
        if (!epoll.runOnce()
            || latMeasure.reached())
        {
            break;
        }
    }
    latMeasure.stats();

    struct iovec iov[2];
    int a = 1;
    float b = 1.0;
    iov[0].iov_base = &a;
    iov[0].iov_len = sizeof(a);
    iov[1].iov_base = &b;
    iov[1].iov_len = sizeof(b);
    writev(fd,iov,2);


    sleep(1);

    
    return EXIT_SUCCESS;
}
