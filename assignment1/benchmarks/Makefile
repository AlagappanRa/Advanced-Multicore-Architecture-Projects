
test:
	/usr/bin/time -p sim-safe cc1.ss -O 1stmt.i 2> sim-safe-gcc.time
	diff 1stmt.s 1stmt.s.ref
	/usr/bin/time -p sim-safe compress95.ss < compress95.in > compress95.out 2> sim-safe-compress.time
	diff compress95.out compress95.out.ref
	/usr/bin/time -p sim-safe go.ss 50 9 2stone9.in > go.out 2> sim-safe-go.time
	diff go.out go.out.ref
	/usr/bin/time -p sim-profile cc1.ss -O 1stmt.i 2> sim-profile-gcc.time
	diff 1stmt.s 1stmt.s.ref
	/usr/bin/time -p sim-profile compress95.ss < compress95.in > compress95.out 2> sim-profile-compress.time
	diff compress95.out compress95.out.ref
	/usr/bin/time -p sim-profile go.ss 50 9 2stone9.in > go.out 2> sim-profile-go.time
	diff go.out go.out.ref
	/usr/bin/time -p sim-outorder cc1.ss -O 1stmt.i 2> sim-outorder-gcc.time
	diff 1stmt.s 1stmt.s.ref
	/usr/bin/time -p sim-outorder compress95.ss < compress95.in > compress95.out 2> sim-outorder-compress.time
	diff compress95.out compress95.out.ref
	/usr/bin/time -p sim-outorder go.ss 50 9 2stone9.in > go.out 2> sim-outorder-go.time
	diff go.out go.out.ref
