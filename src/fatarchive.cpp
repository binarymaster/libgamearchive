/**
 * @file   fatarchive.cpp
 * @brief  Implementation of a FAT-style archive format.
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

#include <boost/bind.hpp>
#include <boost/algorithm/string.hpp>
#include <camoto/util.hpp> // createString

#include "fatarchive.hpp"

namespace camoto {
namespace gamearchive {

FATArchive::FATEntry::FATEntry()
{
}
FATArchive::FATEntry::~FATEntry()
{
}
std::string FATArchive::FATEntry::getContent() const
{
	std::ostringstream ss;
	ss << this->FileEntry::getContent()
		<< ";fatindex=" << iIndex
		<< ";offset=" << iOffset
		<< ";header=" << lenHeader
	;
	return ss.str();
}

Archive::EntryPtr getFileAt(const Archive::VC_ENTRYPTR& files, unsigned int index)
{
	for (Archive::VC_ENTRYPTR::const_iterator i = files.begin(); i != files.end(); i++) {
		FATArchive::FATEntry *pEntry = dynamic_cast<FATArchive::FATEntry *>(i->get());
		if (pEntry->iIndex == index) return *i;
	}
	return Archive::EntryPtr();
}

FATArchive::FATArchive(stream::inout_sptr psArchive, stream::pos offFirstFile,
	int lenMaxFilename)
	:	psArchive(new stream::seg()),
		offFirstFile(offFirstFile),
		lenMaxFilename(lenMaxFilename)
{
	assert(psArchive);

	this->psArchive->open(psArchive);
}

FATArchive::~FATArchive()
{
	// Can't flush here as it could throw stream::error and we have no way
	// of handling it.
	//this->flush(); // make sure it saves on close just in case
}

const FATArchive::VC_ENTRYPTR& FATArchive::getFileList() const
{
	return this->vcFAT;
}

FATArchive::EntryPtr FATArchive::find(const std::string& strFilename) const
{
	// TESTED BY: fmt_grp_duke3d_*
	for (VC_ENTRYPTR::const_iterator i = this->vcFAT.begin();
		i != this->vcFAT.end();
		i++
	) {
		const FATEntry *pFAT = dynamic_cast<const FATEntry *>(i->get());
		if (boost::iequals(pFAT->strName, strFilename)) {
			return *i;  // *i is the original shared_ptr
		}
	}
	return EntryPtr();
}

bool FATArchive::isValid(const EntryPtr id) const
{
	const FATEntry *id2 = dynamic_cast<const FATEntry *>(id.get());
	return ((id2) && (id2->bValid));
}

stream::inout_sptr FATArchive::open(const EntryPtr id)
{
	// TESTED BY: fmt_grp_duke3d_open

	// Make sure we're not trying to open a folder as a file
	//assert((id->fAttr & EA_FOLDER) == 0);
	// We can't do this because some folder formats have their FAT and
	// everything stored as a "file" in the parent archive, so the subfolder
	// code opens this file (even though it's flagged as a folder) and then
	// passes the data to the Archive.

	// We are casting away const here, but that's because we need to maintain
	// access to the EntryPtr, which we may need to "change" later (to update the
	// offset if another file gets inserted before it, i.e. any change would not
	// be visible externally.)
	FATEntryPtr pFAT = boost::dynamic_pointer_cast<FATEntry>(id);

	stream::sub_sptr psSub(new stream::sub());
	stream::fn_truncate fnTrunc = boost::bind(&FATArchive::resizeSubstream, this, pFAT, _1);
	psSub->open(
		this->psArchive,
		pFAT->iOffset + pFAT->lenHeader,
		pFAT->storedSize,
		fnTrunc
	);

	// Add it to the list of open files, in case we need to shift the substream
	// around later on as files are added/removed/resized.
	this->openFiles.insert(OPEN_FILE(
		pFAT,
		psSub
	));
	return psSub;
}

ArchivePtr FATArchive::openFolder(const Archive::EntryPtr id)
{
	// This function should only be called for folders (not files)
	assert(id->fAttr & EA_FOLDER);

	// Throw an exception if assertions have been disabled.
	throw stream::error("BUG: Archive format doesn't implement openFolder()");
}

FATArchive::EntryPtr FATArchive::insert(const EntryPtr idBeforeThis,
	const std::string& strFilename, stream::pos storedSize, std::string type, int attr
)
{
	// TESTED BY: fmt_grp_duke3d_insert2
	// TESTED BY: fmt_grp_duke3d_remove_insert
	// TESTED BY: fmt_grp_duke3d_insert_remove

	// Make sure filename is within the allowed limit
	if (
		(this->lenMaxFilename > 0) &&
		(strFilename.length() > this->lenMaxFilename)
	) {
		throw stream::error(createString("maximum filename length is "
			<< this->lenMaxFilename << " chars"));
	}

	FATEntry *pNewFile = this->createNewFATEntry();
	EntryPtr ep(pNewFile);

	pNewFile->strName = strFilename;
	pNewFile->storedSize = storedSize;
	pNewFile->realSize = storedSize; // default to no filter
	pNewFile->type = type;
	pNewFile->fAttr = attr;
	pNewFile->lenHeader = 0;
	pNewFile->bValid = false; // not yet valid

	// Figure out where the new file is going to go
	const FATEntry *pFATBeforeThis = NULL;
	if (this->isValid(idBeforeThis)) {
		// Insert at idBeforeThis
		// TESTED BY: fmt_grp_duke3d_insert_mid
		pFATBeforeThis = dynamic_cast<const FATEntry *>(idBeforeThis.get());
		assert(pFATBeforeThis);
		pNewFile->iOffset = pFATBeforeThis->iOffset;
		pNewFile->iIndex = pFATBeforeThis->iIndex;
	} else {
		// Append to end of archive
		// TESTED BY: fmt_grp_duke3d_insert_end
		if (this->vcFAT.size()) {
			const FATEntry *pFATAfterThis = dynamic_cast<const FATEntry *>(this->vcFAT.back().get());
			assert(pFATAfterThis);
			pNewFile->iOffset = pFATAfterThis->iOffset
				+ pFATAfterThis->lenHeader + pFATAfterThis->storedSize;
			pNewFile->iIndex = pFATAfterThis->iIndex + 1;
		} else {
			// There are no files in the archive
			pNewFile->iOffset = this->offFirstFile;
			pNewFile->iIndex = 0;
		}
	}

	// Add the file's entry from the FAT.  May throw (e.g. filename too long),
	// archive should be left untouched in this case.
	FATEntry *returned = this->preInsertFile(pFATBeforeThis, pNewFile);
	if (returned != pNewFile) {
		ep.reset(returned);
		pNewFile = returned;
	}

	if (!pNewFile->filter.empty()) {
		// The format handler wants us to apply a filter to this file.
	}

	// Now it's mostly valid.  Really this is here so that it's invalid during
	// preInsertFile(), so any calls in there to shiftFiles() will ignore the
	// new file.  But we're about to call shiftFiles() now, and we need the file
	// to be marked valid otherwise it won't be skipped/ignored.
	pNewFile->bValid = true;

	if (this->isValid(idBeforeThis)) {
		// Update the offsets of any files located after this one (since they will
		// all have been shifted forward to make room for the insert.)
		this->shiftFiles(
			pNewFile,
			pNewFile->iOffset + pNewFile->lenHeader,
			pNewFile->storedSize,
			1
		);

		// Add the new file to the vector now all the existing offsets have been
		// updated.
		// TESTED BY: fmt_grp_duke3d_insert_mid
		VC_ENTRYPTR::iterator itBeforeThis = std::find(this->vcFAT.begin(),
			this->vcFAT.end(), idBeforeThis);
		assert(itBeforeThis != this->vcFAT.end());
		this->vcFAT.insert(itBeforeThis, ep);
	} else {
		// TESTED BY: fmt_grp_duke3d_insert_end
		this->vcFAT.push_back(ep);
	}

	// Insert space for the file's data into the archive.  If there is a header
	// (e.g. embedded FAT) then preInsertFile() will have inserted space for
	// this and written the data, so our insert should start just after the
	// header.
	this->psArchive->seekp(pNewFile->iOffset + pNewFile->lenHeader, stream::start);
	this->psArchive->insert(pNewFile->storedSize);

	this->postInsertFile(pNewFile);

	return ep;
}

void FATArchive::remove(EntryPtr id)
{
	// TESTED BY: fmt_grp_duke3d_remove
	// TESTED BY: fmt_grp_duke3d_remove2
	// TESTED BY: fmt_grp_duke3d_remove_insert
	// TESTED BY: fmt_grp_duke3d_insert_remove

	// Make sure the caller doesn't try to remove something that doesn't exist!
	assert(this->isValid(id));

	FATEntry *pFATDel = dynamic_cast<FATEntry *>(id.get());
	assert(pFATDel);

	// Remove the file's entry from the FAT
	this->preRemoveFile(pFATDel);

	// Remove the entry from the vector
	VC_ENTRYPTR::iterator itErase = std::find(this->vcFAT.begin(), this->vcFAT.end(), id);
	assert(itErase != this->vcFAT.end());
	this->vcFAT.erase(itErase);

	// Update the offsets of any files located after this one (since they will
	// all have been shifted back to fill the gap made by the removal.)
	this->shiftFiles(
		pFATDel,
		pFATDel->iOffset,
		-((stream::delta)pFATDel->storedSize + (stream::delta)pFATDel->lenHeader),
		-1
	);

	// Remove the file's data from the archive
	this->psArchive->seekp(pFATDel->iOffset, stream::start);
	this->psArchive->remove(pFATDel->storedSize + pFATDel->lenHeader);

	// Mark it as invalid in case some other code is still holding on to it.
	pFATDel->bValid = false;

	this->postRemoveFile(pFATDel);

	return;
}

void FATArchive::rename(EntryPtr id, const std::string& strNewName)
{
	// TESTED BY: fmt_grp_duke3d_rename
	assert(this->isValid(id));
	FATEntry *pFAT = dynamic_cast<FATEntry *>(id.get());

	// Make sure filename is within the allowed limit
	if (
		(this->lenMaxFilename > 0) &&
		(strNewName.length() > this->lenMaxFilename)
	) {
		throw stream::error(createString("maximum filename length is "
			<< this->lenMaxFilename << " chars"));
	}

	this->updateFileName(pFAT, strNewName);
	pFAT->strName = strNewName;
	return;
}

void FATArchive::move(const EntryPtr idBeforeThis, EntryPtr id)
{
	// Open the file we want to move
	stream::inout_sptr src(this->open(id));
	assert(src);

	// Insert a new file at the destination index
	EntryPtr n = this->insert(idBeforeThis, id->strName, id->storedSize,
		id->type, id->fAttr);
	assert(n->bValid);

	if (n->filter.compare(id->filter) != 0) {
		this->remove(n);
		throw stream::error("Cannot move file to this position (filter change)"
			" - try removing and then adding it instead");
	}

	stream::inout_sptr dst(this->open(n));
	assert(dst);

	// Copy the data into the new file's position
	stream::copy(dst, src);
	dst->flush();

	// If there's a filter set then bring the unfiltered size across too.
	if (!n->filter.empty()) {
		this->resize(n, n->storedSize, id->realSize);
	}

	this->remove(id);

	return;
}

void FATArchive::resize(EntryPtr id, stream::len newStoredSize,
	stream::len newRealSize)
{
	assert(this->isValid(id));
	stream::delta iDelta = newStoredSize - id->storedSize;
	FATEntry *pFAT = dynamic_cast<FATEntry *>(id.get());

	stream::len oldStoredSize = pFAT->storedSize;
	stream::len oldRealSize = pFAT->realSize;
	pFAT->storedSize = newStoredSize;
	pFAT->realSize = newRealSize;

	try {
		// Update the FAT with the file's new sizes
		this->updateFileSize(pFAT, iDelta);
	} catch (stream::error) {
		// Undo and abort the resize
		pFAT->storedSize = oldStoredSize;
		pFAT->realSize = oldRealSize;
		throw;
	}

	// Add or remove the data in the underlying stream
	stream::pos iStart;
	if (iDelta > 0) { // inserting data
		// TESTED BY: fmt_grp_duke3d_resize_larger
		iStart = pFAT->iOffset + pFAT->lenHeader + oldStoredSize;
		this->psArchive->seekp(iStart, stream::start);
		this->psArchive->insert(iDelta);
	} else if (iDelta < 0) { // removing data
		// TESTED BY: fmt_grp_duke3d_resize_smaller
		iStart = pFAT->iOffset + pFAT->lenHeader + newStoredSize;
		this->psArchive->seekp(iStart, stream::start);
		this->psArchive->remove(-iDelta);
	} else if (pFAT->realSize == newRealSize) {
		// Not resizing the internal size, and the external/real size
		// hasn't changed either, so nothing to do.
		return;
	}

	if (iDelta != 0) {
		// The internal file size is changing, so adjust the offsets etc. of the
		// rest of the files in the archive, including any open streams.
		this->shiftFiles(pFAT, iStart, iDelta, 0);

		// Resize any open substreams for this file
		for (OPEN_FILES::iterator i = this->openFiles.begin();
			i != this->openFiles.end();
			i++
		) {
			if (i->first.get() == pFAT) {
				if (stream::sub_sptr sub = i->second.lock()) {
					sub->resize(newStoredSize);
					// no break, could be multiple opens for same entry
				}
			}
		}
	} // else only realSize changed

	return;
}

void FATArchive::flush()
{
	// Write out to the underlying stream
	this->psArchive->flush();
	return;
}

int FATArchive::getSupportedAttributes() const
{
	return 0;
}

void FATArchive::shiftFiles(const FATEntry *fatSkip, stream::pos offStart,
	stream::delta deltaOffset, int deltaIndex)
{
	for (VC_ENTRYPTR::iterator i = this->vcFAT.begin(); i != this->vcFAT.end(); i++) {
		FATEntry *pFAT = dynamic_cast<FATEntry *>(i->get());
		if (this->entryInRange(pFAT, offStart, fatSkip)) {
			// This file is located after the one we're deleting, so tweak its offset
			pFAT->iOffset += deltaOffset;

			// We have to update the index first, as this is used when inserting
			// files, and it's called *after* the FAT has been updated on-disk.  So
			// the index needs to be adjusted before any further on-disk updates to
			// ensure the right place in the file gets changed.
			pFAT->iIndex += deltaIndex;

			this->updateFileOffset(pFAT, deltaOffset);
		}
	}

	// Relocate any open substreams
	bool clean = false;
	for (OPEN_FILES::iterator i = this->openFiles.begin();
		i != this->openFiles.end();
		i++
	) {
		if (i->first->bValid) {
			if (this->entryInRange(i->first.get(), offStart, fatSkip)) {
				if (stream::sub_sptr sub = i->second.lock()) {
					sub->relocate(deltaOffset);
				}
			}
		} else clean = true;
	}

	// If one substream has closed, clean up
	if (clean) this->cleanOpenSubstreams();

	return;
}

FATArchive::FATEntry *FATArchive::preInsertFile(const FATEntry *idBeforeThis,
	FATEntry *pNewEntry)
{
	// No-op default
	return NULL;
}

void FATArchive::postInsertFile(FATEntry *pNewEntry)
{
	// No-op default
}

void FATArchive::preRemoveFile(const FATEntry *pid)
{
	// No-op default
}

void FATArchive::postRemoveFile(const FATEntry *pid)
{
	// No-op default
}

FATArchive::FATEntry *FATArchive::createNewFATEntry()
{
	return new FATEntry();
}

void FATArchive::cleanOpenSubstreams()
{
	bool clean;
	do {
		clean = true;
		// Run through the list looking for the first expired item
		for (OPEN_FILES::iterator i = this->openFiles.begin();
			i != this->openFiles.end();
			i++
		) {
			if (i->second.expired()) {
				// Found one so remove it and restart the search (since removing an
				// item invalidates the iterator.)
				this->openFiles.erase(i);
				clean = false;
				break;
			}
		}
	} while (!clean);

	return;
}

bool FATArchive::entryInRange(const FATEntry *fat, stream::pos offStart,
	const FATEntry *fatSkip)
{
	// Don't move any files earlier than the start of the shift block.
	if (fat->iOffset < offStart) return false;

	// If we have a valid item to skip (an invalid item is given during insert,
	// before the skip item has been fully inserted.)
	if ((fatSkip) && (fatSkip->bValid)) {

		// Don't move the item we're skipping.
		if (fat == fatSkip) return false;
		// Can't use index for comparison here as this function is called during
		// file insertion, and then we have two files with the exact same index and
		// offset.
		// if (fat->iIndex == fatSkip->iIndex) return false;

		if (
			// If it's a zero-length file...
			(fat->storedSize == 0)
			// ...starting at the same location as the skip file...
			&& (fat->iOffset == fatSkip->iOffset)
			// ...but appearing before it in the index order...
			&& (fat->iIndex < fatSkip->iIndex)
		) {
			// ...then don't move it.
			return false;
		}
	}

	return true;
}

void FATArchive::resizeSubstream(FATEntryPtr id, stream::len newSize)
{
	// An open substream belonging to file entry 'id' wants to be resized.

	// Resize the file in the archive.  This function will also tell the
	// substream it can now write to a larger area.

	// We are updating both the stored (in-archive) and the real (extracted)
	// sizes, to handle the case where no filters are used and the sizes are
	// the same.  When filters are in use, the flush() function that writes
	// the filtered data out should call us first, then call the archive's
	// resize() function with the correct real/extracted size.
	//this->resize(id, newSize, newSize);
	stream::len newRealSize;
	if (id->fAttr & EA_COMPRESSED) {
		// We're compressed, so the real and stored sizes are both valid
		newRealSize = id->realSize;
	} else {
		// We're not compressed, so the real size won't be updated by a filter,
		// so we need to update it here.
		newRealSize = newSize;
	}
	this->resize(id, newSize, newRealSize);
	return;
}

} // namespace gamearchive
} // namespace camoto
