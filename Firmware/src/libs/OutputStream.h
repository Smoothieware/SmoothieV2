#pragma once

#include <string>
#include <ostream>
#include <sstream>
#include <unistd.h>
#include <functional>

/**
	Handles an output stream from gcode/mcode handlers
	can be told to append a NL at end, and also to prepend or postpend the ok
*/
class OutputStream
{
public:
	using wrfnc = std::function<size_t(const char *buffer, size_t size)>;
	// create a null output stream
	OutputStream() : xWriteMutex(nullptr), os(nullptr), fdbuf(nullptr), deleteos(false) { usb_flag= closed= uploading= false; clear_flags(); };
	// create from an existing ostream
	OutputStream(std::ostream *o) : xWriteMutex(nullptr), os(o), fdbuf(nullptr), deleteos(false) { usb_flag= closed= uploading= false; clear_flags(); };
	// create using a supplied write fnc
	OutputStream(wrfnc f);

	virtual ~OutputStream();

	void reset() { clear_flags(); prepending.clear(); if(fdbuf != nullptr) fdbuf->str(""); }
	int write(const char *buffer, size_t size);
	int printf(const char *format, ...);
	int puts(const char *str);
	void set_append_nl(bool flg = true) { append_nl = flg; }
	void set_prepend_ok(bool flg = true) { prepend_ok = flg; }
	void set_no_response(bool flg = true) { no_response = flg; }
	bool is_append_nl() const { return append_nl; }
	bool is_prepend_ok() const { return prepend_ok; }
	bool is_no_response() const { return no_response; }
	int flush_prepend();
	void clear_flags() { append_nl= prepend_ok= no_response= done= false; }
	void set_closed(bool flg=true) { closed= flg; }
	bool is_closed() const { return closed; }
	void set_done() { done= true; }
	bool is_done() const { return done; }
    void set_uploading(bool flg) { uploading= flg; }
    bool is_uploading() const { return uploading; }
    void set_stop_request(bool flg) { stop_request= flg; }
    bool get_stop_request() const { return stop_request; }
    bool is_usb() const { return usb_flag; }
    void set_is_usb() { usb_flag = true; }

    std::function<void(char)> capture_fnc;
    std::function<bool(char*, size_t)> fast_capture_fnc;

private:
    static void outchar(void *, char c);

	// Hack to allow us to create a ostream writing to a supplied write function
	class FdBuf : public std::stringbuf
	{
	public:
		FdBuf(OutputStream *p, wrfnc f) : parent(p), fnc(f) {};
		virtual int sync();
	private:
		OutputStream *parent;
		wrfnc fnc;
	};
	void *xWriteMutex;
	std::ostream *os;
	FdBuf *fdbuf;
	std::string prepending;

	struct {
    	bool closed:1;
    	bool uploading:1;
		bool append_nl: 1;
		bool prepend_ok: 1;
		bool deleteos: 1;
		bool no_response: 1;
		bool done:1;
		bool stop_request:1;
        bool usb_flag:1;
	};
};
