/**
 * @file   fmt-epf-lionking.cpp
 * @brief  Implementation of reader/writer for East Point Software's .EPF file
 *         format, used in The Lion King among other games.
 *
 * This file format is fully documented on the ModdingWiki:
 *   http://www.shikadi.net/moddingwiki/EPF_Format
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

#include <boost/algorithm/string.hpp>
#include <camoto/iostream_helpers.hpp>
#include <camoto/util.hpp>

#include "fmt-epf-lionking.hpp"

#define EPF_HEADER_LEN               11
#define EPF_FAT_OFFSET_POS           4
#define EPF_FILECOUNT_POS            9
#define EPF_FIRST_FILE_OFFSET        EPF_HEADER_LEN

#define EPF_FAT_FILENAME_OFFSET      0
#define EPF_MAX_FILENAME_LEN         12
#define EPF_FILENAME_FIELD_LEN       13
#define EPF_FAT_ISCOMPRESSED_OFFSET  13
#define EPF_FAT_FILESIZE_OFFSET      14
#define EPF_FAT_DECOMP_SIZE_OFFSET   18
#define EPF_FAT_ENTRY_LEN            22

#define EPF_FAT_FLAG_COMPRESSED      1

namespace camoto {
namespace gamearchive {

EPFType::EPFType()
	throw ()
{
}

EPFType::~EPFType()
	throw ()
{
}

std::string EPFType::getArchiveCode() const
	throw ()
{
	return "epf-lionking";
}

std::string EPFType::getFriendlyName() const
	throw ()
{
	return "East Point Software EPFS File";
}

std::vector<std::string> EPFType::getFileExtensions() const
	throw ()
{
	std::vector<std::string> vcExtensions;
	vcExtensions.push_back("epf");
	return vcExtensions;
}

std::vector<std::string> EPFType::getGameList() const
	throw ()
{
	std::vector<std::string> vcGames;
	vcGames.push_back("Alien Breed Tower Assault");
	vcGames.push_back("Arcade Pool");
	vcGames.push_back("Jungle Book, The");
	vcGames.push_back("Lion King, The");
	vcGames.push_back("Project X");
	vcGames.push_back("Overdrive");
	vcGames.push_back("Sensible Golf");
	vcGames.push_back("Smurfs, The");
	vcGames.push_back("Spirou");
	vcGames.push_back("Tin Tin in Tibet");
	vcGames.push_back("Universe");
	return vcGames;
}

ArchiveType::Certainty EPFType::isInstance(stream::input_sptr psArchive) const
	throw (stream::error)
{
	stream::pos lenArchive = psArchive->size();

	// TESTED BY: fmt_epf_lionking_isinstance_c02
	if (lenArchive < EPF_HEADER_LEN) return DefinitelyNo; // too short

	char sig[4];
	psArchive->seekg(0, stream::start);
	psArchive->read(sig, 4);

	// TESTED BY: fmt_epf_lionking_isinstance_c00
	if (strncmp(sig, "EPFS", 4) == 0) return DefinitelyYes;

	// TESTED BY: fmt_epf_lionking_isinstance_c01
	return DefinitelyNo;
}

ArchivePtr EPFType::newArchive(stream::inout_sptr psArchive, SuppData& suppData) const
	throw (stream::error)
{
	psArchive->seekp(0, stream::start);
	psArchive
		<< nullPadded("EPFS", 4)
		<< u32le(11) // FAT offset
		<< u8(0)     // Unknown/flags?
		<< u16le(0); // File count
	return ArchivePtr(new EPFArchive(psArchive));
}

ArchivePtr EPFType::open(stream::inout_sptr psArchive, SuppData& suppData) const
	throw (stream::error)
{
	return ArchivePtr(new EPFArchive(psArchive));
}

SuppFilenames EPFType::getRequiredSupps(stream::input_sptr data,
	const std::string& filenameArchive) const
	throw ()
{
	// No supplemental types/empty list
	return SuppFilenames();
}


EPFArchive::EPFArchive(stream::inout_sptr psArchive)
	throw (stream::error) :
		FATArchive(psArchive, EPF_FIRST_FILE_OFFSET, EPF_MAX_FILENAME_LEN)
{
	stream::pos lenArchive = this->psArchive->size();

	// We still have to perform sanity checks in case the user forced an archive
	// to open even though it failed the signature check.
	if (lenArchive < EPF_HEADER_LEN) throw stream::error("file too short");

	this->psArchive->seekg(4, stream::start); // skip "EPFS" sig

	uint8_t unknown;
	uint16_t numFiles;
	this->psArchive
		>> u32le(this->offFAT)
		>> u8(unknown)
		>> u16le(numFiles);

	if (
		// These two comparisons are sort of redundant, but we need the first
		// one in case the values are so large they wrap and the second one
		// returns an incorrect result.

		// TESTED BY: fmt_epf_lionking_invaliddata_c01
		(this->offFAT > lenArchive) ||

		// TESTED BY: fmt_epf_lionking_invaliddata_c02, when stream::pos <= 32bit
		((this->offFAT + numFiles * EPF_FAT_ENTRY_LEN) > lenArchive)
	) {
		throw stream::error("header corrupted or file truncated");
	}
	this->psArchive->seekg(this->offFAT, stream::start);

	stream::pos offNext = EPF_FIRST_FILE_OFFSET;
	uint8_t flags;
	for (int i = 0; i < numFiles; i++) {
		FATEntry *fatEntry = new FATEntry();
		EntryPtr ep(fatEntry);

		fatEntry->iIndex = i;
		fatEntry->iOffset = offNext;
		fatEntry->lenHeader = 0;
		fatEntry->type = FILETYPE_GENERIC;
		fatEntry->fAttr = 0;
		fatEntry->bValid = true;

		// Read the data in from the FAT entry in the file
		this->psArchive
			>> nullPadded(fatEntry->strName, EPF_FILENAME_FIELD_LEN)
			>> u8(flags)
			>> u32le(fatEntry->iSize)
			>> u32le(fatEntry->iPrefilteredSize);

		if (flags & EPF_FAT_FLAG_COMPRESSED) {
			fatEntry->fAttr |= EA_COMPRESSED;
			fatEntry->filter = "lzw-epfs";
		}

		this->vcFAT.push_back(ep);
		offNext += fatEntry->iSize;
	}
	// TODO: hidden data after FAT until EOF?
}

EPFArchive::~EPFArchive()
	throw ()
{
}

int EPFArchive::getSupportedAttributes() const
	throw ()
{
	return EA_COMPRESSED;
}

EPFArchive::MetadataTypes EPFArchive::getMetadataList() const
	throw ()
{
	// TESTED BY: fmt_epf_lionking_get_metadata_description
	MetadataTypes m;
	m.push_back(Description);
	return m;
}

std::string EPFArchive::getMetadata(MetadataType item) const
	throw (stream::error)
{
	// TESTED BY: fmt_epf_lionking_get_metadata_description
	switch (item) {
		case Description: {
			stream::pos offDesc = this->getDescOffset();
			stream::len sizeDesc = this->offFAT - offDesc;
			std::string strDesc;
			if (sizeDesc) {
				this->psArchive->seekg(offDesc, stream::start);
				this->psArchive >> fixedLength(strDesc, sizeDesc);
				//psArchive->read(strDesc.c_str(), sizeDesc);
			}
			return strDesc;
		}
		default:
			assert(false);
			throw stream::error("unsupported metadata item");
	}
}

void EPFArchive::setMetadata(MetadataType item, const std::string& value)
	throw (stream::error)
{
	// TESTED BY: fmt_epf_lionking_set_metadata_description
	// TESTED BY: fmt_epf_lionking_new_to_initialstate
	switch (item) {
		case Description: {
			stream::pos offDesc = this->getDescOffset();
			stream::len sizeDesc = this->offFAT - offDesc;
			stream::delta sizeDelta = value.length() - sizeDesc;
			this->psArchive->seekp(offDesc, stream::start);
			if (sizeDelta < 0) {
				// TESTED BY: ?
				this->psArchive->remove(-sizeDelta);
			} else {
				// TESTED BY: ?
				this->psArchive->insert(sizeDelta);
			}
			this->psArchive << value; // TODO: confirm no terminating null
			this->offFAT += sizeDelta;
			this->updateFATOffset();
			break;
		}
		default:
			assert(false);
			throw stream::error("unsupported metadata item");
	}
	return;
}

void EPFArchive::updateFileName(const FATEntry *pid, const std::string& strNewName)
	throw (stream::error)
{
	// TESTED BY: fmt_epf_lionking_rename
	assert(strNewName.length() <= EPF_MAX_FILENAME_LEN);
	this->psArchive->seekp(this->offFAT + pid->iIndex * EPF_FAT_ENTRY_LEN, stream::start);
	this->psArchive << nullPadded(strNewName, EPF_FILENAME_FIELD_LEN);
	return;
}

void EPFArchive::updateFileOffset(const FATEntry *pid, stream::delta offDelta)
	throw (stream::error)
{
	return;
}

void EPFArchive::updateFileSize(const FATEntry *pid, stream::delta sizeDelta)
	throw (stream::error)
{
	// TESTED BY: fmt_epf_lionking_insert*
	// TESTED BY: fmt_epf_lionking_resize*

	this->offFAT += sizeDelta;

	this->psArchive->seekp(this->offFAT + pid->iIndex * EPF_FAT_ENTRY_LEN + EPF_FAT_FILESIZE_OFFSET, stream::start);
	this->psArchive
		<< u32le(pid->iSize)    // compressed size
		<< u32le(pid->iPrefilteredSize);   // decompressed size

	this->updateFATOffset();

	return;
}

FATArchive::FATEntry *EPFArchive::preInsertFile(const FATEntry *idBeforeThis, FATEntry *pNewEntry)
	throw (stream::error)
{
	// TESTED BY: fmt_epf_lionking_insert*
	assert(pNewEntry->strName.length() <= EPF_MAX_FILENAME_LEN);

	// Set the format-specific variables
	pNewEntry->lenHeader = 0;

	// Set the filter to use if the file should be compressed
	if (pNewEntry->fAttr & EA_COMPRESSED) {
		pNewEntry->filter = "lzw-epfs";
	}

	return pNewEntry;
}

void EPFArchive::postInsertFile(FATEntry *pNewEntry)
	throw (stream::error)
{
	this->offFAT += pNewEntry->iSize;

	this->psArchive->seekp(this->offFAT + pNewEntry->iIndex * EPF_FAT_ENTRY_LEN, stream::start);
	this->psArchive->insert(EPF_FAT_ENTRY_LEN);
	boost::to_upper(pNewEntry->strName);
	uint8_t flags = 0;
	if (pNewEntry->fAttr & EA_COMPRESSED) flags = 1;
	this->psArchive
		<< nullPadded(pNewEntry->strName, EPF_FILENAME_FIELD_LEN)
		<< u8(flags)  // 0 == uncompressed, 1 == compressed
		<< u32le(pNewEntry->iSize)  // compressed
		<< u32le(pNewEntry->iPrefilteredSize); // decompressed

	this->updateFATOffset();
	this->updateFileCount(this->vcFAT.size());

	return;
}

void EPFArchive::preRemoveFile(const FATEntry *pid)
	throw (stream::error)
{
	// TESTED BY: fmt_epf_lionking_remove*

	this->psArchive->seekp(this->offFAT + pid->iIndex * EPF_FAT_ENTRY_LEN, stream::start);
	this->psArchive->remove(EPF_FAT_ENTRY_LEN);

	this->offFAT -= pid->iSize;
	this->updateFATOffset();
	this->updateFileCount(this->vcFAT.size() - 1);

	return;
}

void EPFArchive::updateFileCount(uint16_t iNewCount)
	throw (stream::error)
{
	// TESTED BY: fmt_epf_lionking_insert*
	// TESTED BY: fmt_epf_lionking_remove*
	this->psArchive->seekp(EPF_FILECOUNT_POS, stream::start);
	this->psArchive << u16le(iNewCount);
	return;
}

void EPFArchive::updateFATOffset()
	throw (stream::error)
{
	// TESTED BY: fmt_epf_lionking_insert*
	// TESTED BY: fmt_epf_lionking_remove*

	this->psArchive->seekp(EPF_FAT_OFFSET_POS, stream::start);
	this->psArchive << u32le(this->offFAT);

	return;
}

stream::pos EPFArchive::getDescOffset() const
	throw (stream::error)
{
	stream::pos offDesc;
	if (this->vcFAT.size()) {
		EntryPtr lastFile = this->vcFAT.back();
		assert(lastFile);
		FATEntry *lastFATEntry = dynamic_cast<FATEntry *>(lastFile.get());
		offDesc = lastFATEntry->iOffset + lastFATEntry->iSize;
	} else {
		offDesc = EPF_FIRST_FILE_OFFSET;
	}
	return offDesc;
}

} // namespace gamearchive
} // namespace camoto
