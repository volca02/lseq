#pragma once

class Exception : public std::exception {
public:
    Exception(const std::string &msg) : msg(msg) {}
    virtual const char *what() const throw() { return msg.c_str(); }

protected:
    std::string msg;
};
