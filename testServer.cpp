#include "lat_utils.h"

int main(int argc, char** argv)
{
    if (argc < 3)
    {
        std::cout << "bin <ip> <port>" << std::endl;
        return EXIT_FAILURE;
    }
    std::string ip = argv[1];
    std::string port = argv[2];
    auto fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0)
    {
        std::cout << "ERROR["  << errno << "] mean["
                << strerror(errno) << "] during socket call" << std::endl;
        return EXIT_FAILURE;
    }
    
    std::string device("p1p1");
    int ret = 1;
    // char* dev = "p1p1";
    //ret = setsockopt(fd, SOL_SOCKET, SO_BINDTODEVICE, dev, strlen(dev));
    if (ret < 0)
    {
        std::cout << "ERROR["  << errno << "] mean["
                << strerror(errno) << "] during setsockopt call" << std::endl;
        return EXIT_FAILURE;
    }

    int on = 1;
    ret = setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &on, sizeof(on));
    if (ret < 0)
    {
        std::cout << "ERROR["  << errno << "] mean["
                << strerror(errno) << "] during setsockopt call" << std::endl;
        return EXIT_FAILURE;
    }

    ret = setNonblock(fd);
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
                << gai_strerror(ret) << "] during getaddrinfo call" << std::endl;
        return EXIT_FAILURE;
    }

    errno = 0;
    for (rp = result; rp != NULL; rp = rp->ai_next)
    {
        ret = ::bind(fd, rp->ai_addr, rp->ai_addrlen);
        if (ret == 0)
        {
            break;
        }
    }
    if (!rp)
    {
        std::cout << "ERROR["  << errno << "] mean["
                << strerror(ret) << "] during bind call" << std::endl;
        return EXIT_FAILURE;
    }

    ret = ::listen(fd,5);
    if (ret < 0)
    {
        std::cout << "ERROR[" << errno << "] mean["
                << strerror(errno) << "] during listen call" << std::endl;
        return EXIT_FAILURE;
    }
    
    LatMeasure latMeasure(1000);
    Epoll epoll(latMeasure);
    
    ret = epoll.registerTransport(fd, Type::ACCEPT);
    if (ret < 0 )
    {
        std::cout << "ERROR["  << errno << "] mean["
                << strerror(ret) << "] during epoll_create call" << std::endl;
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
        
        if (!epoll.runOnce())
        {
            break;
        }
    }
    

    return EXIT_SUCCESS;
}
