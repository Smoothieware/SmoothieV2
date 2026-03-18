# This is a guide on how to develop for Smoothie V2

## architecture

There are several threads running in the system.

* One thread for each input device (ACM/CDC, UART, Network), if there are
  two USB ports specified in the config then there will be a thread for
  each
* The main command thread which is where all gcodes and comamnds
  run
* A timer thread that can run tasks regularly, but cannot block execution
* Various other threads depending on what options have been
  enabled (for instance the player module runs a thread which reads gcode
  from the sdcard and sends it to the command thread to be executed)

All commands must be run from the command thread, which gets its command lines
from a message queue which the other threads push lines onto. Checks are made
to make sure you don't accidently try to execute a command from a different
thread, although this cannot be relied on.

## adding commands and/or gcodes

## running things in threads and feeding the command thread

The player module is a good example of how to get gcodes from an sdcard file
and feed them intot the command thread. Basicallt the line is pushed onto the
message queue, although there are some quirks qhere you need to make sure you
don't saturate that queue.

Basically any thread other than the command thread must put things on the
message queue to get them executed.

However if a command needs to run a gcode it can call dispatch directly as it
is already in the command thread, however this cannot handle a lot of lines,
maybe one or two gcodes, becuase stalling the command thread means nothing
else will get done, and all other ports will be blocked. Examples here are
the test command in the CommandShell, although some tests will stall the
command thread, and probably shouldn't. Basically if enough gcodes are issued
to fill the block queue, then the command thread will stall waiting for room.
It is a good idea to not issue more gcodes than the size of the block queue.

If a module must do so then it should be run in a thread and should issue the
gcodes in a controlled way as in the player thread. An alternative, as
mentioned later, is to run in the comms thread for that console, basically
simulating a stream of gcodes from a host, then the console gets stalled but
that is ok. Another alternative is to schedule the issuing of gcodes from a
slowtimer, one or two per cycle.

There is also a callback `in_command_ctx` that can be requested (by setting
`want_command_ctx = true` ) that is called when the command thread is idle.
This allows a thread to send commands direct to the command thread in that
context.

## getting input for a command

## things to avoid

* blocking or stalling the command thread
* blocking the timer threads

An example of a badly implemented command is the drillingcycles module which
was ported from V1. It violates several of the above advice. The peck cycle
could sit in the peck loop issuing gcodes that fill up the block queue, then
blocking the command thread, which means nothing would get done from any
comms thread as there is only one command thread. Although this is not fatal
as eventually the block queue would clear and the peck cycle would end, it is
however undesirable.
