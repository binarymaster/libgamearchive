/**
 * @file   filter-ddave-rle.cpp
 * @brief  Filter implementation for decompressing Dangerous Dave tilesets.
 *
 * This file format is fully documented on the ModdingWiki:
 *   http://www.shikadi.net/moddingwiki/Dangerous_Dave_Graphics_Format
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

#include <camoto/filter.hpp>
#include <camoto/stream_filtered.hpp>
#include <camoto/gamearchive/filtertype.hpp>
#include "filter-ddave-rle.hpp"

namespace camoto {
namespace gamearchive {

void filter_ddave_unrle::reset(stream::len lenInput)
{
	this->count = 0;
	this->copying = 0;
	return;
}

void filter_ddave_unrle::transform(uint8_t *out, stream::len *lenOut,
	const uint8_t *in, stream::len *lenIn)
{
	stream::len r = 0, w = 0;
	// While there's more space to write, and either more data to read or
	// more data to write
	while (              // while there is...
		(w < *lenOut)      // more space to write into, and
		&& (
			(r < *lenIn)     // more data to read, or
			|| (this->count) // more data to write
		)
	) {
		// If there is an RLE decode in progress
		if (this->count) {
			*out++ = this->countByte;
			this->count--;
			w++;
		} else {
			// Otherwise no RLE decode in progress, keep reading
			if (this->copying) {
				*out++ = *in++;
				w++;
				r++;
				this->copying--;
			} else if (*in & 0x80) { // high bit set
				this->copying = 1 + (*in & 0x7F);
				in++;
				r++;
			} else { // high bit unset
				if (r + 2 > *lenIn) {
					// Not enough for this process, try again next time
					break;
				}
				this->count = 3 + *in++;
				this->countByte = *in++;
				r += 2;
			}
		}
	}

	*lenIn = r;
	*lenOut = w;
	return;
}


void filter_ddave_rle::reset(stream::len lenInput)
{
	this->buflen = 0;
	this->prev = 0;
	this->count = 0;
	this->step = 0;
	return;
}

void filter_ddave_rle::transform(uint8_t *out, stream::len *lenOut,
	const uint8_t *in, stream::len *lenIn)
{
	stream::len r = 0, w = 0;

	// While there's more space to write, and either more data to read or
	// more data to write
	while (              // while there is...
		(w < *lenOut)      // more space to write into, and
		&& (
			(r < *lenIn)     // more data to read, or
			|| (
				(*lenIn == 0) && ( // no data to read, but
					(this->count)    // more data to write
					|| (this->buflen)
				)
			)
		)
	) {
		if ((*lenIn == 0) && (step < 20)) {
			// No more read data, just flush
			if ((this->buflen) && (this->count == 0)) step = 50;
			else step = 11;
		}
		switch (step) {
			case 0:
				this->prev = *in++;
				r++;
				this->count = 1;
				step = 10;
				break;
			case 10:
				if (*in == this->prev) {
					this->count++;
					in++;
					r++;
					if (this->count == 130) {
						// If we've reached the maximum repeat amount, write out a code
						*out++ = '\x7F';
						w++;
						step = 21;
					} else if ((count == 3) && (this->buflen)) {
						// If we've reached the point where it is worth writing out the
						// repeat code (eventually), flush the buffer now
						//*out++ = (uint8_t)(0x80 + this->buflen - 1);
						//w++;
						step = 50;
					}
					break;
				} // else drop through
			case 11:
				// Character has changed, write out any cache
				if (this->count >= 3) {
					if (this->buflen) {
						// Need to flush buffer
						step = 50;
						break;
					}

					// Got some repeated bytes to write
					step = 25;
					break;
				}

				// May have some repeated bytes, but they're too short for an RLE code,
				// so include them in the escaped data
				while ((count > 0) && (buflen < 128)) {
					assert(this->buflen < 128);
					this->buf[this->buflen++] = this->prev;
					this->count--;
				}
				if (this->buflen == 128) {
					// Buffer is full, write out a code
					step = 50;
					break;
				}
				step = 0;
				break;
			case 21:
				*out++ = this->prev;
				w++;
				this->count = 0;
				step = 0;
				break;
			case 25:
				assert(this->count <= 130);
				*out++ = (uint8_t)(this->count - 3);
				w++;
				step = 26;
				break;
			case 26:
				*out++ = this->prev;
				w++;
				this->count = 0;
				step = 10;
				break;
			case 50: { // flush buffer
				assert(this->buflen > 0);
				assert(this->buflen < 129);
				*out++ = (uint8_t)(0x80 + this->buflen - 1);
				w++;
				step = 51;
				break;
			}
			case 51:
				assert(this->buflen > 0);
				stream::len maxCopy = *lenOut - w;
				if (this->buflen < maxCopy) maxCopy = this->buflen;
				memcpy(out, this->buf, maxCopy);
				out += maxCopy;
				w += maxCopy;
				if (maxCopy < this->buflen) {
					memmove(this->buf, this->buf + maxCopy, this->buflen - maxCopy);
				} else {
					step = 10;
				}
				this->buflen -= maxCopy;
				break;
		}
	}

	*lenIn = r;
	*lenOut = w;
	return;
}


DDaveRLEFilterType::DDaveRLEFilterType()
{
}

DDaveRLEFilterType::~DDaveRLEFilterType()
{
}

std::string DDaveRLEFilterType::getFilterCode() const
{
	return "rle-ddave";
}

std::string DDaveRLEFilterType::getFriendlyName() const
{
	return "Dangerous Dave RLE";
}

std::vector<std::string> DDaveRLEFilterType::getGameList() const
{
	std::vector<std::string> vcGames;
	vcGames.push_back("Dangerous Dave");
	return vcGames;
}

stream::inout_sptr DDaveRLEFilterType::apply(stream::inout_sptr target,
	stream::fn_truncate resize) const
{
	stream::filtered_sptr st(new stream::filtered());
	filter_sptr de(new filter_ddave_unrle());
	filter_sptr en(new filter_ddave_rle());
	st->open(target, de, en, resize);
	return st;
}

stream::input_sptr DDaveRLEFilterType::apply(stream::input_sptr target) const
{
	stream::input_filtered_sptr st(new stream::input_filtered());
	filter_sptr de(new filter_ddave_unrle());
	st->open(target, de);
	return st;
}

stream::output_sptr DDaveRLEFilterType::apply(stream::output_sptr target,
	stream::fn_truncate resize) const
{
	stream::output_filtered_sptr st(new stream::output_filtered());
	filter_sptr en(new filter_ddave_rle());
	st->open(target, en, resize);
	return st;
}

} // namespace gamearchive
} // namespace camoto
