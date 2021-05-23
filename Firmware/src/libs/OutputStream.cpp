#include "OutputStream.h"

#include <cstdarg>
#include <cstring>
#include <stdio.h>
#include <stdlib.h>

#include "FreeRTOS.h"
#include "semphr.h"

#include "xformatc.h"

OutputStream::OutputStream(wrfnc f) : deleteos(true)
{
	clear_flags();
	stop_request= false;
	// create an output stream using the given write fnc
	fdbuf = new FdBuf(this, f);
	os = new std::ostream(fdbuf);
	*os << std::unitbuf; // auto flush on every write
	xWriteMutex = xSemaphoreCreateMutex();
}

OutputStream::~OutputStream()
{
	if(deleteos)
		delete os;
	if(fdbuf)
		delete fdbuf;
	if(xWriteMutex != nullptr)
		vSemaphoreDelete(xWriteMutex);
};

int OutputStream::flush_prepend()
{
	int n = prepending.size();
	if(n > 0) {
		prepend_ok = false;
		this->write(prepending.c_str(), n);
		prepending.clear();
	}
	return n;
}

// this needs to be protected by a semaphore as it could be preempted by another
// task which can write as well
int OutputStream::write(const char *buffer, size_t size)
{
	if(os == nullptr || closed) return 0;
	if(xWriteMutex != nullptr)
		xSemaphoreTake(xWriteMutex, portMAX_DELAY);
	if(prepend_ok) {
		prepending.append(buffer, size);
	} else {
		// this is expected to always write everything out
		os->write(buffer, size);
	}
	if(xWriteMutex != nullptr)
		xSemaphoreGive(xWriteMutex);
	return size;
}

int OutputStream::puts(const char *str)
{
	if(os == nullptr) return 0;
	size_t n = strlen(str);
	return this->write(str, n);
}

// used by printf only,and must be protected by xWriteMutex
void OutputStream::outchar(void *arg, char c)
{
    OutputStream *o= static_cast<OutputStream*>(arg);
    if(o->prepend_ok) {
        o->prepending.append(1, c);
    } else {
        o->os->write(&c, 1);
    }
}

int OutputStream::printf(const char *format, ...)
{
	if(os == nullptr || closed) return 0;

    if(xWriteMutex != nullptr)
        xSemaphoreTake(xWriteMutex, portMAX_DELAY);

    *os << std::nounitbuf; // no auto flush on every write

    va_list list;

    va_start(list, format);
    unsigned count = xvformat(outchar, this, format, list);
    va_end(list);

    *os << std::flush;
    *os << std::unitbuf; // auto flush on every write

    if(xWriteMutex != nullptr)
        xSemaphoreGive(xWriteMutex);

    return count;
}

int OutputStream::FdBuf::sync()
{
	int ret= 0;
	size_t len= this->str().size();
	if(!parent->closed && len > 0) {
		// fnc is expected to write everything
		size_t n = fnc(this->str().data(), len);
		if(n != len) {
			::printf("OutputStream error: write fnc failed\n");
			parent->set_closed();
			ret= -1;
		}
		this->str("");
	}

	return ret;
}
