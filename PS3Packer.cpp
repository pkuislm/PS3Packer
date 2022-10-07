// PS3Packer.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//

#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <unordered_map>
#include <stdio.h>
#include <stdint.h>
#include <Windows.h>
#include <stdlib.h>
#include <strsafe.h>
#include <atlstr.h>
#include "lzss.h"

#pragma pack(push, 1)
typedef struct
{
	uint8_t magic[4];			// "PS2A0"
	uint32_t header_length;		// 0x30
	uint32_t unk0;
	uint32_t unk1;
	uint32_t unkval_count;
	uint32_t vmcode_length;
	uint32_t unknown3_length;
	uint32_t strpool_length;
	uint32_t unk2;
	uint32_t comprlen;             //should be updated
	uint32_t uncomprlen;           //should be updated
	uint32_t unk3;
} ps2_header_t;
#pragma pack(pop)

static uint32_t op_push_str = 0x01200201;
//static uint32_t op_push_sel = 0x01022001;

struct vm_pushstr
{
	byte* offset_in_code;
	uint32_t value;
	vm_pushstr(byte* a, uint32_t b) :offset_in_code(a), value(b) {};
};

struct import_str
{
	uint32_t offset;
	CStringA text;
	import_str(uint32_t a, CStringA& b) :offset(a), text(b) {};
};

HLOCAL OpenPatchFile(const char* filePath, int* fileSize)
{
	std::ifstream in;
	in.open(filePath, std::ios::binary);
	if (in.is_open())
	{
		in.seekg(0, std::ios::end);
		auto fsize = in.tellg();
		in.seekg(0, std::ios::beg);
		HLOCAL ret = LocalAlloc(0, fsize);
		if (ret)
		{
			in.read((char*)ret, fsize);
			if (fileSize)
				*fileSize = fsize;
		}
		in.close();
		return ret;
	}
	return nullptr;
}

CStringW UTF8ToUcs2(const std::string& str)
{
	if (str.size() == 0)
		return CStringW();
	int nLen = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), str.size(), NULL, 0);
	if (nLen == 0)
		return CStringW();
	CStringW ret(L'\0', nLen);
	if (MultiByteToWideChar(CP_UTF8, 0, str.c_str(), str.size(), ret.GetBuffer(), ret.GetAllocLength()) == 0)
		return CStringW();
	return ret;
}

CStringA Ucs2ToAnsi(int cp, const CStringW& str, LPCCH defChar)
{
	if (str.GetLength() == 0)
		return CStringA();
	int nLen = WideCharToMultiByte(cp, 0, str.GetString(), str.GetLength(), NULL, 0, NULL, NULL);
	if (nLen == 0)
		return CStringA();
	CStringA ret('\0', nLen);
	if (WideCharToMultiByte(cp, 0, str.GetString(), str.GetLength(), ret.GetBuffer(), ret.GetAllocLength(), defChar, NULL) == 0)
		return CStringA();
	return ret;
}

CStringA Ucs2ToGbk(const CStringW& str)
{
	return Ucs2ToAnsi(936, str, "?");
}

class ps2aScript
{
	
	byte* m_script;
	uint32_t m_scriptSize;
	ps2_header_t m_header;
	std::unordered_map<uint32_t, std::vector<byte>> m_string_pool;
	std::unordered_map<uint32_t, uint32_t*> m_offsets;
	std::vector<uint32_t> m_order;
	//std::vector<vm_pushstr> m_offsets;
public:
	ps2aScript(byte* a, uint32_t b) :m_script(a),m_scriptSize(b)
	{
		memcpy((char*)&m_header, m_script, sizeof(ps2_header_t));
	};

	~ps2aScript()
	{
		LocalFree(m_script);
	}

	void scan()
	{
		if (!m_script)
		{
			return;
		}

		byte* scan_start = m_script + (m_header.header_length + m_header.unkval_count * 4);
		byte* scan_end = scan_start + m_header.vmcode_length;
		byte* str_start = scan_end + m_header.unknown3_length;
		char* str = nullptr;
		uint32_t len = 0;
		for (auto i = scan_start; i < scan_end; ++i)
		{
			if (*(uint32_t*)i == op_push_str)
			{
				i += 4;
				m_offsets.emplace(*(uint32_t*)i, (uint32_t*)i);
				m_order.emplace_back(*(uint32_t*)i);
				
				str = (char*)str_start + *(uint32_t*)i;
				len = strlen(str) + 1;
#ifdef _DEBUG
				std::cout << *(uint32_t*)i << ":" << (char*)str << std::endl;
#endif // _DEBUG
				//i += 4;//01022001 F6000000 0F023004
			}
		}
	}

	bool import(const std::string& textPath)
	{
		std::list<import_str> list;
		std::ifstream ifs;
		std::string line;
		ifs.open(textPath);
		if (ifs.is_open())
		{
			while (!ifs.eof())
			{
				getline(ifs, line);
				if (line != "")
				{
					auto out = UTF8ToUcs2(line);
					out.Replace(L'♪', L'⑨');//GBK没有这个符号，需要使用其他符号代替，并使用hook或修改字体的方式替换掉字模！
					out.Replace(L'・', L'·');
					out.Replace(L'~', L'～');
					if (out[0] == L'★')
					{
						auto index = out.Mid(out.Find(L'★') + 1, 10);
						auto text = Ucs2ToGbk(out.Mid(out.Find(L'★', 1) + 1));
						list.emplace_back(_wcstoi64(index.GetString(), nullptr, 16), text);
					}
				}
			}
			ifs.close();

			uint32_t len_new = 0;
			for (auto& i : list)
			{
				if (m_offsets.find(i.offset) == m_offsets.end())
				{
					//
					std::cout << "Unexpected offset at: " << i.offset << ", str: " << i.text.GetString() << std::endl;
					continue;
				}
				len_new = i.text.GetLength() + 1;

				std::vector<byte> vec;
				vec.resize(len_new);
				memcpy(&vec[0], i.text.GetString(), len_new);
				m_string_pool.emplace(i.offset, vec);
			}

			uint32_t pool_offset = 0;
			uint32_t str_start = m_header.strpool_length;
			for(auto i : m_order)
			{
				if (m_string_pool.find(i) != m_string_pool.end())
				{
					*(uint32_t*)m_offsets[i] = str_start + pool_offset;
					pool_offset += m_string_pool[i].size();
				}
			}
			m_header.strpool_length += pool_offset;
			return true;
		}
		return false;
	}

	void write(const std::string& outputPath, bool compress = false)
	{
		std::ofstream ofs;
		ofs.open(outputPath, std::ios::binary);
		if (ofs.is_open())
		{
			if (compress)
			{
				auto numic = m_header.unkval_count * 4 + m_header.vmcode_length + m_header.unknown3_length + m_header.strpool_length;
				auto numic_old = m_header.unkval_count * 4 + m_header.vmcode_length + m_header.unknown3_length + ((ps2_header_t*)m_script)->strpool_length;
				auto blk = LocalAlloc(0, numic);
				auto blk2 = LocalAlloc(0, numic);
				if (blk && blk2)
				{
					
					memcpy(blk, m_script + 0x30, numic_old);
					auto pblk = (char*)blk + numic_old;
					for (auto i : m_order)
					{
						if (m_string_pool.find(i) != m_string_pool.end())
						{
							memcpy(pblk, (char*)&m_string_pool[i][0], m_string_pool[i].size());
							pblk += m_string_pool[i].size();
							//ofs.write((char*)&m_string_pool[i][0], m_string_pool[i].size());
						}
					}
					auto numic_after = lzss_encode((byte*)blk, numic, (byte*)blk2, numic);

					m_header.uncomprlen = numic;
					m_header.comprlen = numic_after;
					ofs.write((char*)&m_header, sizeof(ps2_header_t));
					ofs.write((char*)blk2, numic_after);
				}
				if (blk)
				{
					LocalFree(blk);
				}
				if (blk2)
				{
					LocalFree(blk2);
				}
			}
			else
			{
				ofs.write((char*)m_script, m_scriptSize);
				for (auto i : m_order)
				{
					if (m_string_pool.find(i) != m_string_pool.end())
					{
						ofs.write((char*)&m_string_pool[i][0], m_string_pool[i].size());
					}
				}
				ofs.seekp(0, std::ios::beg);
				ofs.write((char*)&m_header, sizeof(ps2_header_t));
			}
			ofs.close();
		}
	}
};

void ProcessText(const char* path)
{
	std::string strpath(path);
	int size = 0;
	auto scr = OpenPatchFile(strpath.c_str(), &size);
	ps2aScript pscript((byte*)scr, size);
	pscript.scan();
	pscript.import(strpath + ".txt");
	pscript.write(strpath + ".new", true);
}

int main(int argc, char**argv)
{
	if (argc < 2)
	{
		return 0;
	}
	setlocale(LC_ALL, "zh-cn");
	for (UINT i = 1; i < argc; ++i)
	{
		ProcessText(argv[i]);
	}
	system("pause");
	return 0;
}



// 运行程序: Ctrl + F5 或调试 >“开始执行(不调试)”菜单
// 调试程序: F5 或调试 >“开始调试”菜单

// 入门使用技巧: 
//   1. 使用解决方案资源管理器窗口添加/管理文件
//   2. 使用团队资源管理器窗口连接到源代码管理
//   3. 使用输出窗口查看生成输出和其他消息
//   4. 使用错误列表窗口查看错误
//   5. 转到“项目”>“添加新项”以创建新的代码文件，或转到“项目”>“添加现有项”以将现有代码文件添加到项目
//   6. 将来，若要再次打开此项目，请转到“文件”>“打开”>“项目”并选择 .sln 文件
