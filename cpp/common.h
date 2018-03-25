/// 786

/******************************************************************************/

#pragma once

/******************************************************************************/

#include <map>
#include <vector>
#include <string>
#include <functional>
#include <chrono>
#include <string>
#include <cstdlib>
#include <unordered_map>

#include <sys/types.h>
#include <sys/stat.h>

#include "format.h"

/******************************************************************************/

#ifdef __cplusplus
#define EXTERNC extern "C"
#else
#define EXTERNC
#endif

#define prn(f, ...)    fmt::print(f "\n", ##__VA_ARGS__)
#define prnn(...)      fmt::print(__VA_ARGS__)

#define eprn(f, ...)   fmt::print(stderr, f "\n",  ##__VA_ARGS__)
#define eprnn(...)     fmt::print(stderr, __VA_ARGS__)

#define die_if(Q, f, ...)  if (Q) { fmt::print(stderr, f "---exiting!\n",  ##__VA_ARGS__) ; exit(1); }

#ifdef NDEBUG
#define dprn(f, ...)   ;
#define dprnn(...)     ;
#else
#define dprn(f, ...)   { if(getenv("EMADBG")) fmt::print(stderr, f "\n",  ##__VA_ARGS__); }
#define dprnn(...)     { if(getenv("EMADBG")) fmt::print(stderr, __VA_ARGS__); }
#endif

#define cur_time()     chrono::high_resolution_clock::now()
#define elapsed(t)     (chrono::duration_cast<chrono::milliseconds>(chrono::high_resolution_clock::now() - (t)).count() / 1000.00)

#define DO(x) for(int _ = 0; _ < (x); _++)
#define DOV(i,x) for(int (i) = 0; (i) < (x); (i)++)

/******************************************************************************/

const int MATE1_TRIM = 7;
const int BC_LEN = 16;

const int ILLUMINA_QUAL_OFFSET = 33;
const int QUAL_BASE = ILLUMINA_QUAL_OFFSET + 1;

const int MIN_READ_SIZE = 32;

const size_t KB = 1024;
const size_t MB = 1024 * KB;
const size_t GB = 1024 * MB;

/******************************************************************************/

struct DNA {
	char val[128];
	constexpr DNA(int def): val()
	{
		for (int i = 0; i < 128; i++) val[i] = def;
		val['A'] = val['a'] = 0;
		val['C'] = val['c'] = 1;
		val['G'] = val['g'] = 2;
		val['T'] = val['t'] = 3;
	}
};
constexpr auto dna_hash_lookup = DNA(0);
inline char hash_dna(char c)
{
	return dna_hash_lookup.val[c];
}

/******************************************************************************/

template<typename K, typename V>
inline size_t estimate_size(const std::map<K, V> &m)
{
	const int overhead = 32;
	return (sizeof(K) + sizeof(V) + overhead) * m.size();
}

template<typename K, typename V>
inline size_t estimate_size(const std::unordered_map<K, V> &M) 
{
	size_t n = M.bucket_count();
	float m = M.max_load_factor();
	if (m > 1.0) n *= m;
	return (M.size() * sizeof(V) + n * (sizeof(size_t) + sizeof(void*)));
}

inline int stat_dir(const std::string &path)
{
	struct stat path_stat;
	int s = stat(path.c_str(), &path_stat);
	if (s != 0) {
		return 0;
	}
	if (S_ISDIR(path_stat.st_mode)) {
		return 1;
	}
	if (S_ISREG(path_stat.st_mode)) {
		return 2;
	} 
	return 3;
}