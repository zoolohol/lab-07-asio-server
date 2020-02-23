// Copyright 2018 Your Name <your_email>
#include <header.hpp>
#include <deque>

constexpr static int PORT = 60013;
struct Client {
    tcp::socket socket;
    std::string login;
    std::chrono::system_clock::time_point lastLogin;
    boost::asio::streambuf buffer;
    std::string clientList;
    explicit Client(boost::asio::io_context& ioContext):socket(ioContext),
    login(),
    lastLogin(std::chrono::system_clock::now()),
    buffer(),
    clientList(){
    }
    void updateTime(){
        lastLogin = std::chrono::system_clock::now();
    }
};
using std::chrono_literals::operator""ms;
using std::chrono_literals::operator""s;
int main(){
    std::recursive_mutex mutex;
    logging::add_file_log
    (
            keywords::file_name = "sample_%N.log",
            keywords::rotation_size = 10 * 1024 * 1024,
            keywords::time_based_rotation
             = sinks::file::rotation_at_time_point(0, 0, 0),
            keywords::format = "[%TimeStamp%]: %Message%");
    using logging::trivial::severity_level;
    using logging::trivial::severity_level::info;
    src::severity_logger<severity_level> lg;
    boost::asio::io_context ioContext;
    tcp::acceptor acceptor(ioContext, tcp::endpoint(tcp::v4(), PORT));
    std::deque<std::shared_ptr<Client>> clients;
    std::thread connectionHandler([&acceptor, &ioContext, &mutex, &clients](){
        while (true) {
            auto client = std::make_shared<Client>(ioContext);
            acceptor.accept(client -> socket);
            std::scoped_lock lock(mutex);
            clients.push_back(client);
            std::this_thread::sleep_for(1ms);
        }
    });
    std::thread clientHandler([&mutex, &clients, &lg](){
        while (true) {
            std::this_thread::sleep_for(1ms);
            std::scoped_lock lock(mutex);
            for (const auto &client : clients) {
                error_code error;
                if (client->socket.available(error)) {
                    boost::asio::read_until(
                            client->socket,
                            client->buffer,
                            "\n",
                            error);
                    if (!error) {
                        client->updateTime();
                        std::istream is(&(client->buffer));
                        std::string message;
                        is >> message;
                        std::cout << message << "\n";
                        if (message == "ping") {
                            std::string a;
                            client->clientList = "";
                            for (const auto &clientInner : clients) {
                                a += clientInner->login + ", ";
                            }
                            if (a != client->clientList) {
                                client->clientList = a;
                                boost::asio::streambuf output;
                                std::ostream os(&output);
                                os << "client_list_changed\n";
                                boost::asio::write(
                                        client->socket,
                                        output,
                                        error);
                            }
                        } else if (message == "clients") {
                            boost::asio::streambuf output;
                            std::ostream os(&output);
                            client->clientList = "";
                            for (const auto &clientInner : clients) {
                                client->clientList += clientInner->login + ", ";
                            }
                            os << client->clientList << "\n";
                            boost::asio::write(client->socket, output, error);
                        } else if (message.substr(0, 5) == "login") {
                            client->login = message.substr(6);
                            BOOST_LOG_SEV(lg, info) << "New client: "
                             << message.substr(6);
                            for (const auto &clientInner : clients) {
                                client->clientList += clientInner->login + ", ";
                                boost::asio::streambuf output;
                                std::ostream os(&output);
                                os << "login ok\n";
                                boost::asio::write(
                                        client->socket,
                                        output,
                                        error);
                            }
                        }
                    }
                }
            }
        }
    });
    connectionHandler.detach();
    clientHandler.detach();
    while (true) {
        std::this_thread::sleep_for(1ms);
    }
}
