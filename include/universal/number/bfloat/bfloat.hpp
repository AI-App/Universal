#pragma once
// bfloat.hpp: definition of an arbitrary configuration linear floating-point representation
//
// Copyright (C) 2017-2021 Stillwater Supercomputing, Inc.
//
// This file is part of the universal numbers project, which is released under an MIT Open Source license.
#include <cassert>
#include <limits>

#include <universal/native/ieee754.hpp>
#include <universal/native/bit_functions.hpp>
#include <universal/internal/blockbinary/blockbinary.hpp>
#include <universal/number/bfloat/exceptions.hpp>

// compiler specific operators
#if defined(__clang__)
/* Clang/LLVM. ---------------------------------------------- */
#define BIT_CAST_SUPPORT 0
#define CONSTEXPRESSION 

#elif defined(__ICC) || defined(__INTEL_COMPILER)
/* Intel ICC/ICPC. ------------------------------------------ */

#elif defined(__GNUC__) || defined(__GNUG__)
/* GNU GCC/G++. --------------------------------------------- */
#define BIT_CAST_SUPPORT 0
#define CONSTEXPRESSION 

#elif defined(__HP_cc) || defined(__HP_aCC)
/* Hewlett-Packard C/aC++. ---------------------------------- */

#elif defined(__IBMC__) || defined(__IBMCPP__)
/* IBM XL C/C++. -------------------------------------------- */

#elif defined(_MSC_VER)
/* Microsoft Visual Studio. --------------------------------- */
//#pragma warning(disable : 4310)  // cast truncates constant value

#define BIT_CAST_SUPPORT 1
#define CONSTEXPRESSION constexpr
#include <bit>

#elif defined(__PGI)
/* Portland Group PGCC/PGCPP. ------------------------------- */

#elif defined(__SUNPRO_C) || defined(__SUNPRO_CC)
/* Oracle Solaris Studio. ----------------------------------- */

#endif

#ifndef THROW_ARITHMETIC_EXCEPTION
#define THROW_ARITHMETIC_EXCEPTION 0
#endif
#ifndef TRACE_CONVERSION
#define TRACE_CONVERSION 0
#endif

namespace sw::universal {
		
	static constexpr double oneOver2p6 = 0.015625;
	static constexpr double oneOver2p14 = 0.00006103515625;
	static constexpr double oneOver2p30 = 1.0 / 1073741824.0;
	static constexpr double oneOver2p50 = 1.0 / 1125899906842624.0;
	static constexpr double oneOver2p62 = 1.0 / 4611686018427387904.0;
	static constexpr double oneOver2p126 = oneOver2p62 * oneOver2p62 * 0.25;
	static constexpr double oneOver2p254 = oneOver2p126 * oneOver2p126 * 0.25;
	static constexpr double oneOver2p510 = oneOver2p254 * oneOver2p254 * 0.25;
	static constexpr double oneOver2p1022 = oneOver2p510 * oneOver2p510 * 0.25;

// precomputed values for subnormal exponents as a function of es
	static constexpr int subnormal_reciprocal_shift[] = {
		0,                    // es = 0 : not a valid value
		-1,                   // es = 1 : 2^(2 - 2^(es-1)) = 2^1
		0,                    // es = 2 : 2^(2 - 2^(es-1)) = 2^0
		2,                    // es = 3 : 2^(2 - 2^(es-1)) = 2^-2
		6,                    // es = 4 : 2^(2 - 2^(es-1)) = 2^-6
		14,                   // es = 5 : 2^(2 - 2^(es-1)) = 2^-14
		30,                   // es = 6 : 2^(2 - 2^(es-1)) = 2^-30
		62,                   // es = 7 : 2^(2 - 2^(es-1)) = 2^-62
		126,                  // es = 8 : 2^(2 - 2^(es-1)) = 2^-126
		254,                  // es = 9 : 2^(2 - 2^(es-1)) = 2^-254
		510,                  // es = 10 : 2^(2 - 2^(es-1)) = 2^-510
		1022                  // es = 11 : 2^(2 - 2^(es-1)) = 2^-1022
	};
// es > 11 requires a long double representation, which MSVC does not provide.
	static constexpr double subnormal_exponent[] = {
		0,                    // es = 0 : not a valid value
		2.0,                  // es = 1 : 2^(2 - 2^(es-1)) = 2^1
		1.0,                  // es = 2 : 2^(2 - 2^(es-1)) = 2^0
		0.25,                 // es = 3 : 2^(2 - 2^(es-1)) = 2^-2
		oneOver2p6,           // es = 4 : 2^(2 - 2^(es-1)) = 2^-6
		oneOver2p14,          // es = 5 : 2^(2 - 2^(es-1)) = 2^-14
		oneOver2p30,          // es = 6 : 2^(2 - 2^(es-1)) = 2^-30
		oneOver2p62,          // es = 7 : 2^(2 - 2^(es-1)) = 2^-62
		oneOver2p126,         // es = 8 : 2^(2 - 2^(es-1)) = 2^-126
		oneOver2p254,         // es = 9 : 2^(2 - 2^(es-1)) = 2^-254
		oneOver2p510,         // es = 10 : 2^(2 - 2^(es-1)) = 2^-510
		oneOver2p1022         // es = 11 : 2^(2 - 2^(es-1)) = 2^-1022
	};

// Forward definitions
template<size_t nbits, size_t es, typename bt> class bfloat;
template<size_t nbits, size_t es, typename bt> bfloat<nbits,es,bt> abs(const bfloat<nbits,es,bt>&);
template<typename bt> inline std::string to_binary_storage(const bt&, bool);

static constexpr int NAN_TYPE_SIGNALLING = -1;   // a Signalling NaN
static constexpr int NAN_TYPE_EITHER     = 0;    // any NaN
static constexpr int NAN_TYPE_QUIET      = 1;    // a Quiet NaN

static constexpr int INF_TYPE_NEGATIVE   = -1;   // -inf
static constexpr int INF_TYPE_EITHER     = 0;    // any inf
static constexpr int INF_TYPE_POSITIVE   = 1;    // +inf

constexpr bool BFLOAT_NIBBLE_MARKER = true;


/// <summary>
/// decode an bfloat value into its constituent parts
/// </summary>
/// <typeparam name="bt"></typeparam>
/// <param name="v"></param>
/// <param name="s"></param>
/// <param name="e"></param>
/// <param name="f"></param>
template<size_t nbits, size_t es, size_t fbits, typename bt>
void decode(const bfloat<nbits, es, bt>& v, bool& s, blockbinary<es, bt>& e, blockbinary<fbits, bt>& f) {
	v.sign(s);
	v.exponent(e);
	v.fraction(f);
}

/// <summary>
/// return the binary scale of the given number
/// </summary>
/// <typeparam name="bt">Block type used for storage: derived through ADL</typeparam>
/// <param name="v">the bfloat number for which we seek to know the binary scale</param>
/// <returns>binary scale, i.e. 2^scale, of the value of the bfloat</returns>
template<size_t nbits, size_t es, typename bt>
int scale(const bfloat<nbits, es, bt>& v) {
	return v.scale();
}

/////////////////////////////////////////////////////////////////////////////////
/// free functions that can set an bfloat to extreme values in its state space
/// organized in descending order.

// fill an bfloat object with maximum positive value
template<size_t nbits, size_t es, typename bt>
bfloat<nbits, es, bt>& maxpos(bfloat<nbits, es, bt>& bmaxpos) {
	// maximum positive value has this bit pattern: 0-1...1-111...111, that is, sign = 0, e = 1.1, f = 111...100
	bmaxpos.clear();
	bmaxpos.flip();
	bmaxpos.reset(nbits - 1ull);
	bmaxpos.reset(0ull);
	bmaxpos.reset(1ull);
	return bmaxpos;
}
// fill an bfloat object with mininum positive value
template<size_t nbits, size_t es, typename bt>
bfloat<nbits, es, bt>& minpos(bfloat<nbits, es, bt>& bminpos) {
	// minimum positive value has this bit pattern: 0-000-00...010, that is, sign = 0, e = 00, f = 00001, u = 0
	bminpos.clear();
	bminpos.set(1);
	return bminpos;
}
// fill an bfloat object with the zero encoding: 0-0...0-00...000-0
template<size_t nbits, size_t es, typename bt>
bfloat<nbits, es, bt>& zero(bfloat<nbits, es, bt>& tobezero) {
	tobezero.clear();
	return tobezero;
}
// fill an bfloat object with smallest negative value
template<size_t nbits, size_t es, typename bt>
bfloat<nbits, es, bt>& minneg(bfloat<nbits, es, bt>& bminneg) {
	// minimum negative value has this bit pattern: 1-000-00...010, that is, sign = 1, e = 00, f = 00001, u = 0
	bminneg.clear();
	bminneg.set(nbits - 1ull);
	bminneg.set(1);
	return bminneg;
}
// fill an bfloat object with largest negative value
template<size_t nbits, size_t es, typename bt>
bfloat<nbits, es, bt>& maxneg(bfloat<nbits, es, bt>& bmaxneg) {
	// maximum negative value has this bit pattern: 1-1...1-111...110, that is, sign = 1, e = 1.1, f = 111...110, u = 0
	bmaxneg.clear();
	bmaxneg.flip();
	bmaxneg.reset(0ull);
	bmaxneg.reset(1ull);
	return bmaxneg;
}

/// <summary>
/// An arbitrary configuration real number with gradual under/overflow and uncertainty bit
/// </summary>
/// <typeparam name="nbits">number of bits in the encoding</typeparam>
/// <typeparam name="es">number of exponent bits in the encoding</typeparam>
/// <typeparam name="bt">the type to use as storage class: one of [uint8_t|uint16_t|uint32_t]</typeparam>
template<size_t _nbits, size_t _es, typename bt = uint8_t>
class bfloat {
public:
	static_assert(_nbits > _es + 1ull, "nbits is too small to accomodate the requested number of exponent bits");
	static_assert(_es < 2147483647ull, "my God that is a big number, are you trying to break the Interweb?");
	static_assert(_es > 0, "number of exponent bits must be bigger than 0 to be a floating point number");
	static constexpr size_t bitsInByte = 8ull;
	static constexpr size_t bitsInBlock = sizeof(bt) * bitsInByte;
	static_assert(bitsInBlock <= 64, "storage unit for block arithmetic needs to be <= uint64_t"); // TODO: carry propagation on uint64_t requires assembly code

	static constexpr size_t nbits = _nbits;
	static constexpr size_t es = _es;
	static constexpr size_t fbits  = nbits - 1ull - es;    // number of fraction bits excluding the hidden bit
	static constexpr size_t fhbits = fbits + 1ull;         // number of fraction bits including the hidden bit
	static constexpr size_t abits = fhbits + 3ull;         // size of the addend
	static constexpr size_t mbits = 2ull * fhbits;         // size of the multiplier output
	static constexpr size_t divbits = 3ull * fhbits + 4ull;// size of the divider output

	static constexpr size_t nrBlocks = 1ull + ((nbits - 1ull) / bitsInBlock);
	static constexpr size_t storageMask = (0xFFFFFFFFFFFFFFFFull >> (64ull - bitsInBlock));

	static constexpr size_t MSU = nrBlocks - 1ull; // MSU == Most Significant Unit, as MSB is already taken
	static constexpr bt ALL_ONES = bt(~0);
	static constexpr bt MSU_MASK = (ALL_ONES >> (nrBlocks * bitsInBlock - nbits));
	static constexpr size_t bitsInMSU = bitsInBlock - (nrBlocks * bitsInBlock - nbits);
	static constexpr bt SIGN_BIT_MASK = bt(bt(1ull) << ((nbits - 1ull) % bitsInBlock));
	static constexpr bt LSB_BIT_MASK = bt(1ull);
	static constexpr bool MSU_CAPTURES_E = (1ull + es) <= bitsInMSU;
	static constexpr size_t EXP_SHIFT = (MSU_CAPTURES_E ? (nbits - 1ull - es) : 0);
	static constexpr bt MSU_EXP_MASK = ((ALL_ONES << EXP_SHIFT) & ~SIGN_BIT_MASK) & MSU_MASK;
	static constexpr int EXP_BIAS = ((1l << (es - 1ull)) - 1l);
	static constexpr int MAX_EXP = (1l << es) - EXP_BIAS;
	static constexpr int MIN_EXP_NORMAL = 1 - EXP_BIAS;
	static constexpr int MIN_EXP_SUBNORMAL = 1 - EXP_BIAS - int(fbits); // the scale of smallest ULP
	static constexpr bt BLOCK_MASK = bt(-1);

	using BlockType = bt;

	// constructors
	constexpr bfloat() noexcept : _block{ 0 } {};

	constexpr bfloat(const bfloat&) noexcept = default;
	constexpr bfloat(bfloat&&) noexcept = default;

	constexpr bfloat& operator=(const bfloat&) noexcept = default;
	constexpr bfloat& operator=(bfloat&&) noexcept = default;

	// decorated/converting constructors

	/// <summary>
	/// construct an bfloat from another, block type bt must be the same
	/// </summary>
	/// <param name="rhs"></param>
	template<size_t nnbits, size_t ees>
	bfloat(const bfloat<nnbits, ees, bt>& rhs) {
		// this->assign(rhs);
	}

	/// <summary>
	/// construct an bfloat from a native type, specialized for size
	/// </summary>
	/// <param name="iv">initial value to construct</param>
	constexpr bfloat(signed char iv)        noexcept : _block{ 0 } { *this = iv; }
	constexpr bfloat(short iv)              noexcept : _block{ 0 } { *this = iv; }
	constexpr bfloat(int iv)                noexcept : _block{ 0 } { *this = iv; }
	constexpr bfloat(long iv)               noexcept : _block{ 0 } { *this = iv; }
	constexpr bfloat(long long iv)          noexcept : _block{ 0 } { *this = iv; }
	constexpr bfloat(char iv)               noexcept : _block{ 0 } { *this = iv; }
	constexpr bfloat(unsigned short iv)     noexcept : _block{ 0 } { *this = iv; }
	constexpr bfloat(unsigned int iv)       noexcept : _block{ 0 } { *this = iv; }
	constexpr bfloat(unsigned long iv)      noexcept : _block{ 0 } { *this = iv; }
	constexpr bfloat(unsigned long long iv) noexcept : _block{ 0 } { *this = iv; }
	constexpr bfloat(float iv)              noexcept : _block{ 0 } { *this = iv; }
	constexpr bfloat(double iv)             noexcept : _block{ 0 } { *this = iv; }
	constexpr bfloat(long double iv)        noexcept : _block{ 0 } { *this = iv; }

	// assignment operators
	constexpr bfloat& operator=(signed char rhs) { return convert_signed_integer(rhs); }
	constexpr bfloat& operator=(short rhs)       { return convert_signed_integer(rhs); }
	constexpr bfloat& operator=(int rhs)         { return convert_signed_integer(rhs); }
	constexpr bfloat& operator=(long rhs)        { return convert_signed_integer(rhs); }
	constexpr bfloat& operator=(long long rhs)   { return convert_signed_integer(rhs); }

	constexpr bfloat& operator=(char rhs)               { return convert_unsigned_integer(rhs); }
	constexpr bfloat& operator=(unsigned short rhs)     { return convert_unsigned_integer(rhs); }
	constexpr bfloat& operator=(unsigned int rhs)       { return convert_unsigned_integer(rhs); }
	constexpr bfloat& operator=(unsigned long rhs)      { return convert_unsigned_integer(rhs); }
	constexpr bfloat& operator=(unsigned long long rhs) { return convert_unsigned_integer(rhs); }

	template<typename Ty>
	constexpr bfloat& convert_unsigned_integer(const Ty& rhs) noexcept {
		clear();
		if (0 == rhs) return *this;
		uint64_t raw = static_cast<uint64_t>(rhs);
		int exponent = int(findMostSignificantBit(raw)) - 1; // precondition that msb > 0 is satisfied by the zero test above
		constexpr uint32_t sizeInBits = 8 * sizeof(Ty);
		uint32_t shift = sizeInBits - exponent - 1;
		raw <<= shift;
		raw = round<sizeInBits, uint64_t>(raw, exponent);
		return *this;
	}
	template<typename Ty>
	constexpr bfloat& convert_signed_integer(const Ty& rhs) noexcept {
		clear();
		if (0 == rhs) return *this;
		bool s = (rhs < 0);
		uint64_t raw = static_cast<uint64_t>(s ? -rhs : rhs);
		int exponent = int(findMostSignificantBit(raw)) - 1; // precondition that msb > 0 is satisfied by the zero test above
		constexpr uint32_t sizeInBits = 8 * sizeof(Ty);
		uint32_t shift = sizeInBits - exponent - 1;
		raw <<= shift;
		raw = round<sizeInBits, uint64_t>(raw, exponent);
#ifdef LATER
		// construct the target bfloat
		if constexpr (64 >= nbits - es - 1ull) {
			uint64_t bits = (s ? 1u : 0u);
			bits <<= es;
			bits |= exponent + EXP_BIAS;
			bits <<= nbits - 1ull - es;
			bits |= raw;
			bits &= 0xFFFF'FFFF;
			if constexpr (1 == nrBlocks) {
				_block[MSU] = bt(bits);
			}
			else {
				copyBits(bits);
			}
		}
		else {
			std::cerr << "TBD\n";
		}
#endif
		return *this;
	}


	CONSTEXPRESSION bfloat& operator=(float rhs) {
		clear();
#if BIT_CAST_SUPPORT
		// normal number
		uint32_t bc      = std::bit_cast<uint32_t>(rhs);
		bool s           = (0x8000'0000u & bc);
		uint32_t raw_exp = uint32_t((0x7F80'0000u & bc) >> 23u);
		uint32_t raw     = (0x007F'FFFFu & bc);
#else // !BIT_CAST_SUPPORT
		float_decoder decoder;
		decoder.f        = rhs;
		bool s           = decoder.parts.sign ? true : false;
		uint32_t raw_exp = decoder.parts.exponent;
		uint32_t raw     = decoder.parts.fraction;
#endif // !BIT_CAST_SUPPORT

		// special case handling
		if (raw_exp == 0xFFu) { // special cases
			if (raw == 1ul || raw == 0x0040'0001ul) {
				// 1.11111111.00000000000000000000001 signalling nan
				// 0.11111111.00000000000000000000001 signalling nan
				// MSVC
				// 1.11111111.10000000000000000000001 signalling nan
				// 0.11111111.10000000000000000000001 signalling nan
				setnan(NAN_TYPE_SIGNALLING);
				return *this;
			}
			if (raw == 0x0040'0000ul) {
				// 1.11111111.10000000000000000000000 quiet nan
				// 0.11111111.10000000000000000000000 quiet nan
				setnan(NAN_TYPE_QUIET);
				return *this;
			}
			if (raw == 0ul) {
				// 1.11111111.00000000000000000000000 -inf
				// 0.11111111.00000000000000000000000 +inf
				setinf(s);
				return *this;
			}
		}
		if (rhs == 0.0) { // IEEE rule: this is valid for + and - 0.0
			set(nbits - 1ull, s);
			return *this;
		}
		
		// this is not a special number
		int exponent = int(raw_exp) - 127;  // unbias the exponent

#if TRACE_CONVERSION
		std::cout << '\n';
		std::cout << "value           : " << rhs << '\n';
		std::cout << "segments        : " << to_binary(rhs) << '\n';
		std::cout << "sign     bit    : " << (s ? '1' : '0') << '\n';
		std::cout << "exponent bits   : " << to_binary_storage(uint8_t(raw_exp), true) << '\n';
		std::cout << "fraction bits   : " << to_binary_storage(raw, true) << std::endl;
		std::cout << "exponent value  : " << exponent << '\n';
#endif
		// saturate to minpos/maxpos if out of range
		if (exponent > MAX_EXP) {
			if (s) maxneg(*this); else maxpos(*this); // saturate the maxpos or maxneg
			this->set(0);
			return *this;
		}
		if (exponent < MIN_EXP_SUBNORMAL) {
			if (s) this->set(nbits - 1); // set -0
			this->set(0);
			return *this;
		}
		// set the exponent
		uint32_t biasedExponent{ 0 };
		int shiftRight = 23 - static_cast<int>(fbits); // this is the bit shift to get the MSB of the src to the MSB of the tgt
		int adjustment{ 0 };
		// we have 23 fraction bits and one hidden bit for a normal number, and no hidden bit for a subnormal
		// simpler rounding as compared to IEEE as uncertainty bit captures any non-zero bit past the LSB
		// ...  lsb | round guard sticky
		//       x      0          0
		//       x  |   1          1
		uint32_t mask = 0x007F'FFFFu >> fbits; // mask for sticky bit 
		if (exponent >= MIN_EXP_SUBNORMAL && exponent < MIN_EXP_NORMAL) {
			// this number is a subnormal number in this representation
			// but it might be a normal number in IEEE single precision (float) representation
			if (exponent > -127) {
				// the source real is a normal number, so we must add the hidden bit to the fraction bits
				raw |= (1ull << 23);
				mask = 0x00FF'FFFFu >> (fbits + exponent + subnormal_reciprocal_shift[es] + 1); // mask for sticky bit 
#if TRACE_CONVERSION
				std::cout << "fraction bits   : " << to_binary_storage(raw, true) << std::endl;
#endif
				// fraction processing: we have 24 bits = 1 hidden + 23 explicit fraction bits 
				// f = 1.ffff 2^exponent * 2^fbits * 2^-(2-2^(es-1)) = 1.ff...ff >> (23 - (-exponent + fbits - (2 -2^(es-1))))
				// -exponent because we are right shifting and exponent in this range is negative
				adjustment = -(exponent + subnormal_reciprocal_shift[es]); // this is the right shift adjustment due to the scale of the input number, i.e. the exponent of 2^-adjustment
				if (shiftRight > 0) {		// do we need to round?
					raw >>= shiftRight + adjustment;
				}
				else { // all bits of the float go into this representation and need to be shifted up
					std::cout << "conversion of IEEE float to more precise bfloats not implemented yet\n";
				}
			}
			else {
				// the source real is a subnormal number, and the target representation is a subnormal representation
				mask = 0x00FF'FFFFu >> (fbits + exponent + subnormal_reciprocal_shift[es] + 1); // mask for sticky bit 
#if TRACE_CONVERSION
				std::cout << "fraction bits   : " << to_binary_storage(raw, true) << std::endl;
#endif
				// fraction processing: we have 24 bits = 1 hidden + 23 explicit fraction bits 
				// f = 1.ffff 2^exponent * 2^fbits * 2^-(2-2^(es-1)) = 1.ff...ff >> (23 - (-exponent + fbits - (2 -2^(es-1))))
				// -exponent because we are right shifting and exponent in this range is negative
				adjustment = -(exponent + subnormal_reciprocal_shift[es]); // this is the right shift adjustment due to the scale of the input number, i.e. the exponent of 2^-adjustment
				if (shiftRight > 0) {		// do we need to round?
					raw >>= shiftRight + adjustment;
				}
				else { // all bits of the float go into this representation and need to be shifted up
					std::cout << "conversion of subnormal IEEE float to more precise bfloats not implemented yet\n";
				}
			}
		}
		else {
			// this number is a normal/supernormal number in this representation, we can leave the hidden bit hidden
			biasedExponent = static_cast<uint32_t>(exponent + EXP_BIAS); // reasonable to limit exponent to 32bits

			// fraction processing
			if (shiftRight > 0) {		// do we need to round?
				// we have 23 fraction bits and one hidden bit for a normal number, and no hidden bit for a subnormal
				// simpler rounding as uncertainty bit captures any non-zero bit past the LSB
				// ...  lsb | round guard sticky
				//       x      0          0
				//       x  |   1          1
				raw >>= shiftRight;
			}
			else { // all bits of the double go into this representation and need to be shifted up
				std::cout << "conversion of IEEE double to more precise bfloats not implemented yet\n";
			}
		}
#if TRACE_CONVERSION
		std::cout << "biased exponent : " << biasedExponent << " : 0x" << std::hex << biasedExponent << std::dec << '\n';
		std::cout << "shift           : " << shiftRight << '\n';
		std::cout << "adjustment shift: " << adjustment << '\n';
		std::cout << "sticky bit mask : " << to_binary_storage(mask, true) << '\n';
		std::cout << "fraction bits   : " << to_binary_storage(raw, true) << '\n';
#endif
		// construct the target bfloat
		uint32_t bits = (s ? 1u : 0u);
		bits <<= es;
		bits |= biasedExponent;
		bits <<= nbits - 1ull - es;
		bits |= raw;
		if constexpr (1 == nrBlocks) {
			_block[MSU] = bt(bits);
		}
		else {
			copyBits(bits);
		}
		return *this;
	}
	CONSTEXPRESSION bfloat& operator=(double rhs) {
		clear();
#if BIT_CAST_SUPPORT
		// normal number
		uint64_t bc      = std::bit_cast<uint64_t>(rhs);
		bool s           = (0x8000'0000'0000'0000ull & bc);
		uint32_t raw_exp = static_cast<uint32_t>((0x7FF0'0000'0000'0000ull & bc) >> 52);
		uint64_t raw     = (0x000F'FFFF'FFFF'FFFFull & bc);
#else // !BIT_CAST_SUPPORT
		double_decoder decoder;
		decoder.d        = rhs;
		bool s           = decoder.parts.sign ? true : false;
		uint32_t raw_exp = static_cast<uint32_t>(decoder.parts.exponent);
		uint64_t raw     = decoder.parts.fraction;
#endif // !BIT_CAST_SUPPORT
		if (raw_exp == 0x7FFul) { // special cases
			if (raw == 1ull) {
				// 1.11111111111.0000000000000000000000000000000000000000000000000001 signalling nan
				// 0.11111111111.0000000000000000000000000000000000000000000000000001 signalling nan
				setnan(NAN_TYPE_SIGNALLING);
				return *this;
			}
			if (raw == 0x0008'0000'0000'0000ull) {
				// 1.11111111111.1000000000000000000000000000000000000000000000000000 quiet nan
				// 0.11111111111.1000000000000000000000000000000000000000000000000000 quiet nan
				setnan(NAN_TYPE_QUIET);
				return *this;
			}
			if (raw == 0ull) {
				// 1.11111111111.0000000000000000000000000000000000000000000000000000 -inf
				// 0.11111111111.0000000000000000000000000000000000000000000000000000 +inf
				setinf(s);
				return *this;
			}
		}
		if (rhs == 0.0) { // IEEE rule: this is valid for + and - 0.0
			set(nbits - 1ull, s);
			return *this;
		}
		// this is not a special number
		int exponent = int(raw_exp) - 1023;  // unbias the exponent
#if TRACE_CONVERSION
		std::cout << '\n';
		std::cout << "value           : " << rhs << '\n';
		std::cout << "segments        : " << to_binary(rhs) << '\n';
		std::cout << "sign   bits     : " << (s ? '1' : '0') << '\n';
		std::cout << "exponent value  : " << exponent << '\n';
		std::cout << "fraction bits   : " << to_binary_storage(raw, true) << std::endl;
#endif
		// saturate to minpos/maxpos with uncertainty bit set to 1
		if (exponent > MAX_EXP) {	
			if (s) maxneg(*this); else maxpos(*this); // saturate the maxpos or maxneg
			this->set(0); // and set the uncertainty bit to reflect it is (maxpos, inf) or (maxneg, -inf)
			return *this;
		}
		if (exponent < MIN_EXP_SUBNORMAL) {
			if (s) this->set(nbits - 1); // set -0
			this->set(0); // and set the uncertainty bit to reflect (0,minpos) or (-0,minneg)
			return *this;
		}
		// set the exponent
		uint64_t biasedExponent{ 0 };
		int shiftRight = 52 - static_cast<int>(fbits); // this is the bit shift to get the MSB of the src to the MSB of the tgt
		int adjustment{ 0 };
		// we have 52 fraction bits and one hidden bit for a normal number, and no hidden bit for a subnormal
		// simpler rounding as compared to IEEE as uncertainty bit captures any non-zero bit past the LSB
		// ...  lsb | round guard sticky
		//       x      0          0
		//       x  |   1          1
		uint64_t mask;
		if (exponent >= MIN_EXP_SUBNORMAL && exponent < MIN_EXP_NORMAL) {
			// this number is a subnormal number in this representation
			// but it might be a normal number in IEEE double precision representation
			// which will require a reinterpretation of the bits as the hidden bit becomes explicit in a subnormal representation
			if (exponent > -1022) {
				mask = 0x001F'FFFF'FFFF'FFFF >> (fbits + exponent + subnormal_reciprocal_shift[es] + 1); // mask for sticky bit 
				// the source real is a normal number, so we must add the hidden bit to the fraction bits
				raw |= (1ull << 52);
#if TRACE_CONVERSION
				std::cout << "mask     bits   : " << to_binary_storage(mask, true) << std::endl;
				std::cout << "fraction bits   : " << to_binary_storage(raw, true) << std::endl;
#endif
				// fraction processing: we have 53 bits = 1 hidden + 52 explicit fraction bits 
				// f = 1.ffff 2^exponent * 2^fbits * 2^-(2-2^(es-1)) = 1.ff...ff >> (52 - (-exponent + fbits - (2 -2^(es-1))))
				// -exponent because we are right shifting and exponent in this range is negative
				adjustment = -(exponent + subnormal_reciprocal_shift[es]);
#if TRACE_CONVERSION
				std::cout << "exponent        : " << exponent << std::endl;
				std::cout << "bias shift      : " << subnormal_reciprocal_shift[es] << std::endl;
				std::cout << "adjustment      : " << adjustment << std::endl;
#endif
				if (shiftRight > 0) {		// do we need to round?
					raw >>= (shiftRight + adjustment);
				}
				else { // all bits of the double go into this representation and need to be shifted up
					std::cout << "conversion of IEEE double to more precise bfloats not implemented yet\n";
				}
			}
			else {
				// this is a subnormal double
				std::cout << "conversion of subnormal IEEE doubles not implemented yet\n";
			}
		}
		else {
			// this number is a normal/supernormal number in this representation, we can leave the hidden bit hidden
			biasedExponent = static_cast<uint64_t>(exponent + EXP_BIAS); // reasonable to limit exponent to 32bits

			// fraction processing
			mask = 0x000F'FFFF'FFFF'FFFF >> fbits; // mask for sticky bit 
			if (shiftRight > 0) {		// do we need to round?
				// we have 52 fraction bits and one hidden bit for a normal number, and no hidden bit for a subnormal
				// simpler rounding as uncertainty bit captures any non-zero bit past the LSB
				// ...  lsb | round guard sticky
				//       x      0          0
				//       x  |   1          1
				raw >>= shiftRight;
			}
			else { // all bits of the double go into this representation and need to be shifted up
				std::cout << "conversion of IEEE double to more precise bfloats not implemented yet\n";
			}
		}
#if TRACE_CONVERSION
		std::cout << "biased exponent : " << biasedExponent << " : " << std::hex << biasedExponent << std::dec << '\n';
		std::cout << "shift           : " << shiftRight << '\n';
		std::cout << "sticky bit mask : " << to_binary_storage(mask, true) << '\n';
		std::cout << "fraction bits   : " << to_binary_storage(raw, true) << '\n';
#endif
		// construct the target bfloat
		uint64_t bits = (s ? 1ull : 0ull);
		bits <<= es;
		bits |= biasedExponent;
		bits <<= nbits - 1ull - es;
		bits |= raw;
		bits &= 0xFFFF'FFFF'FFFF'FFFFULL;
		if (nrBlocks == 1) {
			_block[MSU] = bt(bits);
		}
		else {
			copyBits(bits);
		}
		return *this;
	}
	CONSTEXPRESSION bfloat& operator=(long double rhs) {
		return *this = double(rhs);
	}

	// arithmetic operators
	// prefix operator
	inline bfloat operator-() const {
		bfloat tmp(*this);
		tmp._block[MSU] ^= SIGN_BIT_MASK;
		return tmp;
	}

	bfloat& operator+=(const bfloat& rhs) {
		return *this;
	}
	bfloat& operator+=(double rhs) {
		return *this += bfloat(rhs);
	}
	bfloat& operator-=(const bfloat& rhs) {

		return *this;
	}
	bfloat& operator-=(double rhs) {
		return *this -= bfloat<nbits, es>(rhs);
	}
	bfloat& operator*=(const bfloat& rhs) {

		return *this;
	}
	bfloat& operator*=(double rhs) {
		return *this *= bfloat<nbits, es>(rhs);
	}
	bfloat& operator/=(const bfloat& rhs) {

		return *this;
	}
	bfloat& operator/=(double rhs) {
		return *this /= bfloat<nbits, es>(rhs);
	}
	/// <summary>
	/// move to the next bit encoding modulo 2^nbits
	/// </summary>
	/// <typeparam name="bt"></typeparam>
	inline bfloat& operator++() {
		if constexpr (0 == nrBlocks) {
			return *this;
		}
		else if constexpr (1 == nrBlocks) {
			// special cases are: 011...111 and 111...111
			if ((_block[MSU] & MSU_MASK) == MSU_MASK) { // == all bits are set
				_block[MSU] = 0;
			}
			else {
				++_block[MSU];
			}
		}
		else {
			bool carry = true;
			for (unsigned i = 0; i < MSU; ++i) {
				if ((_block[i] & storageMask) == storageMask) { // block will overflow
					++_block[i];
				}
				else {
					++_block[i];
					carry = false;
					break;
				}
			}
			if (carry) {
				// encoding behaves like a 2's complement modulo wise
				if ((_block[MSU] & MSU_MASK) == MSU_MASK) {
					_block[MSU] = 0;
				}
				else {
					++_block[MSU]; // a carry will flip the sign
				}
			}
		}
		return *this;
	}
	inline bfloat operator++(int) {
		bfloat tmp(*this);
		operator++();
		return tmp;
	}
	inline bfloat& operator--() {

		return *this;
	}
	inline bfloat operator--(int) {
		bfloat tmp(*this);
		operator--();
		return tmp;
	}

	// modifiers
	
	/// <summary>
	/// clear the content of this bfloat to zero
	/// </summary>
	/// <returns>void</returns>
	inline constexpr void clear() noexcept {
		for (size_t i = 0; i < nrBlocks; ++i) {
			_block[i] = bt(0);
		}
	}
	/// <summary>
	/// set the number to +0
	/// </summary>
	/// <returns>void</returns>
	inline constexpr void setzero() noexcept { clear(); }
	/// <summary>
	/// set the number to +inf
	/// </summary>
	/// <param name="sign">boolean to make it + or - infinity, default is -inf</param>
	/// <returns>void</returns> 
	inline constexpr void setinf(bool sign = true) noexcept {
		if constexpr (0 == nrBlocks) {
			return;
		}
		else if constexpr (1 == nrBlocks) {
			_block[MSU] = sign ? bt(MSU_MASK ^ LSB_BIT_MASK) : bt(~SIGN_BIT_MASK & (MSU_MASK ^ LSB_BIT_MASK));
		}
		else if constexpr (2 == nrBlocks) {
			_block[0] = BLOCK_MASK ^ LSB_BIT_MASK;
			_block[MSU] = sign ? MSU_MASK : bt(~SIGN_BIT_MASK & MSU_MASK);
		}
		else if constexpr (3 == nrBlocks) {
			_block[0] = BLOCK_MASK ^ LSB_BIT_MASK;
			_block[1] = BLOCK_MASK;
			_block[MSU] = sign ? MSU_MASK : bt(~SIGN_BIT_MASK & MSU_MASK);
		}
		else {
			_block[0] = BLOCK_MASK ^ LSB_BIT_MASK;
			for (size_t i = 1; i < nrBlocks - 1; ++i) {
				_block[i] = BLOCK_MASK;
			}
			_block[MSU] = sign ? MSU_MASK : bt(~SIGN_BIT_MASK & MSU_MASK);
		}	
	}
	/// <summary>
	/// set the number to a quiet NaN (+nan) or a signalling NaN (-nan, default)
	/// </summary>
	/// <param name="sign">boolean to make it + or - infinity, default is -inf</param>
	/// <returns>void</returns> 
	inline constexpr void setnan(int NaNType = NAN_TYPE_SIGNALLING) noexcept {
		if constexpr (0 == nrBlocks) {
			return;
		}
		else if constexpr (1 == nrBlocks) {
			// fall through
		}
		else if constexpr (2 == nrBlocks) {
			_block[0] = BLOCK_MASK;
		}
		else if constexpr (3 == nrBlocks) {
			_block[0] = BLOCK_MASK;
			_block[1] = BLOCK_MASK;
		}
		else {
			for (size_t i = 0; i < nrBlocks - 1; ++i) {
				_block[i] = BLOCK_MASK;
			}
		}
		_block[MSU] = NaNType == NAN_TYPE_SIGNALLING ? MSU_MASK : bt(~SIGN_BIT_MASK & MSU_MASK);
	}

	/// <summary>
	/// set the raw bits of the bfloat. This is a required API function for number systems in the Universal Numbers Library
	/// This enables verification test suites to inject specific test bit patterns using a common interface.
	//  This is a memcpy type operator, but the target number system may not have a linear memory layout and
	//  thus needs to steer the bits in potentially more complicated ways then memcpy.
	/// </summary>
	/// <param name="raw_bits">unsigned long long carrying bits that will be written verbatim to the bfloat</param>
	/// <returns>reference to the bfloat</returns>
	inline constexpr bfloat& set_raw_bits(uint64_t raw_bits) noexcept {
		if constexpr (0 == nrBlocks) {
			return *this;
		}
		else if constexpr (1 == nrBlocks) {
			_block[0] = raw_bits & storageMask;
		}
		else {
			for (size_t i = 0; i < nrBlocks; ++i) {
				_block[i] = raw_bits & storageMask;
				raw_bits >>= bitsInBlock; // shift can be the same size as type as it is protected by loop constraints
			}
		}
		_block[MSU] &= MSU_MASK; // enforce precondition for fast comparison by properly nulling bits that are outside of nbits
		return *this;
	}
	/// <summary>
	/// set a specific bit in the encoding to true or false. If bit index is out of bounds, no modification takes place.
	/// </summary>
	/// <param name="i">bit index to set</param>
	/// <param name="v">boolean value to set the bit to. Default is true.</param>
	/// <returns>void</returns>
	inline constexpr void set(size_t i, bool v = true) noexcept {
		if (i < nbits) {
			bt block = _block[i / bitsInBlock];
			bt null = ~(1ull << (i % bitsInBlock));
			bt bit = bt(v ? 1 : 0);
			bt mask = bt(bit << (i % bitsInBlock));
			_block[i / bitsInBlock] = bt((block & null) | mask);
			return;
		}
	}
	/// <summary>
	/// reset a specific bit in the encoding to false. If bit index is out of bounds, no modification takes place.
	/// </summary>
	/// <param name="i">bit index to reset</param>
	/// <returns>void</returns>
	inline constexpr void reset(size_t i) noexcept {
		if (i < nbits) {
			bt block = _block[i / bitsInBlock];
			bt mask = ~(1ull << (i % bitsInBlock));
			_block[i / bitsInBlock] = bt(block & mask);
			return;
		}
	}
	/// <summary>
	/// 1's complement of the encoding
	/// </summary>
	/// <returns>reference to this bfloat object</returns>
	inline constexpr bfloat& flip() noexcept { // in-place one's complement
		for (size_t i = 0; i < nrBlocks; ++i) {
			_block[i] = bt(~_block[i]);
		}
		_block[MSU] &= MSU_MASK; // assert precondition of properly nulled leading non-bits
		return *this;
	}
	/// <summary>
	/// assign the value of the string representation of a scientific number to the bfloat
	/// </summary>
	/// <param name="stringRep">decimal scientific notation of a real number to be assigned</param>
	/// <returns>reference to this bfloat</returns>
	inline bfloat& assign(const std::string& stringRep) {
		std::cout << "assign TBD\n";
		return *this;
	}

	// selectors
	inline constexpr bool sign() const { return (_block[MSU] & SIGN_BIT_MASK) == SIGN_BIT_MASK; }
	inline constexpr int scale() const {
		int e{ 0 };
		if constexpr (MSU_CAPTURES_E) {
			e = int((_block[MSU] & ~SIGN_BIT_MASK) >> EXP_SHIFT);
			if (e == 0) {
				// subnormal scale is determined by fraction
				// subnormals: (-1)^s * 2^(2-2^(es-1)) * (f/2^fbits))
				e = (2l - (1l << (es - 1ull))) - 1;
				for (size_t i = nbits - 2ull - es; i > 0; --i) {
					if (test(i)) break;
					--e;
				}
			}
			else {
				e -= EXP_BIAS;
			}
		}
		else {
			blockbinary<es, bt> ebits;
			exponent(ebits);
			if (ebits.iszero()) {
				// subnormal scale is determined by fraction
				e = -1;
				for (size_t i = nbits - 2ull - es; i > 0; --i) {
					if (test(i)) break;
					--e;
				}
			}
			else {
				e = int(ebits) - EXP_BIAS;
			}
		}
		return e;
	}
	// tests
	inline constexpr bool isneg() const { return sign(); }
	inline constexpr bool ispos() const { return !sign(); }
	inline constexpr bool iszero() const {
		if constexpr (0 == nrBlocks) {
			return true;
		}
		else if constexpr (1 == nrBlocks) {
			return (_block[MSU] & ~SIGN_BIT_MASK) == 0;
		}
		else if constexpr (2 == nrBlocks) {
			return (_block[0] == 0) && (_block[MSU] & ~SIGN_BIT_MASK) == 0;
		}
		else if constexpr (3 == nrBlocks) {
			return (_block[0] == 0) && _block[1] == 0 && (_block[MSU] & ~SIGN_BIT_MASK) == 0;
		}
		else {
			for (size_t i = 0; i < nrBlocks-1; ++i) if (_block[i] != 0) return false;
			return (_block[MSU] & ~SIGN_BIT_MASK) == 0;
		}
	}
	inline constexpr bool isone() const {
		// unbiased exponent = scale = 0, fraction = 0
		int s = scale();
		if (scale() == 0) {
			blockbinary<fbits, bt> f;
			fraction(f);
			return f.iszero();
		}
		return false;
	}
	/// <summary>
	/// check if value is infinite, -inf, or +inf. 
	/// +inf = 0-1111-11111-0: sign = 0, uncertainty = 0, es/fraction bits = 1
	/// -inf = 1-1111-11111-0: sign = 1, uncertainty = 0, es/fraction bits = 1
	/// </summary>
	/// <param name="InfType">default is 0, both types, -1 checks for -inf, 1 checks for +inf</param>
	/// <returns>true if +-inf, false otherwise</returns>
	inline constexpr bool isinf(int InfType = INF_TYPE_EITHER) const {
		bool isInf = false;
		bool isNegInf = false;
		bool isPosInf = false;
		if constexpr (0 == nrBlocks) {
			return false;
		}
		else if constexpr (1 == nrBlocks) {
			isNegInf = (_block[MSU] & MSU_MASK) == (MSU_MASK ^ LSB_BIT_MASK);
			isPosInf = (_block[MSU] & MSU_MASK) == ((MSU_MASK ^ SIGN_BIT_MASK) ^ LSB_BIT_MASK);
		}
		else if constexpr (2 == nrBlocks) {
			isInf = (_block[0] == (BLOCK_MASK ^ LSB_BIT_MASK));
			isNegInf = isInf && ((_block[MSU] & MSU_MASK) == MSU_MASK);
			isPosInf = isInf && (_block[MSU] & MSU_MASK) == (MSU_MASK ^ SIGN_BIT_MASK);
		}
		else if constexpr (3 == nrBlocks) {
			isInf = (_block[0] == (BLOCK_MASK ^ LSB_BIT_MASK)) && (_block[1] == BLOCK_MASK);
			isNegInf = isInf && ((_block[MSU] & MSU_MASK) == MSU_MASK);
			isPosInf = isInf && (_block[MSU] & MSU_MASK) == (MSU_MASK ^ SIGN_BIT_MASK);
		}
		else {
			isInf = (_block[0] == (BLOCK_MASK ^ LSB_BIT_MASK));
			for (size_t i = 1; i < nrBlocks - 1; ++i) {
				if (_block[i] != BLOCK_MASK) {
					isInf = false;
					break;
				}
			}
			isNegInf = isInf && ((_block[MSU] & MSU_MASK) == MSU_MASK);
			isPosInf = isInf && (_block[MSU] & MSU_MASK) == (MSU_MASK ^ SIGN_BIT_MASK);
		}

		return (InfType == INF_TYPE_EITHER ? (isNegInf || isPosInf) :
			(InfType == INF_TYPE_NEGATIVE ? isNegInf :
				(InfType == INF_TYPE_POSITIVE ? isPosInf : false)));
	}
	/// <summary>
	/// check if a value is a quiet or a signalling NaN
	/// quiet NaN      = 0-1111-11111-1: sign = 0, uncertainty = 1, es/fraction bits = 1
	/// signalling NaN = 1-1111-11111-1: sign = 1, uncertainty = 1, es/fraction bits = 1
	/// </summary>
	/// <param name="NaNType">default is 0, both types, 1 checks for Signalling NaN, -1 checks for Quiet NaN</param>
	/// <returns>true if the right kind of NaN, false otherwise</returns>
	inline constexpr bool isnan(int NaNType = NAN_TYPE_EITHER) const {
		bool isNaN = true;
		if constexpr (0 == nrBlocks) {
			return false;
		}
		else if constexpr (1 == nrBlocks) {
		}
		else if constexpr (2 == nrBlocks) {
			isNaN = (_block[0] == BLOCK_MASK);
		}
		else if constexpr (3 == nrBlocks) {
			isNaN = (_block[0] == BLOCK_MASK) && (_block[1] == BLOCK_MASK);
		}
		else {
			for (size_t i = 0; i < nrBlocks - 1; ++i) {
				if (_block[i] != BLOCK_MASK) {
					isNaN = false;
					break;
				}
			}
		}
		bool isNegNaN = isNaN && ((_block[MSU] & MSU_MASK) == MSU_MASK);
		bool isPosNaN = isNaN && (_block[MSU] & MSU_MASK) == (MSU_MASK ^ SIGN_BIT_MASK);
		return (NaNType == NAN_TYPE_EITHER ? (isNegNaN || isPosNaN) : 
			     (NaNType == NAN_TYPE_SIGNALLING ? isNegNaN : 
				   (NaNType == NAN_TYPE_QUIET ? isPosNaN : false)));
	}

	inline constexpr bool test(size_t bitIndex) const noexcept {
		return at(bitIndex);
	}
	inline constexpr bool at(size_t bitIndex) const noexcept {
		if (bitIndex < nbits) {
			bt word = _block[bitIndex / bitsInBlock];
			bt mask = bt(1ull << (bitIndex % bitsInBlock));
			return (word & mask);
		}
		return false;
	}
	inline constexpr uint8_t nibble(size_t n) const noexcept {
		if (n < (1 + ((nbits - 1) >> 2))) {
			bt word = _block[(n * 4) / bitsInBlock];
			int nibbleIndexInWord = int(n % (bitsInBlock >> 2ull));
			bt mask = bt(0xF << (nibbleIndexInWord * 4));
			bt nibblebits = bt(mask & word);
			return uint8_t(nibblebits >> (nibbleIndexInWord * 4));
		}
		return false;
	}
	inline constexpr bt block(size_t b) const noexcept {
		if (b < nrBlocks) {
			return _block[b];
		}
		return 0;
	}

	void debug() const {
		std::cout << "nbits             : " << nbits << '\n';
		std::cout << "es                : " << es << std::endl;
		std::cout << "ALL_ONES           : " << to_binary_storage(ALL_ONES, true) << '\n';
		std::cout << "BLOCK_MASK        : " << to_binary_storage(BLOCK_MASK, true) << '\n';
		std::cout << "nrBlocks          : " << nrBlocks << '\n';
		std::cout << "bits in MSU       : " << bitsInMSU << '\n';
		std::cout << "MSU               : " << MSU << '\n';
		std::cout << "MSU MASK          : " << to_binary_storage(MSU_MASK, true) << '\n';
		std::cout << "SIGN_BIT_MASK     : " << to_binary_storage(SIGN_BIT_MASK, true) << '\n';
		std::cout << "LSB_BIT_MASK      : " << to_binary_storage(LSB_BIT_MASK, true) << '\n';
		std::cout << "MSU CAPTURES E    : " << (MSU_CAPTURES_E ? "yes\n" : "no\n");
		std::cout << "EXP_SHIFT         : " << EXP_SHIFT << '\n';
		std::cout << "MSU EXP MASK      : " << to_binary_storage(MSU_EXP_MASK, true) << '\n';
		std::cout << "EXP_BIAS          : " << EXP_BIAS << '\n';
		std::cout << "MAX_EXP           : " << MAX_EXP << '\n';
		std::cout << "MIN_EXP_NORMAL    : " << MIN_EXP_NORMAL << '\n';
		std::cout << "MIN_EXP_SUBNORMAL : " << MIN_EXP_SUBNORMAL << '\n';
	}
	// extract the sign firld from the encoding
	inline constexpr void sign(bool& s) const {
		s = sign();
	}
	// extract the exponent field from the encoding
	inline constexpr void exponent(blockbinary<es, bt>& e) const {
		e.clear();
		if constexpr (0 == nrBlocks) return;
		else if constexpr (1 == nrBlocks) {
			bt ebits = bt(_block[MSU] & ~SIGN_BIT_MASK);
			e.set_raw_bits(uint64_t(ebits >> EXP_SHIFT));
		}
		else if constexpr (nrBlocks > 1) {
			if (MSU_CAPTURES_E) {
				bt ebits = bt(_block[MSU] & ~SIGN_BIT_MASK);
				e.set_raw_bits(uint64_t(ebits >> ((nbits - 1ull - es) % bitsInBlock)));
			}
			else {
				for (size_t i = 0; i < es; ++i) { e.set(i, at(nbits - 1ull - es + i)); }
			}
		}
	}
	// extract the fraction field from the encoding
	inline constexpr void fraction(blockbinary<fbits, bt>& f) const {
		f.clear();
		if constexpr (0 == nrBlocks) return;
		else if constexpr (1 == nrBlocks) {
			bt fraction = bt(_block[MSU] & ~MSU_EXP_MASK);
			f.set_raw_bits(fraction);
		}
		else if constexpr (nrBlocks > 1) {
			for (size_t i = 0; i < fbits; ++i) { f.set(i, at(nbits - 1ull - es - fbits + i)); } // TODO: TEST!
		}
	}
	
	// casts to native types
	long to_long() const { return long(to_native<double>()); }
	long long to_long_long() const { return (long long)(to_native<double>()); }
	// transform an bfloat to a native C++ floating-point. We are using the native
	// precision to compute, which means that all sub-values need to be representable 
	// by the native precision.
	// A more accurate appromation would require an adaptive precision algorithm
	// with a final rounding step.
	template<typename TargetFloat>
	TargetFloat to_native() const { 
		TargetFloat v = TargetFloat(0);
		if (iszero()) {
			if (sign()) // the optimizer might destroy the sign
				return -TargetFloat(0);
			else
				return TargetFloat(0);
		}
		else if (isnan()) {
			v = sign() ? std::numeric_limits<TargetFloat>::signaling_NaN() : std::numeric_limits<TargetFloat>::quiet_NaN();
		}
		else if (isinf()) {
			v = sign() ? -INFINITY : INFINITY;
		}
		else { // TODO: this approach has catastrophic cancellation when nbits is large and native target float is small
			TargetFloat f{ 0 };
			TargetFloat fbit{ 0.5 };
			for (int i = static_cast<int>(nbits - 2ull - es); i >= 0; --i) {
				f += at(static_cast<size_t>(i)) ? fbit : TargetFloat(0);
				fbit *= TargetFloat(0.5);
			}
			blockbinary<es, bt> ebits;
			exponent(ebits);
			if (ebits.iszero()) {
				// subnormals: (-1)^s * 2^(2-2^(es-1)) * (f/2^fbits))
				TargetFloat exponentiation = subnormal_exponent[es]; // precomputed values for 2^(2-2^(es-1))
				v = exponentiation * f;
			}
			else {
				// regular: (-1)^s * 2^(e+1-2^(es-1)) * (1 + f/2^fbits))
				int exponent = unsigned(ebits) + 1ll - (1ll << (es - 1ull));
				if (exponent < 64) {
					TargetFloat exponentiation = (exponent >= 0 ? TargetFloat(1ull << exponent) : (1.0f / TargetFloat(1ull << -exponent)));
					v = exponentiation * (TargetFloat(1.0) + f);
				}
				else {
					double exponentiation = ipow(exponent);
					v = exponentiation * (1.0 + f);
				}
			}
			v = sign() ? -v : v;
		}
		return v;
	}

	// make conversions to native types explicit
	explicit operator int() const { return to_long_long(); }
	explicit operator long long() const { return to_long_long(); }
	explicit operator long double() const { return to_native<long double>(); }
	explicit operator double() const { return to_native<double>(); }
	explicit operator float() const { return to_native<float>(); }

protected:
	// HELPER methods

	/// <summary>
	/// round a set of source bits to the present representation.
	/// srcbits is the number of bits of significant in the source representation
	/// </summary>
	/// <typeparam name="StorageType"></typeparam>
	/// <param name="raw"></param>
	/// <returns></returns>
	template<size_t srcbits, typename StorageType>
	constexpr uint64_t round(StorageType raw, int& exponent) noexcept {
		if constexpr (fhbits < srcbits) {
			// round to even: lsb guard round sticky
		   // collect guard, round, and sticky bits
		   // this same logic will work for the case where
		   // we only have a guard bit and no round and sticky bits
		   // because the mask logic will make round and sticky both 0
			constexpr uint32_t shift = srcbits - fhbits - 1ull;
			StorageType mask = (StorageType(1ull) << shift);
			bool guard = (mask & raw);
			mask >>= 1;
			bool round = (mask & raw);
			if constexpr (shift > 1u) { // protect against a negative shift
				StorageType allones(StorageType(~0));
				mask = StorageType(allones << (shift - 2));
				mask = ~mask;
			}
			else {
				mask = 0;
			}
			bool sticky = (mask & raw);

			raw >>= (shift + 1);  // shift out the bits we are rounding away
			bool lsb = (raw & 0x1u);
			//  ... lsb | guard  round sticky   round
			//       x     0       x     x       down
			//       0     1       0     0       down  round to even
			//       1     1       0     0        up   round to even
			//       x     1       0     1        up
			//       x     1       1     0        up
			//       x     1       1     1        up
			if (guard) {
				if (lsb && (!round && !sticky)) ++raw; // round to even
				if (round || sticky) ++raw;
				if (raw == (1ull << nbits)) { // overflow
					++exponent;
					raw >>= 1u;
				}
			}
		}
		else {
			constexpr size_t shift = fhbits - srcbits;
			raw <<= shift;
		}
		uint64_t significant = raw;
		return significant;
	}
	template<typename ArgumentBlockType>
	constexpr void copyBits(ArgumentBlockType v) {
		size_t blocksRequired = (8 * sizeof(v) + 1 ) / bitsInBlock;
		size_t maxBlockNr = (blocksRequired < nrBlocks ? blocksRequired : nrBlocks);
		bt b{ 0ul }; b = bt(~b);
		ArgumentBlockType mask = ArgumentBlockType(b);
		size_t shift = 0;
		for (size_t i = 0; i < maxBlockNr; ++i) {
			_block[i] = bt((mask & v) >> shift);
			mask <<= bitsInBlock;
			shift += bitsInBlock;
		}
	}
	void shiftLeft(int bitsToShift) {
		if (bitsToShift == 0) return;
		if (bitsToShift < 0) return shiftRight(-bitsToShift);
		if (bitsToShift > long(nbits)) bitsToShift = nbits; // clip to max
		if (bitsToShift >= long(bitsInBlock)) {
			int blockShift = bitsToShift / bitsInBlock;
			for (signed i = signed(MSU); i >= blockShift; --i) {
				_block[i] = _block[i - blockShift];
			}
			for (signed i = blockShift - 1; i >= 0; --i) {
				_block[i] = bt(0);
			}
			// adjust the shift
			bitsToShift -= (long)(blockShift * bitsInBlock);
			if (bitsToShift == 0) return;
		}
		// construct the mask for the upper bits in the block that need to move to the higher word
		bt mask = 0xFFFFFFFFFFFFFFFF << (bitsInBlock - bitsToShift);
		for (unsigned i = MSU; i > 0; --i) {
			_block[i] <<= bitsToShift;
			// mix in the bits from the right
			bt bits = (mask & _block[i - 1]);
			_block[i] |= (bits >> (bitsInBlock - bitsToShift));
		}
		_block[0] <<= bitsToShift;
	}

	void shiftRight(int bitsToShift) {
		if (bitsToShift == 0) return;
		if (bitsToShift < 0) return shiftLeft(-bitsToShift);
		if (bitsToShift >= long(nbits)) {
			setzero();
			return;
		}
		bool signext = sign();
		size_t blockShift = 0;
		if (bitsToShift >= long(bitsInBlock)) {
			blockShift = bitsToShift / bitsInBlock;
			if (MSU >= blockShift) {
				// shift by blocks
				for (size_t i = 0; i <= MSU - blockShift; ++i) {
					_block[i] = _block[i + blockShift];
				}
			}
			// adjust the shift
			bitsToShift -= (long)(blockShift * bitsInBlock);
			if (bitsToShift == 0) {
				// fix up the leading zeros if we have a negative number
				if (signext) {
					// bitsToShift is guaranteed to be less than nbits
					bitsToShift += (long)(blockShift * bitsInBlock);
					for (size_t i = nbits - bitsToShift; i < nbits; ++i) {
						this->set(i);
					}
				}
				else {
					// clean up the blocks we have shifted clean
					bitsToShift += (long)(blockShift * bitsInBlock);
					for (size_t i = nbits - bitsToShift; i < nbits; ++i) {
						this->reset(i);
					}
				}
			}
		}
		//bt mask = 0xFFFFFFFFFFFFFFFFull >> (64 - bitsInBlock);  // is that shift necessary?
		bt mask = bt(0xFFFFFFFFFFFFFFFFull);
		mask >>= (bitsInBlock - bitsToShift); // this is a mask for the lower bits in the block that need to move to the lower word
		for (unsigned i = 0; i < MSU; ++i) {  // TODO: can this be improved? we should not have to work on the upper blocks in case we block shifted
			_block[i] >>= bitsToShift;
			// mix in the bits from the left
			bt bits = (mask & _block[i + 1]);
			_block[i] |= (bits << (bitsInBlock - bitsToShift));
		}
		_block[MSU] >>= bitsToShift;

		// fix up the leading zeros if we have a negative number
		if (signext) {
			// bitsToShift is guaranteed to be less than nbits
			bitsToShift += (long)(blockShift * bitsInBlock);
			for (size_t i = nbits - bitsToShift; i < nbits; ++i) {
				this->set(i);
			}
		}
		else {
			// clean up the blocks we have shifted clean
			bitsToShift += (long)(blockShift * bitsInBlock);
			for (size_t i = nbits - bitsToShift; i < nbits; ++i) {
				this->reset(i);
			}
		}

		// enforce precondition for fast comparison by properly nulling bits that are outside of nbits
		_block[MSU] &= MSU_MASK;
	}

	// calculate the integer power 2 ^ b using exponentiation by squaring
	double ipow(int exponent) const {
		bool negative = (exponent < 0);
		exponent = negative ? -exponent : exponent;
		double result(1.0);
		double base = 2.0;
		for (;;) {
			if (exponent % 2) result *= base;
			exponent >>= 1;
			if (exponent == 0) break;
			base *= base;
		}
		return (negative ? (1.0 / result) : result);
	}

private:
	bt _block[nrBlocks];

	//////////////////////////////////////////////////////////////////////////////
	// friend functions

	// template parameters need names different from class template parameters (for gcc and clang)
	template<size_t nnbits, size_t nes, typename nbt>
	friend std::ostream& operator<< (std::ostream& ostr, const bfloat<nnbits,nes,nbt>& r);
	template<size_t nnbits, size_t nes, typename nbt>
	friend std::istream& operator>> (std::istream& istr, bfloat<nnbits,nes,nbt>& r);

	template<size_t nnbits, size_t nes, typename nbt>
	friend bool operator==(const bfloat<nnbits,nes,nbt>& lhs, const bfloat<nnbits,nes,nbt>& rhs);
	template<size_t nnbits, size_t nes, typename nbt>
	friend bool operator!=(const bfloat<nnbits,nes,nbt>& lhs, const bfloat<nnbits,nes,nbt>& rhs);
	template<size_t nnbits, size_t nes, typename nbt>
	friend bool operator< (const bfloat<nnbits,nes,nbt>& lhs, const bfloat<nnbits,nes,nbt>& rhs);
	template<size_t nnbits, size_t nes, typename nbt>
	friend bool operator> (const bfloat<nnbits,nes,nbt>& lhs, const bfloat<nnbits,nes,nbt>& rhs);
	template<size_t nnbits, size_t nes, typename nbt>
	friend bool operator<=(const bfloat<nnbits,nes,nbt>& lhs, const bfloat<nnbits,nes,nbt>& rhs);
	template<size_t nnbits, size_t nes, typename nbt>
	friend bool operator>=(const bfloat<nnbits,nes,nbt>& lhs, const bfloat<nnbits,nes,nbt>& rhs);
};

////////////////////// operators
template<size_t nbits, size_t es, typename bt>
inline std::ostream& operator<<(std::ostream& ostr, const bfloat<nbits,es,bt>& v) {
	// TODO: make it a native conversion
	ostr << double(v);
	return ostr;
}

template<size_t nnbits, size_t nes, typename nbt>
inline std::istream& operator>>(std::istream& istr, const bfloat<nnbits,nes,nbt>& v) {
	istr >> v._fraction;
	return istr;
}

template<size_t nnbits, size_t nes, typename nbt>
inline bool operator==(const bfloat<nnbits,nes,nbt>& lhs, const bfloat<nnbits,nes,nbt>& rhs) { 
	for (size_t i = 0; i < lhs.nrBlocks; ++i) {
		if (lhs._block[i] != rhs._block[i]) {
			return false;
		}
	}
	return true;
}
template<size_t nnbits, size_t nes, typename nbt>
inline bool operator!=(const bfloat<nnbits,nes,nbt>& lhs, const bfloat<nnbits,nes,nbt>& rhs) { return !operator==(lhs, rhs); }
template<size_t nnbits, size_t nes, typename nbt>
inline bool operator< (const bfloat<nnbits,nes,nbt>& lhs, const bfloat<nnbits,nes,nbt>& rhs) { return (lhs - rhs).isneg(); }
template<size_t nnbits, size_t nes, typename nbt>
inline bool operator> (const bfloat<nnbits,nes,nbt>& lhs, const bfloat<nnbits,nes,nbt>& rhs) { return  operator< (rhs, lhs); }
template<size_t nnbits, size_t nes, typename nbt>
inline bool operator<=(const bfloat<nnbits,nes,nbt>& lhs, const bfloat<nnbits,nes,nbt>& rhs) { return !operator> (lhs, rhs); }
template<size_t nnbits, size_t nes, typename nbt>
inline bool operator>=(const bfloat<nnbits,nes,nbt>& lhs, const bfloat<nnbits,nes,nbt>& rhs) { return !operator< (lhs, rhs); }

// posit - posit binary arithmetic operators
// BINARY ADDITION
template<size_t nbits, size_t es, typename bt>
inline bfloat<nbits, es, bt> operator+(const bfloat<nbits, es, bt>& lhs, const bfloat<nbits, es, bt>& rhs) {
	bfloat<nbits, es, bt> sum(lhs);
	sum += rhs;
	return sum;
}
// BINARY SUBTRACTION
template<size_t nbits, size_t es, typename bt>
inline bfloat<nbits, es, bt> operator-(const bfloat<nbits, es, bt>& lhs, const bfloat<nbits, es, bt>& rhs) {
	bfloat<nbits, es, bt> diff(lhs);
	diff -= rhs;
	return diff;
}
// BINARY MULTIPLICATION
template<size_t nbits, size_t es, typename bt>
inline bfloat<nbits, es, bt> operator*(const bfloat<nbits, es, bt>& lhs, const bfloat<nbits, es, bt>& rhs) {
	bfloat<nbits, es, bt> mul(lhs);
	mul *= rhs;
	return mul;
}
// BINARY DIVISION
template<size_t nbits, size_t es, typename bt>
inline bfloat<nbits, es, bt> operator/(const bfloat<nbits, es, bt>& lhs, const bfloat<nbits, es, bt>& rhs) {
	bfloat<nbits, es, bt> ratio(lhs);
	ratio /= rhs;
	return ratio;
}

// convert to std::string
template<size_t nbits, size_t es, typename bt>
inline std::string to_string(const bfloat<nbits,es,bt>& v) {
	std::stringstream s;
	if (v.iszero()) {
		s << " zero b";
		return s.str();
	}
	else if (v.isinf()) {
		s << " infinite b";
		return s.str();
	}
//	s << "(" << (v.sign() ? "-" : "+") << "," << v.scale() << "," << v.fraction() << ")";
	return s.str();
}

// transform bfloat to a binary representation
template<size_t nbits, size_t es, typename bt>
inline std::string to_binary(const bfloat<nbits, es, bt>& number, bool nibbleMarker = false) {
	std::stringstream ss;
	ss << 'b';
	size_t index = nbits;
	for (size_t i = 0; i < nbits; ++i) {
		ss << (number.at(--index) ? '1' : '0');
		if (index > 0 && (index % 4) == 0 && nibbleMarker) ss << '\'';
	}
	return ss.str();
}

// helper to report on BlockType blocks
template<typename bt>
inline std::string to_binary_storage(const bt& number, bool nibbleMarker) {
	std::stringstream ss;
	ss << 'b';
	constexpr size_t nbits = sizeof(bt) * 8;
	bt mask = bt(bt(1ull) << (nbits - 1ull));
	size_t index = nbits;
	for (size_t i = 0; i < nbits; ++i) {
		ss << (number & mask ? '1' : '0');
		--index;
		if (index > 0 && (index % 4) == 0 && nibbleMarker) ss << '\'';
		mask >>= 1ul;
	}
	return ss.str();
}

/// Magnitude of a scientific notation value (equivalent to turning the sign bit off).
template<size_t nbits, size_t es, typename bt>
bfloat<nbits,es> abs(const bfloat<nbits,es,bt>& v) {
	return bfloat<nbits,es>(false, v.scale(), v.fraction(), v.isZero());
}


///////////////////////////////////////////////////////////////////////
///   binary logic literal comparisons

// posit - long logic operators
template<size_t nbits, size_t es, typename bt>
inline bool operator==(const bfloat<nbits, es, bt>& lhs, long long rhs) {
	return operator==(lhs, bfloat<nbits, es, bt>(rhs));
}
template<size_t nbits, size_t es, typename bt>
inline bool operator!=(const bfloat<nbits, es, bt>& lhs, long long rhs) {
	return operator!=(lhs, bfloat<nbits, es, bt>(rhs));
}
template<size_t nbits, size_t es, typename bt>
inline bool operator< (const bfloat<nbits, es, bt>& lhs, long long rhs) {
	return operator<(lhs, bfloat<nbits, es, bt>(rhs));
}
template<size_t nbits, size_t es, typename bt>
inline bool operator> (const bfloat<nbits, es, bt>& lhs, long long rhs) {
	return operator<(bfloat<nbits, es, bt>(rhs), lhs);
}
template<size_t nbits, size_t es, typename bt>
inline bool operator<=(const bfloat<nbits, es, bt>& lhs, long long rhs) {
	return operator<(lhs, bfloat<nbits, es, bt>(rhs)) || operator==(lhs, bfloat<nbits, es, bt>(rhs));
}
template<size_t nbits, size_t es, typename bt>
inline bool operator>=(const bfloat<nbits, es, bt>& lhs, long long rhs) {
	return !operator<(lhs, bfloat<nbits, es, bt>(rhs));
}

}  // namespace sw::universal
