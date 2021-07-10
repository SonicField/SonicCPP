#include <sys/socket.h>
#include <netinet/in.h>
#include <poll.h>
#include <unordered_map>
#include <functional>
#include <sys/errno.h>
#include <queue>
#include <algorithm>
#include <mutex>
#include <sstream>
#include <fcntl.h>
#include <thread>
#include <chrono>
#include <poll.h>
#include <arpa/inet.h>
#include "sonic_field.h"

namespace comms
{
    using namespace std::chrono_literals;

    class tcp_server;

    static void ent(std::string msg)
    {
        SF_THROW(std::runtime_error{
                msg + ": " + strerror(errno)});
    };

    class net_client
    {
        // Makes writes thread safe.
        std::mutex m_mutex;

        // Tell the user a write is possible.
        std::function<void()> m_on_can_write;

        // Tell the user that data has arrived.
        std::function<void(uint8_t* data, size_t len)> m_on_data;

        // Tell the user that an error has occured
        std::function<void(int)> m_on_error;

        // Call the socket owner to do a write.
        // Write from the vector starting at an offset.
        // Return amount written and an error which is errno in effect.
        std::function<std::pair<size_t,int>(std::vector<uint8_t>&, size_t)> m_do_write;

        // Store data to be written.
        std::queue<std::vector<uint8_t>> m_write_queue;

        // The current block of data being written.
        std::vector<uint8_t> m_current;

        // The current offset in m_current.
        size_t m_current_offset;

        friend class tcp_server;

        // Called by the tcp socket system when ever a write the socket
        // can happen.
        void get_data()
        {
            // Tell the user that a write can happen.
            // This might result in in more data being added.
            // Call this before we lock because write might be called which locks
            // and then we would deadlock.
            std::cout << "Call on_can_write" << std::endl;
            m_on_can_write();
            {
                std::lock_guard<std::mutex> lck{m_mutex};
                if (m_current.size() > m_current_offset)
                {
                    std::cout << "Call do_write: offset=" << m_current_offset << std::endl;
                    auto got = m_do_write(m_current, m_current_offset);
                    m_current_offset += got.first;
                    if (got.second)
                    {
                        m_on_error(got.second);
                    }
                }
                if (m_current.size() == m_current_offset)
                {
                    m_current_offset = 0;
                    if (!m_write_queue.empty())
                    {
                        m_current = std::move(m_write_queue.back());
                        m_write_queue.pop();
                    }
                    else
                    {
                        m_current = decltype(m_current){};
                    }
                }
            }
        }

    public:
        // Add data to be written.
        void write(uint8_t* data, size_t len)
        {
            std::lock_guard<std::mutex> lck{m_mutex};
            std::vector<uint8_t> store{};
            store.reserve(len);
            std::copy(data, data+len, store.begin());
            m_write_queue.emplace(store);
        };
    }; 

    // Create a noddy 'reflection' server.
    class tcp_server
    {
        // Method synchronization
        std::mutex m_sync_mutex;

        // Start stop main loop
        std::mutex m_main_loop_mutex;

        // If the main loop running - should it.
        bool m_main_loop_running;

        // Thread in which the main loop is running/
        std::thread m_main_loop_thread;

        // Server socket fd.
        int m_server_fd;

        // Store the clients for accepted sockets.
        // Maps file descriptor to client.
        std::unordered_map<int, net_client> m_clients;

        uint32_t parse_ip4(std::string saddr)
        {

            SF_MARK_STACK;
            uint32_t ret{};
            if(inet_pton(AF_INET, saddr.c_str(), &ret)<=0) 
            {
                SF_MARK_STACK;
                ent("Could not parse ip address: " + saddr);
             }
            return ret;
        }

        void run_main_loop()
        {
            // Tell the starting thread that this thread is now running.
            m_main_loop_mutex.unlock();
            std::cout << "Main loop running" << std::endl;
            while(m_main_loop_running)
            {
                std::vector<pollfd> pollfds{};
                pollfds.push_back({m_server_fd, POLLIN, 0});
                for(const auto& el: m_clients)
                {
                    pollfds.push_back({el.first, POLLIN, 0});
                }
                if ( 0 >poll(&(pollfds[0]), pollfds.size(), 0))
                {
                    SF_MARK_STACK;
                    ent("Poll failed");
                }
                bool is_server{true};
                for(const auto& an_pfd: pollfds)
                {
                    std::cout << 
                        "Poll result for fd: " << an_pfd.fd << 
                        " events=" << an_pfd.events <<
                        " revents=" << an_pfd.revents << std::endl;
                    if (is_server)
                    {
                        is_server = false;
                        if ( an_pfd.revents & POLLIN)
                        {
                            process_accept(an_pfd.fd);
                        }
                    }
                }
                std::this_thread::sleep_for(500ms);
                std::cout << "Main thread..." << std::endl;
            }
        }

        void process_accept(int fd)
        {
            sockaddr addr{};
            socklen_t len{sizeof(addr)};
            if (0 > accept(fd, &addr, &len))
            {
                SF_MARK_STACK;
                ent("Accept failed");
            }
            std::cout << "Accepted" << std::endl;
        }

        public:
        tcp_server(const std::string& addr, uint16_t port, size_t backlog=128):
            m_main_loop_running{false}
        {
            SF_MARK_STACK;
            if (!(m_server_fd = socket(AF_INET, SOCK_STREAM, 0)))
            {
                SF_MARK_STACK;
                ent("Could not create server socket");
            }
            sockaddr_in address;
            address.sin_family = AF_INET;
            address.sin_addr.s_addr = parse_ip4(addr);
            address.sin_port = htons(port);
            std::cout << "Address == " << address.sin_addr.s_addr << std::endl;
            if (bind(m_server_fd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) < 0)
            {
                SF_MARK_STACK;
                ent("Could not bind socket");
            }

            auto flags = fcntl(m_server_fd, F_GETFL, 0);
            if (flags == -1)
            {
                SF_MARK_STACK;
                ent("Could not get server socket flags");
            }
            if( -1 == fcntl(m_server_fd, F_SETFL, flags | O_NONBLOCK))
            {
                SF_MARK_STACK;
                ent("Could not set server socket flags none blocking");
            }
            if(listen(m_server_fd, backlog))
            {
                SF_MARK_STACK;
                ent("Could not listen on server socket");
            }
        }

        uint16_t server_port() const
        {
            sockaddr_in sin;
            socklen_t len = sizeof(sin);
            if (getsockname(m_server_fd, reinterpret_cast<sockaddr*>(&sin), &len) == -1)
            {
                SF_MARK_STACK;
                ent("Could not get sever socket name");
            }
            return ntohs(sin.sin_port);
        }

        void start_main_loop()
        {
            std::lock_guard<std::mutex> lck{m_sync_mutex};
            if (m_main_loop_running)
            {
                SF_THROW(std::runtime_error{"Main loop already running"});
            }
            m_main_loop_running = true;
            // Lock the m_main_loop_mutex and then we can wait on it, it will then
            // be unlocked when the main thread started.
            m_main_loop_mutex.lock();
            auto starter = [&](){run_main_loop();};
            m_main_loop_thread = std::thread{starter};

            // Block till thread started - i.e. we are double locked here.
            std::lock_guard<std::mutex> started{m_main_loop_mutex};
        }

        void stop_main_loop()
        {
            std::lock_guard<std::mutex> lck{m_sync_mutex};
            if (!m_main_loop_running)
            {
                SF_THROW(std::runtime_error{"Main loop not running"});
            }
            m_main_loop_running = false;
            m_main_loop_thread.join();
        }
    };

    void run_tests()
    {
        std::cout << "Running comms tests" << std::endl;
        auto server = tcp_server{"127.0.0.1", 0};
        std::cout << "Bound to port: " << server.server_port() << std::endl;
        server.start_main_loop();
        std::cout << "Main loop started" << std::endl;
        std::this_thread::sleep_for(2000ms);

        struct sockaddr_in serv_addr;
        int sock_fd;
        if ((sock_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        {
            ent("Create client socket failed");
        }
        serv_addr.sin_family = AF_INET;
        serv_addr.sin_port = htons(server.server_port() );
        if(inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr)<=0) 
        {
            ent("Set ip address of socket failed");
        }
        
        std::cout << "About to connect!" << std::endl;
        if (connect(sock_fd, reinterpret_cast<sockaddr *>(&serv_addr), sizeof(serv_addr)) < 0)
        {
            ent("Connect failed");
        }

        std::this_thread::sleep_for(2000ms);
        server.stop_main_loop();
        std::cout << "Main loop stopped" << std::endl;
    }
}
