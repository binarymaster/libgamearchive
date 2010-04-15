/*
 * test-archive.hpp - generic test code for Archive class descendents.
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

#include <boost/test/unit_test.hpp>

#include <boost/algorithm/string.hpp> // for case-insensitive string compare
#include <boost/iostreams/copy.hpp>
#include <camoto/gamearchive.hpp>
#include <iostream>
#include <iomanip>

#include "tests.hpp"

// Local headers that will not be installed
#include "../src/segmented_stream.hpp"

namespace ga = camoto::gamearchive;

// Defines to allow code reuse
#define COMBINE_CLASSNAME_EXP(c, n)  c ## _ ## n
#define COMBINE_CLASSNAME(c, n)  COMBINE_CLASSNAME_EXP(c, n)

#define TEST_VAR(n)        COMBINE_CLASSNAME(ARCHIVE_CLASS, n)
#define TEST_NAME(n)       TEST_VAR(n)
#define TEST_RESULT(n)     TEST_VAR(n ## _result)

#define FIXTURE_NAME       TEST_VAR(sample)
#define SUITE_NAME         TEST_VAR(suite)
#define INITIALSTATE_NAME  TEST_VAR(initialstate)

// Allow a string constant to be passed around with embedded nulls
#define makeString(x)  std::string((x), sizeof((x)) - 1)

struct FIXTURE_NAME: public default_sample {

	typedef boost::shared_ptr<std::stringstream> sstr_ptr;

	sstr_ptr psstrBase;
	void *_do; // unused var, but allows a statement to run in constructor init
	camoto::iostream_sptr psBase;
	boost::shared_ptr<ga::Archive> pArchive;
	ga::MP_SUPPDATA suppData;
	std::map<ga::E_SUPPTYPE, sstr_ptr> suppBase;

	FIXTURE_NAME() :
		psstrBase(new std::stringstream),
		_do((*this->psstrBase) << makeString(INITIALSTATE_NAME)),
		psBase(this->psstrBase)
	{
		#ifdef HAS_FAT
		{
			boost::shared_ptr<std::stringstream> suppSS(new std::stringstream);
			suppSS->exceptions(std::ios::badbit | std::ios::failbit | std::ios::eofbit);
			(*suppSS) << makeString(TEST_VAR(FAT_initialstate));
			camoto::iostream_sptr suppStream(suppSS);
			this->suppData[ga::EST_FAT] = suppStream;
			this->suppBase[ga::EST_FAT] = suppSS;
		}
		#endif

		BOOST_REQUIRE_NO_THROW(
			this->psstrBase->exceptions(std::ios::badbit | std::ios::failbit | std::ios::eofbit);

			boost::shared_ptr<ga::Manager> pManager(ga::getManager());
			ga::Manager::arch_sptr pTestType(pManager->getArchiveTypeByCode(ARCHIVE_TYPE));
			this->pArchive = boost::shared_ptr<ga::Archive>(pTestType->open(psBase, this->suppData));
			BOOST_REQUIRE_MESSAGE(this->pArchive, "Could not create archive class");
		);
	}

	FIXTURE_NAME(const char *data, ga::MP_SUPPDATA& suppData) :
		psstrBase(new std::stringstream),
		_do((*this->psstrBase) << data),
		psBase(this->psstrBase),
		suppData(suppData)
	{
		BOOST_REQUIRE_NO_THROW(
			this->psstrBase->exceptions(std::ios::badbit | std::ios::failbit | std::ios::eofbit);

			boost::shared_ptr<ga::Manager> pManager(ga::getManager());
			ga::Manager::arch_sptr pTestType(pManager->getArchiveTypeByCode(ARCHIVE_TYPE));
			this->pArchive = boost::shared_ptr<ga::Archive>(pTestType->open(psBase, this->suppData));
			BOOST_REQUIRE_MESSAGE(this->pArchive, "Could not create archive class");
		);
	}

	boost::test_tools::predicate_result is_equal(const std::string& strExpected)
	{
		// Flush out any changes before we perform the check
		BOOST_CHECK_NO_THROW(
			this->pArchive->flush()
		);

		return this->default_sample::is_equal(strExpected, psstrBase->str());
	}

	boost::test_tools::predicate_result is_supp_equal(ga::E_SUPPTYPE type, const std::string& strExpected)
	{
		// Do we need to flush any changes here?  Hopefully not if the
		// main archive was flushed first.
		return this->default_sample::is_equal(strExpected, this->suppBase[type]->str());
	}

};
BOOST_FIXTURE_TEST_SUITE(SUITE_NAME, FIXTURE_NAME)

// Make sure a corrupted file doesn't segfault
BOOST_AUTO_TEST_CASE(TEST_NAME(open_invalid))
{
	BOOST_TEST_MESSAGE("Opening invalid archive");

	// Find the archive handler
	boost::shared_ptr<ga::Manager> pManager(ga::getManager());
	ga::Manager::arch_sptr pTestType(pManager->getArchiveTypeByCode(ARCHIVE_TYPE));

	// Prepare an invalid archive
	boost::shared_ptr<std::stringstream> psstrBase(new std::stringstream);
	(*psstrBase) << makeString(TEST_RESULT(invalidcontent));
	camoto::iostream_sptr psBase(psstrBase);

	ga::MP_SUPPDATA suppData;
	#ifdef HAS_FAT
	{
		boost::shared_ptr<std::stringstream> suppSS(new std::stringstream);
		suppSS->exceptions(std::ios::badbit | std::ios::failbit | std::ios::eofbit);
		(*suppSS) << makeString(TEST_RESULT(FAT_invalidcontent));
		camoto::iostream_sptr suppStream(suppSS);
		suppData[ga::EST_FAT] = suppStream;
	}
	#endif

	// Try to open the invalid file.  This will result in an attempt to allocate
	// ~16GB of memory.  This is an error condition (the file is corrupt/invalid)
	// but it may succeed on a system with a lot of RAM!
	BOOST_CHECK_THROW(
		boost::shared_ptr<ga::Archive> pArchive(pTestType->open(psBase, suppData)),
		std::ios::failure
	);
}

BOOST_AUTO_TEST_CASE(TEST_NAME(open))
{
	BOOST_TEST_MESSAGE("Opening file in archive");

	// Find the file we're going to open
	ga::Archive::EntryPtr ep = pArchive->find("ONE.DAT");

	// Make sure we found it
	BOOST_REQUIRE_MESSAGE(pArchive->isValid(ep),
		"Couldn't find ONE.DAT in sample archive");

	// Open it
	camoto::iostream_sptr pfsIn(pArchive->open(ep));
	std::stringstream out;
	out.exceptions(std::ios::badbit | std::ios::failbit | std::ios::eofbit);

	// Make sure the file opens at the start
	BOOST_REQUIRE_EQUAL(pfsIn->tellg(), 0);

	// Copy it into the stringstream
	boost::iostreams::copy(*pfsIn, out);

	BOOST_CHECK_MESSAGE(
		default_sample::is_equal(makeString(
			"This is one.dat"
		), out.str()),
		"Error opening file or wrong file opened"
	);
}

BOOST_AUTO_TEST_CASE(TEST_NAME(rename))
{
	BOOST_TEST_MESSAGE("Renaming file inside archive");

	// Find the file we're going to rename
	ga::Archive::EntryPtr ep = pArchive->find("ONE.DAT");

	// Make sure we found it
	BOOST_REQUIRE_MESSAGE(pArchive->isValid(ep),
		"Couldn't find ONE.DAT in sample archive");

	// Swap the file positions
	pArchive->rename(ep, "HELLO.BIN");

	BOOST_CHECK_MESSAGE(
		is_equal(makeString(TEST_RESULT(rename))),
		"Error renaming file"
	);

#ifdef HAS_FAT
	BOOST_CHECK_MESSAGE(
		is_supp_equal(ga::EST_FAT, makeString(TEST_RESULT(FAT_rename))),
		"Error renaming file"
	);
#endif
}

BOOST_AUTO_TEST_CASE(TEST_NAME(insert_long))
{
	BOOST_TEST_MESSAGE("Inserting file with name too long");

	// Find the file we're going to insert before
	ga::Archive::EntryPtr epb = pArchive->find("ONE.DAT");

	// Make sure we found it
	BOOST_REQUIRE_MESSAGE(pArchive->isValid(epb),
		"Couldn't find ONE.DAT in sample archive");

	char name[MAX_FILENAME_LEN + 2];
	memset(name, 65, MAX_FILENAME_LEN + 1);
	name[MAX_FILENAME_LEN + 1] = 0;

	BOOST_CHECK_THROW(
		ga::Archive::EntryPtr ep = pArchive->insert(epb, name, 5),
		std::ios::failure
	);

	BOOST_CHECK_MESSAGE(
		is_equal(makeString(INITIALSTATE_NAME)),
		"File corrupted after failed insert"
	);

	memset(name, 65, MAX_FILENAME_LEN);
	name[MAX_FILENAME_LEN] = 0;

	BOOST_CHECK_NO_THROW(
		ga::Archive::EntryPtr ep = pArchive->insert(ga::Archive::EntryPtr(), name, 5)
	);

}

BOOST_AUTO_TEST_CASE(TEST_NAME(insert_end))
{
	BOOST_TEST_MESSAGE("Inserting file into archive");

	// Insert the file
	ga::Archive::EntryPtr ep = pArchive->insert(ga::Archive::EntryPtr(), "THREE.DAT", 17);

	// Make sure it went in ok
	BOOST_REQUIRE_MESSAGE(pArchive->isValid(ep),
		"Couldn't create new file in sample archive");

	// Open it
	boost::shared_ptr<std::iostream> pfsNew(pArchive->open(ep));
	pfsNew->write("This is three.dat", 17);
	pfsNew->flush();

	BOOST_CHECK_MESSAGE(
		is_equal(makeString(TEST_RESULT(insert_end))),
		"Error inserting file at end of archive"
	);
}

BOOST_AUTO_TEST_CASE(TEST_NAME(insert_mid))
{
	BOOST_TEST_MESSAGE("Inserting file into middle of archive");

	// Find the file we're going to insert before
	ga::Archive::EntryPtr idBefore = pArchive->find("TWO.DAT");

	// Make sure we found it
	BOOST_REQUIRE_MESSAGE(pArchive->isValid(idBefore),
		"Couldn't find TWO.DAT in sample archive");

	// Insert the file
	ga::Archive::EntryPtr ep = pArchive->insert(idBefore, "THREE.DAT", 17);

	// Make sure it went in ok
	BOOST_REQUIRE_MESSAGE(pArchive->isValid(ep),
		"Couldn't insert new file in sample archive");

	// Open it
	boost::shared_ptr<std::iostream> pfsNew(pArchive->open(ep));
	pfsNew->write("This is three.dat", 17);
	pfsNew->flush();

	BOOST_CHECK_MESSAGE(
		is_equal(makeString(TEST_RESULT(insert_mid))),
		"Error inserting file in middle of archive"
	);
}

BOOST_AUTO_TEST_CASE(TEST_NAME(insert2))
{
	BOOST_TEST_MESSAGE("Inserting multiple files");

	// Find the file we're going to insert before
	ga::Archive::EntryPtr idBefore = pArchive->find("TWO.DAT");

	// Make sure we found it
	BOOST_REQUIRE_MESSAGE(pArchive->isValid(idBefore),
		"Couldn't find TWO.DAT in sample archive");

	// Insert the file
	ga::Archive::EntryPtr ep1 = pArchive->insert(idBefore, "THREE.DAT", 17);

	// Make sure it went in ok
	BOOST_REQUIRE_MESSAGE(pArchive->isValid(ep1),
		"Couldn't insert first new file in sample archive");

	// Open it
	boost::shared_ptr<std::iostream> pfsNew1(pArchive->open(ep1));
	pfsNew1->write("This is three.dat", 17);
	pfsNew1->flush();

	// Find the file we're going to insert before (since the previous insert
	// invalidated all EntryPtrs.)
	idBefore = pArchive->find("TWO.DAT");

	// Make sure we found it
	BOOST_REQUIRE_MESSAGE(pArchive->isValid(idBefore),
		"Couldn't find TWO.DAT in sample archive after first insert");

	// Insert the file
	ga::Archive::EntryPtr ep2 = pArchive->insert(idBefore, "FOUR.DAT", 16);

	// Make sure it went in ok
	BOOST_REQUIRE_MESSAGE(pArchive->isValid(ep2),
		"Couldn't insert second new file in sample archive");

	boost::shared_ptr<std::iostream> pfsNew2(pArchive->open(ep2));
	pfsNew2->write("This is four.dat", 16);
	pfsNew2->flush();

	BOOST_CHECK_MESSAGE(
		is_equal(makeString(TEST_RESULT(insert2))),
		"Error inserting two files"
	);
}

BOOST_AUTO_TEST_CASE(TEST_NAME(remove))
{
	BOOST_TEST_MESSAGE("Removing file from archive");

	// Find the file we're going to remove
	ga::Archive::EntryPtr ep = pArchive->find("ONE.DAT");

	// Make sure we found it
	BOOST_REQUIRE_MESSAGE(pArchive->isValid(ep),
		"Couldn't find ONE.DAT in sample archive");

	// Remove it
	pArchive->remove(ep);

	BOOST_CHECK_MESSAGE(
		is_equal(makeString(TEST_RESULT(remove))),
		"Error removing file"
	);
}

BOOST_AUTO_TEST_CASE(TEST_NAME(remove2))
{
	BOOST_TEST_MESSAGE("Removing multiple files from archive");

	// Find the files we're going to remove
	ga::Archive::EntryPtr ep1 = pArchive->find("ONE.DAT");
	ga::Archive::EntryPtr ep2 = pArchive->find("TWO.DAT");

	// Make sure we found it
	BOOST_REQUIRE_MESSAGE(pArchive->isValid(ep1),
		"Couldn't find ONE.DAT in sample archive");
	BOOST_REQUIRE_MESSAGE(pArchive->isValid(ep2),
		"Couldn't find TWO.DAT in sample archive");

	// Remove it
	pArchive->remove(ep1);
	pArchive->remove(ep2);

	BOOST_CHECK_MESSAGE(
		is_equal(makeString(TEST_RESULT(remove2))),
		"Error removing multiple files"
	);
}

BOOST_AUTO_TEST_CASE(TEST_NAME(insert_remove))
{
	BOOST_TEST_MESSAGE("Insert then remove file from archive");

	// Find the file we're going to insert before
	ga::Archive::EntryPtr idBefore = pArchive->find("TWO.DAT");

	// Make sure we found it
	BOOST_REQUIRE_MESSAGE(pArchive->isValid(idBefore),
		"Couldn't find TWO.DAT in sample archive");

	// Insert the file
	ga::Archive::EntryPtr ep = pArchive->insert(idBefore, "THREE.DAT", 17);

	// Make sure it went in ok
	BOOST_REQUIRE_MESSAGE(pArchive->isValid(ep),
		"Couldn't insert new file in sample archive");

	// Open it
	boost::shared_ptr<std::iostream> pfsNew(pArchive->open(ep));
	pfsNew->write("This is three.dat", 17);
	pfsNew->flush();

	// Find the file we're going to remove
	ga::Archive::EntryPtr ep2 = pArchive->find("ONE.DAT");

	// Make sure we found it
	BOOST_REQUIRE_MESSAGE(pArchive->isValid(ep2),
		"Couldn't find ONE.DAT in sample archive");

	// Remove it
	pArchive->remove(ep2);

	BOOST_CHECK_MESSAGE(
		is_equal(makeString(TEST_RESULT(insert_remove))),
		"Error inserting then removing file"
	);
}

BOOST_AUTO_TEST_CASE(TEST_NAME(remove_insert))
{
	BOOST_TEST_MESSAGE("Remove then insert file from archive");

	// Find the file we're going to remove
	ga::Archive::EntryPtr ep2 = pArchive->find("ONE.DAT");

	// Make sure we found it
	BOOST_REQUIRE_MESSAGE(pArchive->isValid(ep2),
		"Couldn't find ONE.DAT in sample archive");

	// Remove it
	pArchive->remove(ep2);

	// Find the file we're going to insert before
	ga::Archive::EntryPtr idBefore = pArchive->find("TWO.DAT");

	// Make sure we found it
	BOOST_REQUIRE_MESSAGE(pArchive->isValid(idBefore),
		"Couldn't find TWO.DAT in sample archive");

	// Insert the file
	ga::Archive::EntryPtr ep = pArchive->insert(idBefore, "THREE.DAT", 17);

	// Make sure it went in ok
	BOOST_REQUIRE_MESSAGE(pArchive->isValid(ep),
		"Couldn't insert new file in sample archive");

	// Open it
	boost::shared_ptr<std::iostream> pfsNew(pArchive->open(ep));
	pfsNew->write("This is three.dat", 17);
	pfsNew->flush();

	BOOST_CHECK_MESSAGE(
		is_equal(makeString(TEST_RESULT(remove_insert))),
		"Error removing then inserting file"
	);
}

BOOST_AUTO_TEST_CASE(TEST_NAME(move))
{
	BOOST_TEST_MESSAGE("Moving file inside archive");

	// Find the file we're going to move
	ga::Archive::EntryPtr ep1 = pArchive->find("ONE.DAT");
	ga::Archive::EntryPtr ep2 = pArchive->find("TWO.DAT");

	// Make sure we found it
	BOOST_REQUIRE_MESSAGE(pArchive->isValid(ep1),
		"Couldn't find ONE.DAT in sample archive");
	BOOST_REQUIRE_MESSAGE(pArchive->isValid(ep2),
		"Couldn't find TWO.DAT in sample archive");

	// Swap the file positions
	pArchive->move(ep1, ep2);

	BOOST_CHECK_MESSAGE(
		is_equal(makeString(TEST_RESULT(move))),
		"Error moving file"
	);
}

BOOST_AUTO_TEST_CASE(TEST_NAME(resize_larger))
{
	BOOST_TEST_MESSAGE("Enlarging a file inside the archive");

	// Find the file we're going to resize
	ga::Archive::EntryPtr ep = pArchive->find("ONE.DAT");

	// Make sure we found it
	BOOST_REQUIRE_MESSAGE(pArchive->isValid(ep),
		"Couldn't find ONE.DAT in sample archive");

	// Swap the file positions
	pArchive->resize(ep, 20);

	BOOST_CHECK_MESSAGE(
		is_equal(makeString(TEST_RESULT(resize_larger))),
		"Error enlarging a file"
	);
}

BOOST_AUTO_TEST_CASE(TEST_NAME(resize_smaller))
{
	BOOST_TEST_MESSAGE("Shrink a file inside the archive");

	// Find the file we're going to resize
	ga::Archive::EntryPtr ep = pArchive->find("ONE.DAT");

	// Make sure we found it
	BOOST_REQUIRE_MESSAGE(pArchive->isValid(ep),
		"Couldn't find ONE.DAT in sample archive");

	// Swap the file positions
	pArchive->resize(ep, 10);

	BOOST_CHECK_MESSAGE(
		is_equal(makeString(TEST_RESULT(resize_smaller))),
		"Error shrinking a file"
	);
}

BOOST_AUTO_TEST_CASE(TEST_NAME(resize_write))
{
	BOOST_TEST_MESSAGE("Enlarging a file inside the archive");

	// Find the file we're going to resize
	ga::Archive::EntryPtr ep = pArchive->find("ONE.DAT");

	// Make sure we found it
	BOOST_REQUIRE_MESSAGE(pArchive->isValid(ep),
		"Couldn't find ONE.DAT in sample archive");

	// Swap the file positions
	pArchive->resize(ep, 23);

	boost::shared_ptr<std::iostream> pfsNew(pArchive->open(ep));
	pfsNew->write("Now resized to 23 chars", 23);
	pfsNew->flush();

	BOOST_CHECK_MESSAGE(
		is_equal(makeString(TEST_RESULT(resize_write))),
		"Error enlarging a file"
	);

	// Open the file following it to make sure it was moved out of the way

	// Find the file we're going to open
	ga::Archive::EntryPtr ep2 = pArchive->find("TWO.DAT");

	// Make sure we found it
	BOOST_REQUIRE_MESSAGE(pArchive->isValid(ep2),
		"Couldn't find TWO.DAT in sample archive");

	// Open it
	camoto::iostream_sptr pfsIn(pArchive->open(ep2));
	std::stringstream out;
	out.exceptions(std::ios::badbit | std::ios::failbit | std::ios::eofbit);

	// Copy it into the stringstream
	boost::iostreams::copy(*pfsIn, out);

	BOOST_CHECK_MESSAGE(
		default_sample::is_equal(makeString(
			"This is two.dat"
		), out.str()),
		"Unrelated file was corrupted after file resize operation"
	);

}

BOOST_AUTO_TEST_SUITE_END()
