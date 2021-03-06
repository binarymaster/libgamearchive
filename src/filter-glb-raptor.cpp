/**
 * @file   filter-glb-raptor.cpp
 * @brief  Filter that encrypts and decrypts files in Raptor GLB archives.
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

#include <boost/iostreams/invert.hpp>
#include <camoto/stream_filtered.hpp>
#include "filter-glb-raptor.hpp"

namespace camoto {
namespace gamearchive {

/// Key to use for .GLB files
#define GLB_KEY "32768GLB"

/// Length of each cipher block in the .GLB FAT
#define GLB_BLOCKLEN 28

filter_glb_decrypt::filter_glb_decrypt(const std::string& key, int lenBlock)
	:	lenBlock(lenBlock),
		key(key),
		lenKey(key.length()),
		// posKey in reset()
		offset(0)
		// lastByte in reset()
{
	this->reset(0);
}

filter_glb_decrypt::~filter_glb_decrypt()
{
}

void filter_glb_decrypt::reset(stream::len lenInput)
{
	this->posKey = 25 % this->lenKey;
	this->lastByte = this->key[this->posKey];
	return;
}

void filter_glb_decrypt::transform(uint8_t *out, stream::len *lenOut,
	const uint8_t *in, stream::len *lenIn)
{
	stream::len lenRemaining = std::min(*lenIn, *lenOut);
	*lenIn = 0;
	*lenOut = 0;
	while (lenRemaining--) {
		// Reset the cipher if the block length has been reached
		if (this->lenBlock != 0) {
			if ((this->offset % this->lenBlock) == 0) {
				this->reset(0);
			}
		}

		*out = (*in - this->key[this->posKey] - this->lastByte) & 0xFF;
		this->posKey++;
		this->posKey %= this->lenKey;
		this->lastByte = *in;
		out++;
		in++;
		(*lenIn)++;
		(*lenOut)++;
		this->offset++;
	}
	return;
}

filter_glb_encrypt::filter_glb_encrypt(const std::string& key, int lenBlock)
	:	lenBlock(lenBlock),
		key(key),
		lenKey(key.length()),
		// posKey in reset()
		offset(0)
		// lastByte in reset()
{
	this->reset(0);
}

filter_glb_encrypt::~filter_glb_encrypt()
{
}

void filter_glb_encrypt::reset(stream::len lenInput)
{
	this->posKey = 25 % this->lenKey;
	this->lastByte = this->key[this->posKey];
	return;
}

void filter_glb_encrypt::transform(uint8_t *out, stream::len *lenOut,
	const uint8_t *in, stream::len *lenIn)
{
	stream::len lenRemaining = std::min(*lenIn, *lenOut);
	*lenIn = 0;
	*lenOut = 0;
	while (lenRemaining--) {
		// Reset the cipher if the block length has been reached
		if (this->lenBlock != 0) {
			if ((this->offset % this->lenBlock) == 0) {
				this->reset(0);
			}
		}

		*out = (*in + this->lastByte + this->key[this->posKey]) & 0xFF;
		this->posKey++;
		this->posKey %= this->lenKey;
		this->lastByte = *out;//in;
		out++;
		in++;
		(*lenIn)++;
		(*lenOut)++;
		this->offset++;
	}
	return;
}


GLBFATFilterType::GLBFATFilterType()
{
}

GLBFATFilterType::~GLBFATFilterType()
{
}

std::string GLBFATFilterType::getFilterCode() const
{
	return "glb-raptor-fat";
}

std::string GLBFATFilterType::getFriendlyName() const
{
	return "Raptor GLB FAT encryption";
}

std::vector<std::string> GLBFATFilterType::getGameList() const
{
	std::vector<std::string> vcGames;
	vcGames.push_back("Raptor");
	return vcGames;
}

stream::inout_sptr GLBFATFilterType::apply(stream::inout_sptr target,
	stream::fn_truncate resize) const
{
	stream::filtered_sptr st(new stream::filtered());
	// We need two separate filters, otherwise reading from one will
	// affect the XOR key next used when writing to the other.
	filter_sptr de(new filter_glb_decrypt(GLB_KEY, GLB_BLOCKLEN));
	filter_sptr en(new filter_glb_encrypt(GLB_KEY, GLB_BLOCKLEN));
	st->open(target, de, en, resize);
	return st;
}

stream::input_sptr GLBFATFilterType::apply(stream::input_sptr target) const
{
	stream::input_filtered_sptr st(new stream::input_filtered());
	filter_sptr de(new filter_glb_decrypt(GLB_KEY, GLB_BLOCKLEN));
	st->open(target, de);
	return st;
}

stream::output_sptr GLBFATFilterType::apply(stream::output_sptr target,
	stream::fn_truncate resize) const
{
	stream::output_filtered_sptr st(new stream::output_filtered());
	filter_sptr en(new filter_glb_encrypt(GLB_KEY, GLB_BLOCKLEN));
	st->open(target, en, resize);
	return st;
}


GLBFileFilterType::GLBFileFilterType()
{
}

GLBFileFilterType::~GLBFileFilterType()
{
}

std::string GLBFileFilterType::getFilterCode() const
{
	return "glb-raptor";
}

std::string GLBFileFilterType::getFriendlyName() const
{
	return "Raptor GLB file encryption";
}

std::vector<std::string> GLBFileFilterType::getGameList() const
{
	std::vector<std::string> vcGames;
	vcGames.push_back("Raptor");
	return vcGames;
}

stream::inout_sptr GLBFileFilterType::apply(stream::inout_sptr target,
	stream::fn_truncate resize) const
{
	stream::filtered_sptr st(new stream::filtered());
	// We need two separate filters, otherwise reading from one will
	// affect the XOR key next used when writing to the other.
	filter_sptr de(new filter_glb_decrypt(GLB_KEY, 0));
	filter_sptr en(new filter_glb_encrypt(GLB_KEY, 0));
	st->open(target, de, en, resize);
	return st;
}

stream::input_sptr GLBFileFilterType::apply(stream::input_sptr target) const
{
	stream::input_filtered_sptr st(new stream::input_filtered());
	filter_sptr de(new filter_glb_decrypt(GLB_KEY, 0));
	st->open(target, de);
	return st;
}

stream::output_sptr GLBFileFilterType::apply(stream::output_sptr target,
	stream::fn_truncate resize) const
{
	stream::output_filtered_sptr st(new stream::output_filtered());
	filter_sptr en(new filter_glb_encrypt(GLB_KEY, 0));
	st->open(target, en, resize);
	return st;
}

} // namespace gamearchive
} // namespace camoto
