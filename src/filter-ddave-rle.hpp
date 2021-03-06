/**
 * @file   filter-ddave-rle.hpp
 * @brief  Filter implementation for decompressing Dangerous Dave tilesets.
 *
 * Copyright (C) 2010-2013 Adam Nielsen <malvineous@shikadi.net>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _CAMOTO_FILTER_DDAVE_RLE_HPP_
#define _CAMOTO_FILTER_DDAVE_RLE_HPP_

#include <camoto/stream.hpp>
#include <stdint.h>
#include <camoto/gamearchive/filtertype.hpp>

namespace camoto {
namespace gamearchive {

/// Dangerous Dave RLE expansion filter.
class filter_ddave_unrle: virtual public filter
{
	protected:
		int count;         ///< How many times to repeat prev
		uint8_t countByte; ///< Byte being repeated count times
		int copying;       ///< Number of bytes left to copy unchanged

	public:
		virtual void reset(stream::len lenInput);
		virtual void transform(uint8_t *out, stream::len *lenOut,
			const uint8_t *in, stream::len *lenIn);
};

/// Dangerous Dave RLE compression filter.
class filter_ddave_rle: virtual public filter
{
	protected:
		uint8_t buf[129];    ///< Chars to output as-is
		unsigned int buflen; ///< Number of valid chars in buf

		uint8_t prev;        ///< Previous byte read
		unsigned int count;  ///< How many prev has been seen so far
		unsigned int step;   ///< Which point in the algorithm are we up to?

	public:
		virtual void reset(stream::len lenInput);
		virtual void transform(uint8_t *out, stream::len *lenOut,
			const uint8_t *in, stream::len *lenIn);
};

/// Dangerous Dave RLE decompression filter.
class DDaveRLEFilterType: virtual public FilterType
{
	public:
		DDaveRLEFilterType();
		~DDaveRLEFilterType();

		virtual std::string getFilterCode() const;
		virtual std::string getFriendlyName() const;
		virtual std::vector<std::string> getGameList() const;
		virtual stream::inout_sptr apply(stream::inout_sptr target,
			stream::fn_truncate resize) const;
		virtual stream::input_sptr apply(stream::input_sptr target) const;
		virtual stream::output_sptr apply(stream::output_sptr target,
			stream::fn_truncate resize) const;
};

} // namespace gamearchive
} // namespace camoto

#endif // _CAMOTO_FILTER_DDAVE_RLE_HPP_
