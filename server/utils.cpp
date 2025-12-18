#include "utils.h"

#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <cstring>
#include <ctime>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>



std::string recv_line(int fd)
{
    std::string out;
    out.reserve(256);

    char c;
    while (true) {
        ssize_t n = ::read(fd, &c, 1);
        if (n <= 0) break;          // erro ou EOF
        out.push_back(c);
        if (c == '\n') break;       // fim da linha
    }
    return out;
}

bool read_exact(int fd, void *buf, std::size_t n)
{
    std::size_t done = 0;
    char *p = static_cast<char*>(buf);

    while (done < n) {
        ssize_t r = ::read(fd, p + done, n - done);
        if (r <= 0) return false;   // erro ou EOF
        done += static_cast<std::size_t>(r);
    }
    return true;
}

bool write_exact(int fd, const void *buf, std::size_t n)
{
    std::size_t done = 0;
    const char *p = static_cast<const char*>(buf);

    while (done < n) {
        ssize_t r = ::write(fd, p + done, n - done);
        if (r <= 0) return false;   // erro ou EOF
        done += static_cast<std::size_t>(r);
    }
    return true;
}


bool file_exists(const std::string &path) {
    struct stat st{};
    return ::stat(path.c_str(), &st) == 0 && S_ISREG(st.st_mode);
}


bool read_first_line(const std::string &path, std::string &line_out) {
    std::ifstream in(path);
    if (!in.is_open()) {
        return false;
    }
    std::getline(in, line_out);
    return in.good() || !line_out.empty();
}