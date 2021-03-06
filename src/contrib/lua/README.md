This directory contains the code for Mantigraphia's dialect of Lua 5.2.3, Llua
(pronounced "Yua").

It differs from vanilla Lua in the following ways:

- Numbers are 64-bit integers. Hence the "Ll" in "Llua".

- Floating-point syntax can be used to specify coordinates in metres. Eg,
  `2.5 == 0x28000 == 163840` (2.5 metres in Mantigraphia coordinates).

- Division is necessarily integer division. Division by zero is zero. Dividing
  the most negative number by -1 does nothing.

- Modulo is unsigned; eg, `-1 % 3` is 2. Modulo by zero or negative numbers is
  zero.

- Exponentiation of course acts slightly differently. Anything to a negative
  power is zero. It otherwise behaves as one might expect, except for integer
  overflow.

- Common compound assignments are supported (+=, -=, *=, /=, postfix ++).

- The math, io, and os parts of the standard library are gone.

- The interpreter has a fundamental limit as to how many instructions it will
  execute until giving up.

The build produces an executable at `src/llua` which runs this dialect in
standalone format.
