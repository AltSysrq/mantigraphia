This directory contains the code for Mantigraphia's dialect of Lua 5.2.3, Llua
(pronounced "Yua").

It differs from vanilla Lua in the following ways:

- Numbers are 64-bit integers. Hence the "Ll" in "Llua".

- Division is necessarily integer division. Division by zero is zero. Dividing
  the most negative number by -1 does nothing.

- Modulo is unsigned; eg, `-1 % 3` is 2. Modulo by zero or negative numbers is
  zero.

- Exponentiation of course acts slightly differently. Anything to a negative
  power is zero. It otherwise behaves as one might expect, except for integer
  overflow.

- Common compound assignments are supported (+=, -=, *=, /=, postfix ++).

- Strings cannot be implicitly converted to numbers, nor vice-versa, in many
  cases.

- The math, io, and os parts of the standard library are gone.

The build produces an executable at `src/llua` which runs this dialect in
standalone format.
