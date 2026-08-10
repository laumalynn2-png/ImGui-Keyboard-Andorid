#include "Patcher.hpp"
#include <cstring>
#include <KittyMemory.h>
#include "il2cpp/log.h"

using namespace asmjit;

Patcher::Patcher(UnityResolve::Method* method) : target{method->function}
{
#ifdef __aarch64__
    code.init(Environment(Arch::kAArch64));
    code.attach(&assembler);
#endif
}

#ifdef __aarch64__

Error Patcher::nop(int count)
{
    for (int i = 0; i < count; i++)
        assembler.nop();
    return kErrorOk;
}

Error Patcher::ret()
{
    assembler.ret(a64::x30);
    return kErrorOk;
}

Error Patcher::movInt8(int8_t value)
{
    assembler.movz(a64::x0, static_cast<uint8_t>(value));
    return kErrorOk;
}

Error Patcher::movUInt8(uint8_t value)
{
    assembler.movz(a64::x0, value);
    return kErrorOk;
}

Error Patcher::movInt16(int16_t value)
{
    assembler.movz(a64::x0, static_cast<uint16_t>(value));
    return kErrorOk;
}

Error Patcher::movUInt16(uint16_t value)
{
    assembler.movz(a64::x0, value);
    return kErrorOk;
}

Error Patcher::movInt32(int32_t value)
{
    uint16_t lowerBit = static_cast<uint16_t>(value);
    uint16_t higherBit = static_cast<uint16_t>(value >> 16);
    assembler.mov(a64::w0, lowerBit);
    assembler.movk(a64::w0, higherBit, a64::lsl(16));
    return kErrorOk;
}

Error Patcher::movUInt32(uint32_t value)
{
    uint16_t lowerBit = static_cast<uint16_t>(value);
    uint16_t higherBit = static_cast<uint16_t>(value >> 16);
    assembler.mov(a64::w0, lowerBit);
    assembler.movk(a64::w0, higherBit, a64::lsl(16));
    return kErrorOk;
}

Error Patcher::movInt64(int64_t value)
{
    uint64_t v = static_cast<uint64_t>(value);
    uint16_t b0 = static_cast<uint16_t>(v);
    uint16_t b1 = static_cast<uint16_t>(v >> 16);
    uint16_t b2 = static_cast<uint16_t>(v >> 32);
    uint16_t b3 = static_cast<uint16_t>(v >> 48);
    assembler.movz(a64::x0, b0, a64::lsl(0));
    assembler.movk(a64::x0, b1, a64::lsl(16));
    assembler.movk(a64::x0, b2, a64::lsl(32));
    assembler.movk(a64::x0, b3, a64::lsl(48));
    return kErrorOk;
}

Error Patcher::movUInt64(uint64_t value)
{
    uint16_t b0 = static_cast<uint16_t>(value);
    uint16_t b1 = static_cast<uint16_t>(value >> 16);
    uint16_t b2 = static_cast<uint16_t>(value >> 32);
    uint16_t b3 = static_cast<uint16_t>(value >> 48);
    assembler.movz(a64::x0, b0, a64::lsl(0));
    assembler.movk(a64::x0, b1, a64::lsl(16));
    assembler.movk(a64::x0, b2, a64::lsl(32));
    assembler.movk(a64::x0, b3, a64::lsl(48));
    return kErrorOk;
}

Error Patcher::movFloat(float value)
{
    union FloatBits { float f; uint32_t i; };
    FloatBits fb{value};
    uint16_t lowerBit = static_cast<uint16_t>(fb.i);
    uint16_t higherBit = static_cast<uint16_t>(fb.i >> 16);
    assembler.mov(a64::w0, lowerBit);
    assembler.movk(a64::w0, higherBit, a64::lsl(16));
    assembler.fmov(a64::s0, a64::w0);
    return kErrorOk;
}

Error Patcher::movDouble(double value)
{
    union DoubleBits { double d; uint64_t i; };
    DoubleBits db{value};
    uint16_t b0 = static_cast<uint16_t>(db.i);
    uint16_t b1 = static_cast<uint16_t>(db.i >> 16);
    uint16_t b2 = static_cast<uint16_t>(db.i >> 32);
    uint16_t b3 = static_cast<uint16_t>(db.i >> 48);
    assembler.movz(a64::x0, b0, a64::lsl(0));
    assembler.movk(a64::x0, b1, a64::lsl(16));
    assembler.movk(a64::x0, b2, a64::lsl(32));
    assembler.movk(a64::x0, b3, a64::lsl(48));
    assembler.fmov(a64::d0, a64::x0);
    return kErrorOk;
}

Error Patcher::movBool(bool value)
{
    assembler.mov(a64::x0, value ? 1 : 0);
    return kErrorOk;
}

Error Patcher::movPtr(void* value)
{
    assembler.mov(a64::x0, imm(value));
    return kErrorOk;
}

Error Patcher::movVector2(float x, float y)
{
    union { float f; uint32_t i; } fb;
    fb.f = x;
    assembler.mov(a64::w1, static_cast<uint16_t>(fb.i));
    assembler.movk(a64::w1, static_cast<uint16_t>(fb.i >> 16), a64::lsl(16));
    assembler.fmov(a64::s0, a64::w1);
    fb.f = y;
    assembler.mov(a64::w1, static_cast<uint16_t>(fb.i));
    assembler.movk(a64::w1, static_cast<uint16_t>(fb.i >> 16), a64::lsl(16));
    assembler.fmov(a64::s1, a64::w1);
    return kErrorOk;
}

Error Patcher::movVector3(float x, float y, float z)
{
    union { float f; uint32_t i; } fb;
    fb.f = x;
    assembler.mov(a64::w1, static_cast<uint16_t>(fb.i));
    assembler.movk(a64::w1, static_cast<uint16_t>(fb.i >> 16), a64::lsl(16));
    assembler.fmov(a64::s0, a64::w1);
    fb.f = y;
    assembler.mov(a64::w1, static_cast<uint16_t>(fb.i));
    assembler.movk(a64::w1, static_cast<uint16_t>(fb.i >> 16), a64::lsl(16));
    assembler.fmov(a64::s1, a64::w1);
    fb.f = z;
    assembler.mov(a64::w1, static_cast<uint16_t>(fb.i));
    assembler.movk(a64::w1, static_cast<uint16_t>(fb.i >> 16), a64::lsl(16));
    assembler.fmov(a64::s2, a64::w1);
    return kErrorOk;
}

Error Patcher::movVector4(float x, float y, float z, float w)
{
    union { float f; uint32_t i; } fb;
    fb.f = x;
    assembler.mov(a64::w1, static_cast<uint16_t>(fb.i));
    assembler.movk(a64::w1, static_cast<uint16_t>(fb.i >> 16), a64::lsl(16));
    assembler.fmov(a64::s0, a64::w1);
    fb.f = y;
    assembler.mov(a64::w1, static_cast<uint16_t>(fb.i));
    assembler.movk(a64::w1, static_cast<uint16_t>(fb.i >> 16), a64::lsl(16));
    assembler.fmov(a64::s1, a64::w1);
    fb.f = z;
    assembler.mov(a64::w1, static_cast<uint16_t>(fb.i));
    assembler.movk(a64::w1, static_cast<uint16_t>(fb.i >> 16), a64::lsl(16));
    assembler.fmov(a64::s2, a64::w1);
    fb.f = w;
    assembler.mov(a64::w1, static_cast<uint16_t>(fb.i));
    assembler.movk(a64::w1, static_cast<uint16_t>(fb.i >> 16), a64::lsl(16));
    assembler.fmov(a64::s3, a64::w1);
    return kErrorOk;
}

#else

void Patcher::emit16(uint16_t val)
{
    byteBuffer.push_back(static_cast<uint8_t>(val & 0xFF));
    byteBuffer.push_back(static_cast<uint8_t>(val >> 8));
}

void Patcher::emit32(uint32_t val)
{
    byteBuffer.push_back(static_cast<uint8_t>(val & 0xFF));
    byteBuffer.push_back(static_cast<uint8_t>((val >> 8) & 0xFF));
    byteBuffer.push_back(static_cast<uint8_t>((val >> 16) & 0xFF));
    byteBuffer.push_back(static_cast<uint8_t>((val >> 24) & 0xFF));
}

void Patcher::emitMovW(uint8_t rd, uint16_t imm16)
{
    uint16_t imm4 = (imm16 >> 12) & 0xF;
    uint16_t i = (imm16 >> 11) & 0x1;
    uint16_t imm3 = (imm16 >> 8) & 0x7;
    uint16_t imm8 = imm16 & 0xFF;
    emit16(static_cast<uint16_t>(0xF240 | (i << 10) | imm4));
    emit16(static_cast<uint16_t>((imm3 << 12) | (rd << 8) | imm8));
}

void Patcher::emitMovT(uint8_t rd, uint16_t imm16)
{
    uint16_t imm4 = (imm16 >> 12) & 0xF;
    uint16_t i = (imm16 >> 11) & 0x1;
    uint16_t imm3 = (imm16 >> 8) & 0x7;
    uint16_t imm8 = imm16 & 0xFF;
    emit16(static_cast<uint16_t>(0xF2C0 | (i << 10) | imm4));
    emit16(static_cast<uint16_t>((imm3 << 12) | (rd << 8) | imm8));
}

Error Patcher::nop(int count)
{
    for (int i = 0; i < count; i++)
        emit16(0xBF00);
    return kErrorOk;
}

Error Patcher::ret()
{
    emit16(0x4770);
    return kErrorOk;
}

Error Patcher::movInt8(int8_t value)
{
    emit16(static_cast<uint16_t>(0x2000 | (static_cast<uint8_t>(value) & 0xFF)));
    return kErrorOk;
}

Error Patcher::movUInt8(uint8_t value)
{
    emit16(static_cast<uint16_t>(0x2000 | value));
    return kErrorOk;
}

Error Patcher::movInt16(int16_t value)
{
    emitMovW(0, static_cast<uint16_t>(value));
    return kErrorOk;
}

Error Patcher::movUInt16(uint16_t value)
{
    emitMovW(0, value);
    return kErrorOk;
}

Error Patcher::movInt32(int32_t value)
{
    uint16_t lowerBit = static_cast<uint16_t>(value);
    uint16_t higherBit = static_cast<uint16_t>(value >> 16);
    emitMovW(0, lowerBit);
    emitMovT(0, higherBit);
    return kErrorOk;
}

Error Patcher::movUInt32(uint32_t value)
{
    uint16_t lowerBit = static_cast<uint16_t>(value);
    uint16_t higherBit = static_cast<uint16_t>(value >> 16);
    emitMovW(0, lowerBit);
    emitMovT(0, higherBit);
    return kErrorOk;
}

Error Patcher::movInt64(int64_t value)
{
    uint64_t v = static_cast<uint64_t>(value);
    emitMovW(0, static_cast<uint16_t>(v));
    emitMovT(0, static_cast<uint16_t>(v >> 16));
    emitMovW(1, static_cast<uint16_t>(v >> 32));
    emitMovT(1, static_cast<uint16_t>(v >> 48));
    return kErrorOk;
}

Error Patcher::movUInt64(uint64_t value)
{
    emitMovW(0, static_cast<uint16_t>(value));
    emitMovT(0, static_cast<uint16_t>(value >> 16));
    emitMovW(1, static_cast<uint16_t>(value >> 32));
    emitMovT(1, static_cast<uint16_t>(value >> 48));
    return kErrorOk;
}

Error Patcher::movFloat(float value)
{
    union FloatBits { float f; uint32_t i; };
    FloatBits fb{value};
    emitMovW(0, static_cast<uint16_t>(fb.i));
    emitMovT(0, static_cast<uint16_t>(fb.i >> 16));
    emit32(0xEE000A10);
    return kErrorOk;
}

Error Patcher::movDouble(double value)
{
    union DoubleBits { double d; uint64_t i; };
    DoubleBits db{value};
    emitMovW(0, static_cast<uint16_t>(db.i));
    emitMovT(0, static_cast<uint16_t>(db.i >> 16));
    emitMovW(1, static_cast<uint16_t>(db.i >> 32));
    emitMovT(1, static_cast<uint16_t>(db.i >> 48));
    emit32(0xEC410B10);
    return kErrorOk;
}

Error Patcher::movBool(bool value)
{
    emit16(static_cast<uint16_t>(0x2000 | (value ? 1 : 0)));
    return kErrorOk;
}

Error Patcher::movPtr(void* value)
{
    uintptr_t v = reinterpret_cast<uintptr_t>(value);
    emitMovW(0, static_cast<uint16_t>(v));
    emitMovT(0, static_cast<uint16_t>(v >> 16));
    return kErrorOk;
}

Error Patcher::movVector2(float x, float y)
{
    union { float f; uint32_t i; } fb;
    fb.f = x;
    emitMovW(0, static_cast<uint16_t>(fb.i));
    emitMovT(0, static_cast<uint16_t>(fb.i >> 16));
    emit32(0xEE000A10);
    fb.f = y;
    emitMovW(0, static_cast<uint16_t>(fb.i));
    emitMovT(0, static_cast<uint16_t>(fb.i >> 16));
    emit32(0xEE010A10);
    return kErrorOk;
}

Error Patcher::movVector3(float x, float y, float z)
{
    union { float f; uint32_t i; } fb;
    fb.f = x;
    emitMovW(0, static_cast<uint16_t>(fb.i));
    emitMovT(0, static_cast<uint16_t>(fb.i >> 16));
    emit32(0xEE000A10);
    fb.f = y;
    emitMovW(0, static_cast<uint16_t>(fb.i));
    emitMovT(0, static_cast<uint16_t>(fb.i >> 16));
    emit32(0xEE010A10);
    fb.f = z;
    emitMovW(0, static_cast<uint16_t>(fb.i));
    emitMovT(0, static_cast<uint16_t>(fb.i >> 16));
    emit32(0xEE020A10);
    return kErrorOk;
}

Error Patcher::movVector4(float x, float y, float z, float w)
{
    union { float f; uint32_t i; } fb;
    fb.f = x;
    emitMovW(0, static_cast<uint16_t>(fb.i));
    emitMovT(0, static_cast<uint16_t>(fb.i >> 16));
    emit32(0xEE000A10);
    fb.f = y;
    emitMovW(0, static_cast<uint16_t>(fb.i));
    emitMovT(0, static_cast<uint16_t>(fb.i >> 16));
    emit32(0xEE010A10);
    fb.f = z;
    emitMovW(0, static_cast<uint16_t>(fb.i));
    emitMovT(0, static_cast<uint16_t>(fb.i >> 16));
    emit32(0xEE020A10);
    fb.f = w;
    emitMovW(0, static_cast<uint16_t>(fb.i));
    emitMovT(0, static_cast<uint16_t>(fb.i >> 16));
    emit32(0xEE030A10);
    return kErrorOk;
}

#endif

std::vector<uint8_t> Patcher::patch()
{
    std::vector<uint8_t> bytes;
#ifdef __aarch64__
    for (auto s : code.sections())
    {
        for (auto c : s->buffer())
        {
            bytes.push_back(static_cast<uint8_t>(c));
        }
    }
#else
    bytes = byteBuffer;
#endif
    if (bytes.empty()) return bytes;

    KittyMemory::ProtectAddr(target, bytes.size(), _PROT_RWX_);
    std::vector<uint8_t> originalBytes(static_cast<uint8_t*>(target),
                                       static_cast<uint8_t*>(target) + bytes.size());
    memcpy(target, bytes.data(), bytes.size());
    return originalBytes;
}

void Patcher::restore(const std::vector<uint8_t>& originalBytes)
{
    if (originalBytes.empty()) return;
    KittyMemory::ProtectAddr(target, originalBytes.size(), _PROT_RWX_);
    memcpy(target, originalBytes.data(), originalBytes.size());
}
