#include "LineEditor.h"
#include "OutputStream.h"

#include <cstring>
#include <string>

// Maximum history lines
constexpr size_t HISTORY_MAX = 16;

LineEditor::LineEditor(size_t max, OutputStream *tos)
{
    maxsize = max;
    this->os = tos;
    pos = 0;
    state = NEXT_CHAR;
}

LineEditor::~LineEditor() {}

size_t LineEditor::get_line(char *rbuf, size_t sz)
{
    size_t n = buf.size();
    if(n > sz - 1) n = sz - 1;
    memcpy(rbuf, buf.data(), n);
    rbuf[n] = '\n';
    return n + 1;
}

void LineEditor::putch(char c)
{
    os->write(&c, 1);
}

void LineEditor::putstr(const char* s)
{
    while (*s) putch(*s++);
}

void LineEditor::initial_add(char *rbuf, size_t len)
{
    buf.assign(rbuf, len);
    pos = len;
}

bool LineEditor::add(char *rbuf, size_t len)
{
    if(buf.size() >= maxsize) return false;

    for (size_t i = 0; i < len; ++i) {
        char c = rbuf[i];
        if(add(c)) {
            return true;
        }
    }

    return false;
}

bool LineEditor::add(char c)
{
    switch(state) {
        case RESET:
            state = NEXT_CHAR;
            buf.clear();
            pos = 0;
            hist_index = history.size(); // start after last history

        case NEXT_CHAR:
            if (c == '\n') {
                putch('\n');
                state = RESET;
                if (!buf.empty()) add_history(buf);
                return true;
            }

            if (c == 0x1B) {    // ESC
                state = ESC_SEQ1;
                return false;
            }

            if (c == 0x7F || c == 0x08) { // DEL or BS
                if (pos > 0) {
                    pos--;
                    buf.erase(buf.begin() + pos);
                    putch('\b');
                    for (size_t i = pos; i < buf.size(); ++i)
                        putch(buf[i]);
                    putch(' ');
                    for (size_t i = pos; i <= buf.size(); ++i)
                        putch('\b');
                }

            } else if (c >= 32 && c < 127) {
                buf.insert(buf.begin() + pos, c);

                // print new + rest of string
                putch(c);
                for (size_t i = pos + 1; i < buf.size(); ++i)
                    putch(buf[i]);

                // move cursor back to where it should be
                for (size_t i = pos + 1; i < buf.size(); ++i)
                    putch('\b');

                pos++;
            }
            state = NEXT_CHAR;
            return false;

        case ESC_SEQ1:
            if (c == '[') {
                state = ESC_SEQ2;
            } else {
                state = NEXT_CHAR;
            }
            return false;

        case ESC_SEQ2:
            switch (c) {
                case 'D': // Left
                    if (pos > 0) { putch('\b'); pos--; }
                    break;
                case 'C': // Right
                    if (pos < buf.size()) { putch(buf[pos]); pos++; }
                    break;
                case 'H': // Home
                    while (pos > 0) { putch('\b'); pos--; }
                    break;
                case 'F': // End
                    while (pos < buf.size()) { putch(buf[pos]); pos++; }
                    break;
                case '3': // Delete key: ESC [ 3 ~
                    state = ESC_SEQ3;
                    return false;
                case 'A': // Up (history)
                    if (!history.empty() && hist_index > 0) {
                        clear_line();
                        hist_index--;
                        buf = history[hist_index];
                        pos = buf.size();
                        putstr(buf.c_str());
                    }
                    break;
                case 'B': // Down (history)
                    if (!history.empty() && hist_index < history.size() - 1) {
                        clear_line();
                        hist_index++;
                        buf = history[hist_index];
                        pos = buf.size();
                        putstr(buf.c_str());
                    } else if (hist_index == history.size() - 1) {
                        clear_line();
                        hist_index++;
                        buf.clear();
                        pos = 0;
                    }
                    break;
            }
            state =  NEXT_CHAR;
            return false;

        case ESC_SEQ3:
            if (c == '~' && pos < buf.size()) {
                buf.erase(buf.begin() + pos);

                // redraw remainder
                for (size_t i = pos; i < buf.size(); ++i)
                    putch(buf[i]);
                putch(' ');  // erase last leftover char
                // move cursor back
                for (size_t i = pos; i <= buf.size(); ++i)
                    putch('\b');
            }
            state = NEXT_CHAR;
            return false;
    }

    return false;
}

void LineEditor::add_history(const std::string& line)
{
    // find if duplicate and move to end if so
    for (auto i = history.begin(); i != history.end(); ++i) {
        if(*i == line) {
            // it is a duplicate, so move the dup to the current end of the list
            auto tmp = *i;
            history.erase(i);
            history.push_back(tmp);
            return;
        }
    }
    if (history.size() >= HISTORY_MAX)
        history.erase(history.begin());
    history.push_back(line);
}

void LineEditor::clear_line()
{
    // move cursor to start
    while (pos > 0) { putch('\b'); pos--; }
    // clear line
    for (size_t i = 0; i < buf.size(); ++i) putch(' ');
    for (size_t i = 0; i < buf.size(); ++i) putch('\b');
}
