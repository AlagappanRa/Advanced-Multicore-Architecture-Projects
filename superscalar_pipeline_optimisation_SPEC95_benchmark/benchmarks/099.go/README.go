
SPEC BENCHMARK VERSION OF MANY FACES OF GO

This is a special version of the Go program, The Many Faces of Go, developed
for use as a part of the SPEC benchmark suite.  I have removed all of the
user interface code, the PC specific optimizations, the debugging and
range checking code, and all code protected by #ifdefs.  I've added function
prototypes for all functions and the code compiles without warnings on
both the microsoft C and HP C compilers.  It is strictly ANSI C compliant,
and has no dependencies on endianness.

This go playing engine is from the 1989 version of Many Faces of Go.  The
latest version is a much stronger go player.

After compilation, the executable is self contained.  There are no input files
required.  It is set up to play a single game of Go against itself, with
the game record sent to stdout.  The correct result is included in the
file "specout.go".

Optionally you may specify an input file on the command line "go file".  The
input file format is the same as the output format (a list of moves), so
you can specify an initial position.  For example, "specin.go" is an
input file that includes the first few moves from "specout.go".  The output
of "go specin.go" should also be identical to specout.go.

This program is a compute bound integer benchmark.  It uses a very small amount
of floating point only during initialization to set up an array of scaled
integers.  It uses almost no divides, and few multiplies.  Most data is
stored in single dimensioned arrays specifically to avoid multiplies.

This program has been extensively optimized using gprof to tune for maximum
performance.  Some inner loops have been unrolled in C.  It features many
small loops and lots of control flow (if/then/else).  Its profile is pretty
flat - it doesn't spend much time in any one place.  Commonly used low level
list manipulation routines are in a separate source, so function inlining
would be difficult.  It does not allocate memory.

This program runs in about 40 minutes on a 25 Mhz 486, and about 4 minutes on 
an HP9000/750, long enough to be a good
benchmark for today's fast RISC processors.  The running time is controlled
by the "fixplaylevel(40);" statement in initinit() in g2.c.  I recommend
leaving the level at 40 since that gives a good profile.  If you must change
it, it can range from 1 to 100, with higher numbers being slower.

It should be quite portable since it doesn't depend on any unusual runtime
library routines, or on endianness.  The only time I saw it run differently
on different processors was due to floating point rounding differences in
the initialization of rtval1[] in initrtval() in g23.c.  I modified the
initialization slightly, and this should not be a problem any more, but if
a port to a non-IEEE FP machine gives a different result, look here first.

g2jlib2.c contains a very large initializer, which might give some compilers
problems.

I retain the copyright to this code, but grant permission to Spec to
use or sell it for the the purpose of benchmarking.  I do not grant anyone
permission to use any part of this code in any Go playing program, or to
add any kind of user interface to this code that allows a person to play
against the computer.

-David Fotland

Any comments or questions:  fotland@cup.hp.com, (408)447-6242 or (408)985-1236

Many Faces of Go, with a user interface, is available for the IBM-PC from
Ishi Press, 76 Bonaventura Ave, San Jose CA 95134, (408) 944-9900, and for 
PenPoint from PenGames, 4863 Capistrano Ave San Jose, CA 95129 (408)985-1236.

Approximate times (without any special compiler option tuning):

486-25 PC 	about 40 minutes
HP9000/755	 2 minutes 51 seconds
HP9000/750 	 4 minutes 50 seconds
HP9000/400	43 minutes 35 seconds

