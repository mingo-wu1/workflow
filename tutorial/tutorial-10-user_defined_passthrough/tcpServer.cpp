#include <iostream>
#include <boost/asio.hpp>
#include <boost/system/error_code.hpp>
#include <boost/asio/ip/address.hpp>
#include <unistd.h>
using namespace boost::asio;

int main()
{
    try 
    {
        typedef ip::tcp::acceptor acceptor_type;
        typedef ip::tcp::endpoint endpoint_type;
        typedef ip::tcp::socket socket_type;

        std::cout << "sercer start." << std::endl;
        
        io_service io;
        acceptor_type acceptor(io,
            endpoint_type(ip::tcp::v4(), 6688));
        std::cout << acceptor.local_endpoint().address() << std::endl;

        for (;;)
        {
            socket_type socket(io);
            acceptor.accept(socket);

            std::cout << "client:"<<std::endl;
            // std::cout << socket.remote_endpoint().address() << std::endl;
            // std::cout << socket.local_endpoint().address() << std::endl;
            
            socket.send(buffer("hello client, i am server."));
            sleep(1);
        }
        
    }
    catch (std::exception& e)
    {
        std::cout << e.what() << std::endl;
    }

    getchar();
    std::cout << "Hello World!\n"; 
}
