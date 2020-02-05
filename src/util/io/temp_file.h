/****
DIAMOND protein aligner
Copyright (C) 2013-2018 Benjamin Buchfink <buchfink@gmail.com>

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
****/

#ifndef TEMP_FILE_H_
#define TEMP_FILE_H_

#include <utility>
#include "output_file.h"

struct TempFile : public OutputFile
{

	TempFile();
	virtual void finalize() override {}
	static std::string get_temp_dir();
	static unsigned n;
	static uint64_t hash_key;
	bool unlinked;

private:

#ifdef _MSC_VER
	std::string init();
#else
	std::pair<std::string, int> init();
#endif

};

#endif /* TEMP_FILE_H_ */
