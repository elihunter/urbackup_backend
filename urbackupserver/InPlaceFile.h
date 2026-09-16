#pragma once

#include "../Interface/File.h"
#include "../Interface/Server.h"
#include <vector>
#include <string.h>
#include <algorithm>

//A file that starts out as the parent's copy (btrfs snapshot or reflink). Only bytes that differ
//are written, so pages the parent already holds keep sharing its extents. On close the file is cut
//to the last byte written, so a shorter result does not keep the parent's tail.
class InPlaceFile : public IFile
{
public:
	explicit InPlaceFile(IFsFile* file)
		: file(file), pos(0), high_water(0)
	{
	}

	~InPlaceFile()
	{
		if (high_water < file->Size())
		{
			file->Resize(high_water, false);
		}
		Server->destroy(file);
	}

	virtual std::string Read(_u32 tr, bool* has_error = NULL)
	{
		std::string ret = file->Read(pos, tr, has_error);
		pos += ret.size();
		return ret;
	}

	virtual std::string Read(int64 spos, _u32 tr, bool* has_error = NULL)
	{
		return file->Read(spos, tr, has_error);
	}

	virtual _u32 Read(char* buffer, _u32 bsize, bool* has_error = NULL)
	{
		_u32 r = file->Read(pos, buffer, bsize, has_error);
		pos += r;
		return r;
	}

	virtual _u32 Read(int64 spos, char* buffer, _u32 bsize, bool* has_error = NULL)
	{
		return file->Read(spos, buffer, bsize, has_error);
	}

	virtual _u32 Write(const std::string& tw, bool* has_error = NULL)
	{
		return Write(tw.c_str(), static_cast<_u32>(tw.size()), has_error);
	}

	virtual _u32 Write(int64 spos, const std::string& tw, bool* has_error = NULL)
	{
		return Write(spos, tw.c_str(), static_cast<_u32>(tw.size()), has_error);
	}

	virtual _u32 Write(const char* buffer, _u32 bsize, bool* has_error = NULL)
	{
		_u32 w = Write(pos, buffer, bsize, has_error);
		pos += w;
		return w;
	}

	virtual _u32 Write(int64 spos, const char* buffer, _u32 bsize, bool* has_error = NULL)
	{
		high_water = (std::max)(high_water, spos + bsize);

		if (cmp.size() < bsize)
		{
			cmp.resize(bsize);
		}

		if (file->Read(spos, cmp.data(), bsize) == bsize
			&& memcmp(cmp.data(), buffer, bsize) == 0)
		{
			return bsize;
		}

		return file->Write(spos, buffer, bsize, has_error);
	}

	virtual bool Seek(_i64 spos)
	{
		pos = spos;
		return true;
	}

	virtual _i64 Size()
	{
		return file->Size();
	}

	virtual _i64 RealSize()
	{
		return file->RealSize();
	}

	virtual bool PunchHole(_i64 spos, _i64 size)
	{
		return file->PunchHole(spos, size);
	}

	virtual bool Sync()
	{
		return file->Sync();
	}

	virtual std::string getFilename()
	{
		return file->getFilename();
	}

private:
	IFsFile* file;
	int64 pos;
	int64 high_water;
	std::vector<char> cmp;
};
