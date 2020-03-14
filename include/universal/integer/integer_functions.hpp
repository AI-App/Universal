#pragma once
// integer_functions.hpp: definition of helper functions for integer type
//
// Copyright (C) 2017-2019 Stillwater Supercomputing, Inc.
//
// This file is part of the universal numbers project, which is released under an MIT Open Source license.
#include "./integer_exceptions.hpp"

#if defined(__clang__)
/* Clang/LLVM. ---------------------------------------------- */


#elif defined(__ICC) || defined(__INTEL_COMPILER)
/* Intel ICC/ICPC. ------------------------------------------ */


#elif defined(__GNUC__) || defined(__GNUG__)
/* GNU GCC/G++. --------------------------------------------- */


#elif defined(__HP_cc) || defined(__HP_aCC)
/* Hewlett-Packard C/aC++. ---------------------------------- */

#elif defined(__IBMC__) || defined(__IBMCPP__)
/* IBM XL C/C++. -------------------------------------------- */

#elif defined(_MSC_VER)
/* Microsoft Visual Studio. --------------------------------- */


#elif defined(__PGI)
/* Portland Group PGCC/PGCPP. ------------------------------- */

#elif defined(__SUNPRO_C) || defined(__SUNPRO_CC)
/* Oracle Solaris Studio. ----------------------------------- */

#endif

namespace sw {
namespace unum {

// calculate the greatest common divisor
template<size_t nbits, typename BlockType>
integer<nbits, BlockType> gcd(const integer<nbits, BlockType>& a, const integer<nbits, BlockType>& b) {
	return b.iszero() ? a : gcd(b, a % b);
}

// calculate the integer power a ^ b
// exponentiation by squaring is the standard method for modular exponentiation of large numbers in asymmetric cryptography
template<size_t nbits>
integer<nbits> ipow(const integer<nbits>& a, const integer<nbits>& b) {
	integer<nbits> result(1), base(a), exp(b);
	for (;;) {
		if (exp.isodd()) result *= base;
		exp >>= 1;
		if (exp == 0) break;
		base *= base;
	}
	return result;
}

} // namespace unum
} // namespace sw
