#pragma once
#include <string>
#include <vector>
#include <deque>
#include <map>
#include <unordered_map>
#include <utility>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <cstdint>
using namespace std;

// Addresses are plain numbers inside the emulator and hex when typed or printed: 1280 <-> 0x500
inline string toHex(uint64_t value) {
	stringstream ss;
	ss << "0x" << uppercase << hex << value;
	return ss.str();
}

// Emulated physical memory with demand paging.
//
// Physical memory is max-overall-mem / mem-per-frame frames. Each process has its own
// pages, numbered from 0, the same size as a frame. Creating a process loads nothing:
// a page is only brought into a frame when the process first uses it (a page fault).
// If no frame is free, the page that was loaded longest ago (FIFO) is evicted to the
// backing store. The backing store is mirrored to csopesy-backing-store.txt.
//
// Only the scheduler uses this, always while holding its own lock, so this class has none.
class Memory {
private:
	// One frame of physical memory
	struct FrameSlot {
		bool used = false;
		int pid = 0;            // process that owns the page in this frame
		uint64_t page = 0;      // which of that process's pages it is
		vector<uint8_t> bytes;  // the page's contents
	};

	// One row of a process's page table
	struct PageEntry {
		bool present = false;  // in a frame right now
		size_t frame = 0;      // which frame, while present
	};

	struct PageTable {
		string name;  // for the backing store file
		vector<PageEntry> pages;
	};

	static constexpr const char* BACKING_STORE_FILE = "csopesy-backing-store.txt";

	uint64_t frameSize;
	vector<FrameSlot> frames;
	deque<size_t> fifo;                                      // frames in load order; the front is evicted first
	unordered_map<int, PageTable> pageTables;                // by process ID
	map<pair<int, uint64_t>, vector<uint8_t>> backingStore;  // evicted pages by (process ID, page)
	uint64_t pagedIn;
	uint64_t pagedOut;
	bool backingStoreChanged;

	size_t lowestFreeFrame() const {
		size_t f = 0;
		while (frames[f].used) {
			f++;
		}
		return f;
	}

	// Moves the page loaded longest ago to the backing store and returns its now-empty frame
	size_t evictOldest() {
		size_t f = fifo.front();
		fifo.pop_front();

		FrameSlot& slot = frames[f];
		backingStore[{ slot.pid, slot.page }] = slot.bytes;
		pageTables[slot.pid].pages[slot.page].present = false;
		slot.used = false;

		pagedOut++;
		backingStoreChanged = true;
		return f;
	}

	// The frame holding this page, loading the page first if it isn't in one (a page fault)
	size_t access(int pid, uint64_t page) {
		if (pageTables.at(pid).pages.at(page).present) {
			return pageTables[pid].pages[page].frame;
		}

		size_t f = fifo.size() < frames.size() ? lowestFreeFrame() : evictOldest();
		FrameSlot& slot = frames[f];

		auto stored = backingStore.find({ pid, page });
		if (stored != backingStore.end()) {
			slot.bytes = stored->second;
			backingStore.erase(stored);
			backingStoreChanged = true;
		}
		else {
			slot.bytes.assign(frameSize, 0);  // never used yet: uninitialized memory reads as 0
		}
		slot.used = true;
		slot.pid = pid;
		slot.page = page;

		PageEntry& entry = pageTables[pid].pages[page];
		entry.present = true;
		entry.frame = f;
		fifo.push_back(f);
		pagedIn++;
		return f;
	}

	uint8_t readByte(int pid, uint64_t address) {
		size_t f = access(pid, address / frameSize);
		return frames[f].bytes[address % frameSize];
	}

	void writeByte(int pid, uint64_t address, uint8_t value) {
		size_t f = access(pid, address / frameSize);
		frames[f].bytes[address % frameSize] = value;
	}

public:
	Memory(uint64_t maxOverallMem, uint64_t memPerFrame)
		: frameSize(memPerFrame), frames(maxOverallMem / memPerFrame),
		  pagedIn(0), pagedOut(0), backingStoreChanged(true) {
		saveBackingStore();  // start each run with an empty file
	}

	// A new process gets a page table with every page not loaded yet
	void addProcess(int pid, const string& name, uint64_t mem) {
		uint64_t pageCount = (mem + frameSize - 1) / frameSize;  // rounded up
		pageTables[pid] = { name, vector<PageEntry>(pageCount) };
	}

	// A terminated process gives back its frames and its pages in the backing store
	void removeProcess(int pid) {
		auto table = pageTables.find(pid);
		if (table == pageTables.end()) {
			return;
		}

		for (const PageEntry& entry : table->second.pages) {
			if (entry.present) {
				frames[entry.frame].used = false;
				fifo.erase(find(fifo.begin(), fifo.end(), entry.frame));
			}
		}

		auto it = backingStore.lower_bound({ pid, 0 });
		while (it != backingStore.end() && it->first.first == pid) {
			it = backingStore.erase(it);
			backingStoreChanged = true;
		}

		pageTables.erase(table);
	}

	// A uint16 takes 2 bytes, low byte first, so it can straddle two pages.
	// The caller has already checked the address is inside the process's memory.
	uint16_t read16(int pid, uint64_t address) {
		uint16_t low = readByte(pid, address);
		uint16_t high = readByte(pid, address + 1);
		return uint16_t(low | (high << 8));
	}

	void write16(int pid, uint64_t address, uint16_t value) {
		writeByte(pid, address, uint8_t(value & 0xFF));
		writeByte(pid, address + 1, uint8_t(value >> 8));
	}

	uint64_t totalBytes() const {
		return frames.size() * frameSize;
	}

	uint64_t usedBytes() const {
		return fifo.size() * frameSize;  // fifo lists exactly the frames in use
	}

	// Bytes of physical memory a process occupies right now
	uint64_t residentBytes(int pid) const {
		uint64_t count = 0;
		for (const FrameSlot& slot : frames) {
			if (slot.used && slot.pid == pid) {
				count++;
			}
		}
		return count * frameSize;
	}

	uint64_t getPagedIn() const {
		return pagedIn;
	}

	uint64_t getPagedOut() const {
		return pagedOut;
	}

	// Rewrites csopesy-backing-store.txt if the backing store changed since the last save.
	// One line per page: owner, page number, then its bytes in hex.
	void saveBackingStore() {
		if (!backingStoreChanged) {
			return;
		}
		backingStoreChanged = false;

		ofstream file(BACKING_STORE_FILE);
		file << "Backing store: " << backingStore.size() << " page(s)\n";
		for (const auto& [key, bytes] : backingStore) {
			file << pageTables[key.first].name << " page " << key.second << ":";
			for (uint8_t b : bytes) {
				file << " " << uppercase << hex << setw(2) << setfill('0') << int(b);
			}
			file << dec << setfill(' ') << "\n";
		}
	}
};
