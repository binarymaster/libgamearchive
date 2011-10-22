/**
 * @file   filter-xor.hpp
 * @brief  Filter implementation for encrypting and decrypting XOR coded files.
 *
 * Copyright (C) 2010-2011 Adam Nielsen <malvineous@shikadi.net>
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

#ifndef _CAMOTO_FILTER_XOR_HPP_
#define _CAMOTO_FILTER_XOR_HPP_

#include <camoto/filter.hpp>
#include <camoto/gamearchive/filtertype.hpp>

namespace camoto {
namespace gamearchive {

/// Encrypt a stream using XOR encryption.
/**
 * This starts by encrypting the first byte with the given seed value, then
 * the seed is incremented by one for the following byte.
 */
class filter_xor_crypt: public filter {

	protected:
		/// Number of bytes to crypt, after this data is left as plaintext.
		/// 0 means crypt everything.
		int lenCrypt;

		/// Initial XOR value
		int seed;

		/// Current offset (number of bytes processed)
		int offset;

	public:

		/// Create a new encryption filter with the given options.
		/**
		 * @param lenCrypt
		 *   @copybrief lenCrypt
		 *
		 * @param seed
		 *   @copybrief seed
		 */
		filter_xor_crypt(int lenCrypt, int seed)
			throw ();

		void transform(uint8_t *out, stream::len *lenOut,
			const uint8_t *in, stream::len *lenIn)
			throw (filter_error);

		/// Change the next XOR value
		void setSeed(int val);

		/// Get the next byte's seed value.
		/**
		 * This can be overridden by descendent classes to provide
		 * custom algorithms here.
		 *
		 * @param delta
		 *   Offset.  Normally +1 for each byte read, but can be more, or negative,
		 *   when seeking.
		 */
		virtual uint8_t getKey();

};

class XORFilterType: virtual public FilterType {

	public:
		XORFilterType()
			throw ();

		~XORFilterType()
			throw ();

		virtual std::string getFilterCode() const
			throw ();

		virtual std::string getFriendlyName() const
			throw ();

		virtual std::vector<std::string> getGameList() const
			throw ();

		virtual stream::inout_sptr apply(stream::inout_sptr target)
			throw (filter_error, stream::read_error);

		virtual stream::input_sptr apply(stream::input_sptr target)
			throw (filter_error, stream::read_error);

		virtual stream::output_sptr apply(stream::output_sptr target)
			throw (filter_error);

};

} // namespace gamearchive
} // namespace camoto

#endif // _CAMOTO_FILTER_XOR_HPP_
