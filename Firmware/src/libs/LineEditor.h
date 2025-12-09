#pragma once

#include <string>

class OutputStream;

class LineEditor
{
public:
    LineEditor(size_t maxsize, OutputStream& os);
    virtual ~LineEditor();
    void initial_add(char *buf, size_t len);
    bool add(char *buf, size_t len);
    size_t get_line(char *buf, size_t len);

private:
    void putch(char c);
    bool add(char c);

    size_t maxsize;
    std::string buf;
    OutputStream *os;
    size_t pos;
    enum STATES {
        NEXT_CHAR,
        ESC_SEQ1,
        ESC_SEQ2,
        ESC_SEQ3,
        RESET
    };
    STATES state;
};
