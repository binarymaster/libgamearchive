/*
 * fmt-res-stellar7.cpp - Implementation of Duke Nukem 3D Group Files (.res)
 *
 * Copyright (C) 2010 Adam Nielsen <malvineous@shikadi.net>
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

#ifndef _CAMOTO_FMT_RES_STELLAR7_HPP_
#define _CAMOTO_FMT_RES_STELLAR7_HPP_

#include <camoto/gamearchive.hpp>

#include "fatarchive.hpp"
#include "subdirarchive.hpp"

namespace camoto {
namespace gamearchive {

class RESType: virtual public ArchiveType {

	public:

		RESType()
			throw ();

		virtual ~RESType()
			throw ();

		virtual std::string getArchiveCode() const
			throw ();

		virtual std::string getFriendlyName() const
			throw ();

		virtual std::vector<std::string> getFileExtensions() const
			throw ();

		virtual std::vector<std::string> getGameList() const
			throw ();

		virtual E_CERTAINTY isInstance(iostream_sptr fsArchive) const
			throw (std::ios::failure);

		// newArchive is not overridden as an entirely blank file is valid

		virtual ArchivePtr open(iostream_sptr fsArchive, MP_SUPPDATA& suppData) const
			throw (std::ios::failure);

		virtual MP_SUPPLIST getRequiredSupps(const std::string& filenameArchive) const
			throw ();

};

class RESArchiveFolder: virtual public FATArchive, virtual public ArchiveWithFolders {

	protected:

		struct RESEntry: virtual public FATEntry, virtual public SubdirEntry {
		};

	public:
		RESArchiveFolder(iostream_sptr psArchive)
			throw (std::ios::failure);

		virtual ~RESArchiveFolder()
			throw ();

		// As per Archive (see there for docs)

		virtual void rename(EntryPtr& id, const std::string& strNewName)
			throw (std::ios_base::failure);

		// As per ArchiveWithFolders (see there for docs)

		virtual ArchivePtr openFolder(const EntryPtr& id)
			throw (std::ios::failure);

		// As per FATArchive (see there for docs)

		virtual void updateFileOffset(const FATEntry *pid)
			throw (std::ios::failure);

		virtual void updateFileSize(const FATEntry *pid)
			throw (std::ios_base::failure);

		void insertFATEntry(const FATEntry *idBeforeThis, FATEntry *pNewEntry)
			throw (std::ios_base::failure);

		void removeFATEntry(const FATEntry *pid)
			throw (std::ios_base::failure);

};

} // namespace gamearchive
} // namespace camoto

#endif // _CAMOTO_FMT_RES_STELLAR7_HPP_