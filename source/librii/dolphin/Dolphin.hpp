#pragma once

#include <core/common.h>
#include <optional>
#include <rsl/SimpleReader.hpp>

namespace librii::dolphin {

struct SharedMem {
  static Result<SharedMem> from(std::string memFileName);
  SharedMem() : sharedMem(nullptr) {}
  SharedMem(void* s) : sharedMem(s) {}
  SharedMem(const SharedMem&) = delete;
  SharedMem(SharedMem&& rhs) noexcept : sharedMem(rhs.sharedMem) {
    rhs.sharedMem = nullptr;
  }
  ~SharedMem();
  void* get() const { return sharedMem; }

  void* sharedMem;
};

std::optional<int> GetDolphinPID();

Result<SharedMem> OpenDolphin(int pid);

static inline u32 GetRamSizeReal(const SharedMem& mem) {
  auto* p = reinterpret_cast<rsl::bu32*>((char*)mem.get() + 0x00000028);
  return *p;
}
static inline uint32_t nextPowerOf2(uint32_t n) {
  if (n == 0)
    return 1;

  n--;
  n |= n >> 1;
  n |= n >> 2;
  n |= n >> 4;
  n |= n >> 8;
  n |= n >> 16;
  return n + 1;
}
static inline u32 GetRamSize(const SharedMem& mem) {
  return nextPowerOf2(GetRamSizeReal(mem));
}

static inline u32 GetL1CacheSize() { return 0x00040000; }
static inline u32 GetFakeVMemSize() {
  // const bool fake_vmem = !wii && !mmu;
  // Lazy: Assume it's a Wii, since we don't care about ExRam on GC anyway
  return 0;
}
static inline u32 GetExRamSize() {
  // Lazy: Max value (128MB)
  return 0x8000000;
}

static inline Result<void*> VirtualToSHMEM(const SharedMem& shmem, u32 vaddr) {
  if (vaddr >= 0x9000'0000) {
    EXPECT(vaddr - 0x9000'0000 <= GetExRamSize());
    u32 mem1_size = GetRamSize(shmem);
    u32 sim_mem2 = mem1_size + GetL1CacheSize() + GetFakeVMemSize();
    u32 mem2_offset = vaddr - 0x9000'0000;
    u32 sim = sim_mem2 + mem2_offset;
    return reinterpret_cast<void*>((char*)shmem.get() + sim);
  }
  if (vaddr >= 0x8000'0000) {
    EXPECT(vaddr - 0x8000'0000 <= GetRamSizeReal(shmem));
    u32 sim = vaddr - 0x8000'0000;
    return reinterpret_cast<void*>((char*)shmem.get() + sim);
  }
  return std::unexpected("Unexpected address space");
}

struct DolphinAc {
  enum class Status {
    Hooked,
    UnHooked,
  };
  static inline uint32_t code[] = {
      llvm::ByteSwap_32(0x029f0010), llvm::ByteSwap_32(0x029f0033),
      llvm::ByteSwap_32(0x029f0034), llvm::ByteSwap_32(0x029f0035)};

  void* find_sequence(void* start, size_t size) {
    uint32_t* p = (uint32_t*)start;
    for (size_t i = 0; i < size - 4 + 1; i++, p++) {
      if (memcmp(p, code, sizeof(code)) == 0) {
        return p;
      }
    }
    return NULL;
  }
  void hook() {
    mPID = GetDolphinPID();
    if (mPID) {
      new (&mSHM) SharedMem(*OpenDolphin(*mPID));
      dumpMemoryLayout();
      void* addr = find_sequence((char*)mSHM.get() + GetRamSize(mSHM),
                                 GetExRamSize() / 4);
      fmt::print("MEM2 at {:x}\n", (unsigned long long)addr);
    }
  }

  void unhook() {
    mPID.reset();
    mSHM.~SharedMem();
  }

  Result<void> readFromRAM(unsigned int offset,
                           std::span<unsigned char> buffer) {
    if (!mSHM.get()) {
      return std::unexpected("Not hooked");
    }
    // TODO: Check end pointer, too
    auto* v = TRY(VirtualToSHMEM(mSHM, offset));
    memcpy(buffer.data(), v, buffer.size());
    return {};
  }

  Result<void> writeToRAM(unsigned int offset,
                          std::span<const unsigned char> buffer) {
    if (!mSHM.get()) {
      return std::unexpected("Not hooked");
    }
    // TODO: Check end pointer, too
    auto* v = TRY(VirtualToSHMEM(mSHM, offset));
    memcpy(v, buffer.data(), buffer.size());
    return {};
  }

  Status getStatus() const {
    return mSHM.get() ? Status::Hooked : Status::UnHooked;
  }

  void dumpMemoryLayout();
  void dumpRegion(const std::string& name, u32 virtualStart, u32 size);

  SharedMem mSHM;
  std::optional<int> mPID;
};

#if 0
m_physical_regions[0] = PhysicalMemoryRegion{
    &m_ram, 0x00000000, GetRamSize(), PhysicalMemoryRegion::ALWAYS, 0, false};
m_physical_regions[1] = PhysicalMemoryRegion{
    &m_l1_cache, 0xE0000000, GetL1CacheSize(), PhysicalMemoryRegion::ALWAYS, 0, false};
m_physical_regions[2] = PhysicalMemoryRegion{
    &m_fake_vmem, 0x7E000000, GetFakeVMemSize(), PhysicalMemoryRegion::FAKE_VMEM, 0, false};
m_physical_regions[3] = PhysicalMemoryRegion{
    &m_exram, 0x10000000, GetExRamSize(), PhysicalMemoryRegion::WII_ONLY, 0, false};
PowerPC::HostWrite_U32(Memory::GetRamSizeReal(), 0x80000028);
#endif

} // namespace librii::dolphin
