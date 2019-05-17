// Vsevolod Ivanov

#include <stdlib.h>
#include <sstream>
#include <iostream>
#include <chrono>
#include <restinio/all.hpp>

template <typename LAMBDA>
void do_with_socket(LAMBDA && lambda, const std::string & addr = "127.0.0.1",
                                      std::uint16_t port = 8080)
{
    restinio::asio_ns::io_context io_context;
    restinio::asio_ns::ip::tcp::socket socket{io_context};

    restinio::asio_ns::ip::tcp::resolver resolver{io_context};
    restinio::asio_ns::ip::tcp::resolver::query
        query{restinio::asio_ns::ip::tcp::v4(), addr, std::to_string(port)};
    restinio::asio_ns::ip::tcp::resolver::iterator iterator = resolver.resolve(query);

    restinio::asio_ns::connect(socket, iterator);

    lambda(socket, io_context);
    socket.close();
}

inline std::string do_request(const std::string & request,
                              const std::string & addr = "127.0.0.1",
                              std::uint16_t port = 8080)
{
    std::string result;
    do_with_socket([&](auto & socket, auto & io_context)
    {
        restinio::asio_ns::streambuf b;
        std::ostream req_stream(&b);
        req_stream << request;
        restinio::asio_ns::write(socket, b);

        std::ostringstream sout;
        restinio::asio_ns::streambuf response_stream;
        restinio::asio_ns::read_until(socket, response_stream, "\r\n\r\n");

        sout << &response_stream;

        restinio::asio_ns::error_code error;
        while(restinio::asio_ns::read(socket, response_stream,
                                     restinio::asio_ns::transfer_at_least(1),
                                     error))
            sout << &response_stream;

        if (!restinio::error_is_eof(error))
            throw std::runtime_error{fmt::format("read error: {}", error)};

        result = sout.str();
    },
    addr, port);

    return result;
}

std::string create_http_request(const restinio::http_request_header_t header,
                                const restinio::http_header_fields_t header_fields,
                                const restinio::http_connection_header_t connection)
{
    std::stringstream request;

    request << restinio::method_to_string(header.method()) << " " <<
               header.request_target() << " " <<
               "HTTP/" << header.http_major() << "." << header.http_minor() << "\r\n";

    for (auto header_field: header_fields)
        request << header_field.name() << ": " << header_field.value() << "\r\n";

    std::string conn_str;
    switch (connection)
    {
        case restinio::http_connection_header_t::keep_alive:
            conn_str = "keep-alive";
            break;
        case restinio::http_connection_header_t::close:
            conn_str = "close";
            break;
        case restinio::http_connection_header_t::upgrade:
            throw std::invalid_argument("upgrade");
            break;
    }
    request << "Connection: " << conn_str << "\r\n\r\n";

    return request.str();
}

int main(const int argc, char* argv[])
{
    if (argc < 4){
        std::cerr << "Insufficient arguments! Needs <addr> <port> <target>" << std::endl;
        return 1;
    }

    const std::string addr = argv[1];
    const in_port_t port = atoi(argv[2]);
    const std::string target = argv[3];

    restinio::http_request_header_t header;
    header.request_target(target);
    header.method(restinio::http_method_t::http_get);

    restinio::http_header_fields_t header_fields;
    header_fields.append_field(restinio::http_field_t::host,
                               (addr + ":" + std::to_string(port)).c_str());
    header_fields.append_field(restinio::http_field_t::user_agent, "RESTinio client");
    header_fields.append_field(restinio::http_field_t::accept, "*/*");

    auto connection = restinio::http_connection_header_t::keep_alive;

    auto request = create_http_request(header, header_fields, connection);
    printf(request.c_str());

    auto response = do_request(request, addr, port);
    std::cout << response << std::endl;

    return 0;
}
