#include "opcoda_core/disasm/x86_disasm.h"

#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

namespace {

using opcoda::disasm::Decoded;
using opcoda::disasm::decode;

[[nodiscard]] Decoded decodeBytes(const std::vector<std::uint8_t>& bytes) {
    return decode(bytes.data(), bytes.size());
}

// gerador xorshift fixo: sem dependencia de <random>, a sequencia e' a mesma em
// qualquer maquina e qualquer revisao da biblioteca.
class DeterministicRandom {
public:
    explicit DeterministicRandom(std::uint64_t seed) noexcept : state_ {seed | 1u} {}

    [[nodiscard]] std::uint64_t next() noexcept {
        auto x = state_;
        x ^= x << 13;
        x ^= x >> 7;
        x ^= x << 17;
        state_ = x;
        return x;
    }

private:
    std::uint64_t state_;
};

} // namespace

TEST(Disasm, DecodesThePrologueOfAFunction) {
    // 48 89 4C 24 08           mov qword ptr [rsp+8], rcx
    // 48 83 EC 28              sub rsp, 0x28
    const auto first = decodeBytes({0x48, 0x89, 0x4C, 0x24, 0x08});
    EXPECT_EQ(first.length, 5u);
    EXPECT_STREQ(first.text.data(), "mov qword ptr [rsp+0x8], rcx");
    EXPECT_FALSE(first.hasTarget);

    const auto second = decodeBytes({0x48, 0x83, 0xEC, 0x28});
    EXPECT_EQ(second.length, 4u);
    EXPECT_STREQ(second.text.data(), "sub rsp, 0x28");
}

TEST(Disasm, RipRelativeUsesTheDisplacement) {
    // 48 8D 05 10 20 00 00     lea rax, [rip+0x2010]
    const auto decoded = decodeBytes({0x48, 0x8D, 0x05, 0x10, 0x20, 0x00, 0x00});
    EXPECT_EQ(decoded.length, 7u);
    EXPECT_STREQ(decoded.text.data(), "lea rax, [rip+0x2010]");
}

TEST(Disasm, RelativeBranchesCarryTheTarget) {
    // E8 21 00 00 00           call +0x21
    const auto call = decodeBytes({0xE8, 0x21, 0x00, 0x00, 0x00});
    EXPECT_EQ(call.length, 5u);
    EXPECT_STREQ(call.text.data(), "call");
    EXPECT_TRUE(call.hasTarget);
    EXPECT_EQ(call.target, 0x21);

    // EB F9                    jmp -7
    const auto jump = decodeBytes({0xEB, 0xF9});
    EXPECT_EQ(jump.length, 2u);
    EXPECT_TRUE(jump.hasTarget);
    EXPECT_EQ(jump.target, -7);

    // 0F 84 FF FF FF FF        je rel32 -1
    const auto je = decodeBytes({0x0F, 0x84, 0xFF, 0xFF, 0xFF, 0xFF});
    EXPECT_EQ(je.length, 6u);
    EXPECT_TRUE(je.hasTarget);
    EXPECT_EQ(je.target, -1);
}

TEST(Disasm, RexSelectsTheExtendedRegisters) {
    // 41 50                    push r8
    const auto push = decodeBytes({0x41, 0x50});
    EXPECT_EQ(push.length, 2u);
    EXPECT_STREQ(push.text.data(), "push r8");

    // 44 89 C7                 mov edi, r8d
    const auto mov = decodeBytes({0x44, 0x89, 0xC7});
    EXPECT_EQ(mov.length, 3u);
    EXPECT_STREQ(mov.text.data(), "mov edi, r8d");

    // 4C 8B 05 08 00 00 00     mov r8, [rip+8]
    const auto load = decodeBytes({0x4C, 0x8B, 0x05, 0x08, 0x00, 0x00, 0x00});
    EXPECT_EQ(load.length, 7u);
    EXPECT_STREQ(load.text.data(), "mov r8, qword ptr [rip+0x8]");
}

TEST(Disasm, SibAndScaleSurvive) {
    // 48 8B 04 95 10 00 00 00  mov rax, [rdx*4+0x10]
    const auto decoded = decodeBytes(
        {0x48, 0x8B, 0x04, 0x95, 0x10, 0x00, 0x00, 0x00});
    EXPECT_EQ(decoded.length, 8u);
    EXPECT_STREQ(decoded.text.data(), "mov rax, qword ptr [rdx*4+0x10]");
}

TEST(Disasm, Endbr64ConsumesTheF3Prefix) {
    const auto decoded = decodeBytes({0xF3, 0x0F, 0x1E, 0xFA});
    EXPECT_EQ(decoded.length, 4u);
    EXPECT_STREQ(decoded.text.data(), "endbr64");
}

TEST(Disasm, ThePaddingNopReadsItsMemoryOperand) {
    // 0F 1F 44 00 00           nop dword ptr [rax+rax*1+0]
    const auto decoded = decodeBytes({0x0F, 0x1F, 0x44, 0x00, 0x00});
    EXPECT_EQ(decoded.length, 5u);
    EXPECT_STREQ(decoded.text.data(), "nop dword ptr [rax+rax+0x0]");
}

TEST(Disasm, UnknownOpcodesFallBackToOneByte) {
    // C4 (VEX) e D8 (FPU) caem em db e consomem um byte: a leitura re-sincroniza
    // no proximo opcode reconhecido.
    const auto vex = decodeBytes({0xC4, 0xE2, 0x7D, 0x18, 0x00});
    EXPECT_EQ(vex.length, 1u);
    EXPECT_STREQ(vex.text.data(), "db 0xC4");

    const auto fpu = decodeBytes({0xD8, 0x05});
    EXPECT_EQ(fpu.length, 1u);
    EXPECT_STREQ(fpu.text.data(), "db 0xD8");
}

TEST(Disasm, TruncatedInstructionReportsZero) {
    // 48 8B 85 sem os quatro bytes do deslocamento
    const auto decoded = decodeBytes({0x48, 0x8B, 0x85});
    EXPECT_EQ(decoded.length, 0u);

    // buffer vazio
    EXPECT_EQ(decode(nullptr, 0).length, 0u);
}

TEST(Disasm, RandomBytesNeverReadPastTheBuffer) {
    DeterministicRandom random {0x4F636F64612026u};
    for (int trial = 0; trial < 4000; ++trial) {
        std::vector<std::uint8_t> bytes(static_cast<std::size_t>(1 + (random.next() % 16)));
        for (auto& byte : bytes) {
            byte = static_cast<std::uint8_t>(random.next());
        }

        const auto decoded = decode(bytes.data(), bytes.size());
        ASSERT_TRUE(decoded.length == 0 || decoded.length <= bytes.size())
            << "a leitura passou do buffer";
        ASSERT_TRUE(decoded.length <= 15) << "instrucao maior que o teto";
        if (decoded.length > 0) {
            ASSERT_TRUE(decoded.text[0] != '\0') << "texto vazio com comprimento";
        }
    }
}

TEST(Disasm, LinearSweepAlwaysAdvances) {
    // uma varredura linear sobre bytes quaisquer avanca sempre: a soma dos
    // comprimentos tem de chegar ao fim do buffer (ou parar por dados
    // truncados), e nunca o programa trava a incrementar zero.
    DeterministicRandom random {0x5DADu};
    for (int trial = 0; trial < 400; ++trial) {
        const auto count = static_cast<std::size_t>(64 + (random.next() % 256));
        std::vector<std::uint8_t> bytes(count);
        for (auto& byte : bytes) {
            byte = static_cast<std::uint8_t>(random.next());
        }

        std::size_t cursor = 0;
        int steps = 0;
        while (cursor < bytes.size() && steps < 4096) {
            const auto decoded = decode(bytes.data() + cursor, bytes.size() - cursor);
            if (decoded.length == 0) {
                break;
            }
            ASSERT_LE(decoded.length, bytes.size() - cursor);
            cursor += decoded.length;
            ++steps;
        }
        ASSERT_GT(steps, 0);
    }
}
