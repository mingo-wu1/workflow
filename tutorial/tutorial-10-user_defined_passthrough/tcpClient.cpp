
#include <iostream>
#include <unistd.h>
#include <vector>
#include <boost/asio.hpp>
#include <boost/system/error_code.hpp>
#include <boost/asio/ip/address.hpp>
using namespace boost::asio;

int main()
{
    try
    {
        typedef ip::tcp::endpoint endpoint_type;
        typedef ip::tcp::socket socket_type;
        typedef ip::address address_type;

        std::cout << "client start." << std::endl;

        io_service io;
        socket_type socket(io);
        endpoint_type ep(
            address_type::from_string("127.0.0.1"),
            6688);

        socket.connect(ep);

        sleep(1);

        std::cout << socket.available() << std::endl;
        // std::vector<char> str(socket.available() + 1, 0);
        char buf[1024];
        buf[1023] = '\0';
        // buf[1] = '\0';
        boost::system::error_code ec;
        for (;;)
        {
            socket.read_some(buffer(buf, 1024), ec);
            if (ec)
            {
                // break;
            }            
            // std::cout << &str[0];
            std::cout<<std::string(buf)<<std::endl;
            sleep(1);
        }

    }
    catch (const std::exception& e)
    {
        std::cout << e.what() << std::endl;
    }

    getchar();
    std::cout << "Hello World!\n";
}

