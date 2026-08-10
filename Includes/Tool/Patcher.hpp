#pragma once
#include <cstdint>
#include <vector>
#include <asmjit/asmjit.h>
#ifdef __aarch64__
#include <asmjit/a64.h>
#endif
#include "il2cpp/UnityResolve.hpp"

class Patcher {
public:
    Patcher(UnityResolve::Method* method);

    asmjit::Error nop(int count = 1);
    asmjit::Error ret();

    asmjit::Error movInt8(int8_t value);
    asmjit::Error movUInt8(uint8_t value);
    asmjit::Error movInt16(int16_t value);
    asmjit::Error movUInt16(uint16_t value);
    asmjit::Error movInt32(int32_t value);
    asmjit::Error movUInt32(uint32_t value);
    asmjit::Error movInt64(int64_t value);
    asmjit::Error movUInt64(uint64_t value);
    asmjit::Error movFloat(float value);
    asmjit::Error movDouble(double value);
    asmjit::Error movBool(bool value);
    asmjit::Error movPtr(void* value);
    asmjit::Error movVector2(float x, float y);
    asmjit::Error movVector3(float x, float y, float z);
    asmjit::Error movVector4(float x, float y, float z, float w);

    std::vector<uint8_t> patch();
    void restore(const std::vector<uint8_t>& originalBytes);

private:
    void* target;
#ifdef __aarch64__
    asmjit::CodeHolder code;
    asmjit::a64::Assembler assembler;
#else
    std::vector<uint8_t> byteBuffer;
    void emit16(uint16_t val);
    void emit32(uint32_t val);
    void emitMovW(uint8_t rd, uint16_t imm16);
    void emitMovT(uint8_t rd, uint16_t imm16);
#endif
};
