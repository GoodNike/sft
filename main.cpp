#include <iostream>
#include <string>
#include <fstream>
#include <iterator>

#include <boost/asio.hpp>
#include <boost/program_options.hpp>

#include <cstdlib>

const uint16_t PORT = 8551;
const size_t PAYLOAD = 1500 - 24 - 24;  // (Ethernet payload) - (IP header size) - (TCP header size).

enum PROTOCOL_VERIONS : uint16_t {
    VERSION_01 = 0x0001
};

const uint16_t CURRENT_VERSION = PROTOCOL_VERIONS::VERSION_01;

struct header_t {
    uint16_t version;
    char fileName[PAYLOAD - sizeof(uint16_t)];

    bool load(const char *data, size_t sz)
    {
        // Minimum size: version + minimum file name length.
        if (sz < PAYLOAD) {
            return false;
        }

        version = *reinterpret_cast<const uint16_t*>(data);

//        const auto begin = std::reverse_iterator(&data[2]);
//        const auto end = std::reverse_iterator(&data[sz]);
//        auto pos = std::find(begin, end, '/');
//        if (pos != end) {
//            std::copy(end, pos, fileName);
//        } else {
            std::copy(&data[2], &data[sz], fileName);
//        }

        return true;
    }
};

namespace opt = boost::program_options;

bool receiver()
{
    using namespace boost::asio;

    io_service service;
    ip::tcp::endpoint ep(ip::tcp::v4(), PORT);
    ip::tcp::acceptor acc(service, ep);
    while (true) {
        ip::tcp::socket sock(service);
        acc.accept(sock);
        bool headerRecieved = false;
        std::ofstream file;

        std::cout << "Incoming connection" << std::endl;

        while (true) {
            char data[PAYLOAD];
            boost::system::error_code err;
            size_t len = sock.read_some(buffer(data), err);
            if (!headerRecieved) {
                header_t hdr;
                if (!hdr.load(data, len)) {
                    break;
                }
                if (hdr.version != CURRENT_VERSION) {
                    break;
                }
                file.open(hdr.fileName, std::ios::binary);
                if (!file) {
                    break;
                }
                headerRecieved = true;

                std::cout << "File name: " << hdr.fileName << std::endl;

                continue;
            } else {
                file.write(data, len);
            }

            if (len == 0 || !sock.is_open()) {
                file.close();
                sock.close();

                std::cout << "Finished receiving" << std::endl;

                break;
            }
        }
    }

    return true;
}

bool sender(const std::string &fileName, const std::string &ipAddr)
{
    std::ifstream file(fileName, std::ios::binary);
    if (!file) {
        return false;
    }

    header_t hdr;
    if (fileName.size() >= sizeof(hdr.fileName)) {
        return false;
    }
    hdr.version = CURRENT_VERSION;
    size_t length = fileName.copy(hdr.fileName, fileName.length());
    hdr.fileName[length] = '\0';

    using namespace boost::asio;
    io_service service;
    ip::tcp::endpoint ep(ip::address::from_string(ipAddr), PORT);
    ip::tcp::socket sock(service);
    sock.connect(ep);

    std::cout << "Connection established with receiver, sending file" << std::endl;

    sock.write_some(buffer(&hdr, sizeof(hdr)));

    while (true) {
        char data[PAYLOAD];
        file.read(data, PAYLOAD);
        sock.write_some(buffer(data, file.gcount()));
        if (!file) {
            file.close();
            sock.close();
            break;
        }
    }

    std::cout << "File sended" << std::endl;

    return true;
}

int main(int argc, char *argv[])
{
    opt::options_description desc("sft (Simple File Transfer) options");

    std::string recieverIpAddr;
    std::string fileName;
    std::string ipAddr;

    desc.add_options()
            ("receiver,r", "Start as a receiver")
            ("send,s", opt::value<std::string>(&fileName), "File name to send")
            ("addr,a", opt::value<std::string>(&ipAddr), "Receiver IP address")
            ("help,h", "Show help")
            ;
    opt::variables_map vm;
    opt::store(opt::parse_command_line(argc, argv, desc), vm);
    opt::notify(vm);

    if (vm.count("help")) {
        std::cout << desc << std::endl;
        return EXIT_FAILURE;
    } else if (vm.count("receiver")) {
        // File receiver mode.
        std::cout << "Starting reciever..." << std::endl;
        if (!receiver()) {
            std::cout << "Error running reciever" << std::endl;
            return EXIT_FAILURE;
        }
    } else if (vm.count("send") && vm.count("addr")) {
        // File sending mode.
        std::cout << "Starting sender" << std::endl;
        fileName = vm["send"].as<std::string>();
        ipAddr = vm["addr"].as<std::string>();

        if (!sender(fileName, ipAddr)) {
            std::cout << "Error running sender" << std::endl;
            return EXIT_FAILURE;
        }
    } else {
        std::cout << "Error parsing command line" << std::endl;
        std::cout << desc << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
