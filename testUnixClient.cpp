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

    errno = 0;
    ret = connect(fd, (struct sockaddr*)&addr, SUN_LEN(&addr));
    if (ret < 0)
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
                << strerror(errno) << "] during epoll_create call" << std::endl;
        return EXIT_FAILURE;
    }

    // sendPacket(fd);
    // if (ret < 0)
    // {
    //     std::cout << "ERROR["  << errno << "] mean["
    //             << strerror(errno) << "] during send call" << std::endl;
    //     return EXIT_FAILURE;
    // }

    auto event_fd = eventfd(0,O_NONBLOCK);
    if (event_fd < 0)
    {
        std::cout << "ERROR["  << errno << "] mean["
                << strerror(errno) << "] during eventfd call" << std::endl;
        return EXIT_FAILURE;
    }

    char buf[CMSG_SPACE(sizeof(event_fd))];
    struct msghdr msg;
    memset((void*)&msg,0,sizeof(msghdr));
    msg.msg_control = buf;
    msg.msg_controllen = sizeof(buf);

    struct cmsghdr* cmsg = CMSG_FIRSTHDR(&msg);
    cmsg->cmsg_level = SOL_SOCKET;
    cmsg->cmsg_type = SCM_RIGHTS;
    cmsg->cmsg_len = CMSG_LEN(sizeof(event_fd));

    int* fdptr = (int*)CMSG_DATA(cmsg);
    memcpy(fdptr,(void*)&event_fd,sizeof(event_fd));
    msg.msg_controllen = cmsg->cmsg_len;
    
    sendmsg(fd,&msg,0);

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

    sleep(1);

    
    return EXIT_SUCCESS;
}
