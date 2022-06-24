#include "lat_utils.h"

int main(int argc, char** argv)
{
    if (argc < 3)
    {
        std::cout << "bin <unix> <count>" << std::endl;
        return EXIT_FAILURE;
    }
    std::string ip = argv[1];
    std::string count = argv[2];
    size_t target;
    std::stringstream ss;
    ss << count; ss >> target;

    auto fd = socket(AF_UNIX, SOCK_SEQPACKET, 0);
    if (fd < 0)
    {
        std::cout << "ERROR["  << errno << "] mean["
                << strerror(errno) << "]during socket call" << std::endl;
        return EXIT_FAILURE;
    }
    
    int ret = 1;
    ret = setNonblock(fd);
    if (ret < 0)
    {
        std::cout << "ERROR["  << errno << "] mean["
                << strerror(errno) << "] during setsockopt call" << std::endl;
        return EXIT_FAILURE;
    }
    
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, ip.c_str(), sizeof(addr.sun_path) -1);

    ret = ::bind(fd, (struct sockaddr*)&addr, SUN_LEN(&addr));
    if (ret < 0)
    {
        std::cout << "ERROR[" << errno << "] mean["
                << strerror(errno) << "] during bind call[" << ip << "]" << std::endl;
        return EXIT_FAILURE;
    }

    fchmod(fd, S_IRUSR | S_IWUSR
        |  S_IRGRP | S_IWGRP
        |  S_IROTH | S_IWOTH);

    errno = 0;
    ret = ::listen(fd,5);
    if (ret < 0)
    {
        std::cout << "ERROR[" << errno << "] mean["
                << strerror(errno) << "] during listen call[" << ip << "]" << std::endl;
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
    writev

    return EXIT_SUCCESS;
}
