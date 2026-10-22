# Lathe Mode

The best setup for basic lathe g codes is a simple index pulse once every
revolution, this can be done with a magnetic sensor or opticla sensor on the
head. However if ELS functionality is required that works somehwat like the
Clough47 ELS then an optical encoder with at least 1000PPR is required
(an index pulse is also usefull and the Omron encoders provide both).

(This writeup presumes the lathe has been fitted with a rotary encoder hooked
 up to the spindle, generally a 1024 to 2000 PPR resolution encoder is
 required, and/or an index pulse which triggers once per revolution).

This module adds the G33 Knnn gcode which will move the lathe Z axis in sync
with the spindle/chuck, the K parameter is the mm per revolution the Z axis
should move. Standard operation will also require a Znnn parameter which will
specify how far the Z axis should move in sync with the spindle. Optionally
one can issue (a non-standard) G33.1 and omit the Z parameter, in this case
the Z axis will move until told to stop (issuing a control Y will do this).

If an index pin has been defined then the Z will start moving when the index
pulse is received, this means that the leadscrew can be resynchronized with
the spindle even after they have moved independently.

*NOTE* Only the index pulse is needed for G33 and no encoder is used, in this
 case G33 will move in sync to the spindle by calculating the RPM based on
 the index pulse, then calculate the feed rate required to meet the mm per
 revolution K parameter. This is better for fast feed rates as the
 acceleration and deceleration is calculated as with a normal G0/G1 move.

 With G33.1 an encoder must be installed, and the carriage is moved in exact
 step with the pulses from the encoder, the downside of this is that there is
 no acceleration or deceleration and so should only be used for very slow
 feed rates. the upside is that the spindle can be turned by hand and the
 carriage will move in lock step, this is more like how some ELS's are
 implemented.

There is an additional ELS module which can display RPM and other data to a TM1638 display, and can also setup some of the "Electronic Lead screw" type functions found on many Opensource projects like Clough42. (Work in Progress).

## Threading modes
There are several ways to do threading with the Lathe module depending on how the lathe is setup. Note that all require that the lead screw is engaged at all times.

*NOTE* that the cutting tool should start far enough off the work piece to
 allow it to accelerate upto full speed before engaging the work.

1. Fully automated. If the X axis is also controlled by a stepper motor and the encoder has an index pulse then a threading cycle can be used which is a stream of gcode that positions the X and Z to a start position, issues a synchronous G33 Knnn Znnn which will cut the initial thread of nnn length, then stop, issue a G0 Xnnn command to move the tool away, issue a G0 Znnn to move back to the start position (or slightly further then back to take up backlash), issue G0 Xnnn to move the cutter back in, then G33 again etc until the thread is cut. This requires an index pulse so that the spindle and leadscrew can be resynchronized between cuts.

2. Semi automated. If the X axis is manual but there is an index pulse, then a similar cycle can be used as above, but the X axis needs to be manually set each cycle. Typically you would issue `G33 K0.5 Z-10` then when it stops retract X, issue `G0 Z0` which should go back to the start, then wind in X the required amount and repeat.

3. Manual. If the encoder has no index pulse then the lead screw needs to be moved via the spindle rotation otherwise it will get out of sync. In this case you would position the carriage at an initial known Z position. the X would be set to the required cutting depth, and G33.1 Knnn is issued, the user then needs to manually stop the spindle when the length of thread is cut, this will stop the Z as well, then the X is manually wound out, then spindle is started up in reverse to get the cross slide back to the starting position and the spindle is stopped, then X is wound in to the new depth, and the spindle started up in the forward direction. This method never disengages the lead screw and the spindle is used to move the carriage back and forward so never loses sync with the lead screw.
