// A tiny loopback HTTP server shared by the crtweb tests (Web Tranche 4): one connection at a time,
// fixed routes for cookies, a download, a popup, JavaScript dialogs and an input field. It makes the
// engine's real network path part of the test without any external host.
#pragma once

#include <arpa/inet.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include <atomic>
#include <string>
#include <thread>

static const size_t kDownloadSize = 70000;

static std::string downloadBody()
{
    std::string body(kDownloadSize, '\0');
    for (size_t i = 0; i < body.size(); ++i)
        body[i] = static_cast<char>((i * 7 + 13) & 0xff);
    return body;
}

class HttpServer {
public:
    HttpServer()
    {
        m_listener = socket(AF_INET, SOCK_STREAM, 0);
        int on = 1;
        setsockopt(m_listener, SOL_SOCKET, SO_REUSEADDR, &on, sizeof on);
        sockaddr_in address = { };
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        address.sin_port = 0;
        bind(m_listener, reinterpret_cast<sockaddr*>(&address), sizeof address);
        listen(m_listener, 16);
        socklen_t length = sizeof address;
        getsockname(m_listener, reinterpret_cast<sockaddr*>(&address), &length);
        m_port = ntohs(address.sin_port);
        m_thread = std::thread([this] { serve(); });
    }

    ~HttpServer()
    {
        m_stop = true;
        // Wake accept() with a connection of our own.
        int fd = socket(AF_INET, SOCK_STREAM, 0);
        sockaddr_in address = { };
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        address.sin_port = htons(m_port);
        connect(fd, reinterpret_cast<sockaddr*>(&address), sizeof address);
        close(fd);
        m_thread.join();
        close(m_listener);
    }

    std::string url(const char* path) const { return "http://127.0.0.1:" + std::to_string(m_port) + path; }

private:
    static void sendAll(int fd, const std::string& data)
    {
        size_t offset = 0;
        while (offset < data.size()) {
            ssize_t sent = send(fd, data.data() + offset, data.size() - offset, 0);
            if (sent <= 0)
                return;
            offset += static_cast<size_t>(sent);
        }
    }

    static std::string page(const std::string& body, const std::string& extraHeaders = std::string())
    {
        return "HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\nContent-Length: " + std::to_string(body.size())
            + "\r\nConnection: close\r\n" + extraHeaders + "\r\n" + body;
    }

    void serve()
    {
        while (!m_stop) {
            int fd = accept(m_listener, nullptr, nullptr);
            if (fd < 0)
                continue;
            if (m_stop) {
                close(fd);
                break;
            }
            std::string request;
            char buffer[2048];
            while (request.find("\r\n\r\n") == std::string::npos) {
                ssize_t got = recv(fd, buffer, sizeof buffer, 0);
                if (got <= 0)
                    break;
                request.append(buffer, static_cast<size_t>(got));
            }
            size_t end = request.find(' ', 4);
            std::string path = request.size() > 4 ? request.substr(4, end == std::string::npos ? 0 : end - 4) : "";
            std::string response;
            if (path == "/set")
                response = page("<!doctype html><title>set</title>ok", "Set-Cookie: sid=abc123; Max-Age=3600; Path=/\r\n");
            else if (path == "/read")
                response = page("<!doctype html><title>x</title><script>document.title='cookie:'+document.cookie;</script>");
            else if (path == "/page")
                response = page("<!doctype html><title>http page</title>hello");
            else if (path == "/dl") {
                std::string body = downloadBody();
                response = "HTTP/1.1 200 OK\r\nContent-Type: application/octet-stream\r\nContent-Disposition: attachment; filename=\"blob.bin\"\r\n"
                    "Content-Length: " + std::to_string(body.size()) + "\r\nConnection: close\r\n\r\n" + body;
            } else if (path == "/opener")
                response = page("<!doctype html><title>opener</title><body style='margin:0'>"
                    "<div id=b style='width:100px;height:100px;background:#fff' onclick=\"window.open('/popup-target')\"></div>");
            else if (path == "/popup-target")
                response = page("<!doctype html><title>popup</title><body style='background:#0a0'>popup");
            else if (path == "/ime")
                response = page("<!doctype html><title>ime</title><body style='margin:0'>"
                    "<input id=t style='position:absolute;left:0;top:0;width:180px;font:20px sans-serif'>"
                    "<script>var t=document.getElementById('t');window.log=[];"
                    "['compositionstart','compositionupdate','compositionend'].forEach(function(n){t.addEventListener(n,function(e){window.log.push(n+':'+e.data);});});"
                    "t.focus();</script>");
            else
                response = "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
            sendAll(fd, response);
            close(fd);
        }
    }

    int m_listener { -1 };
    uint16_t m_port { 0 };
    std::atomic<bool> m_stop { false };
    std::thread m_thread;
};

