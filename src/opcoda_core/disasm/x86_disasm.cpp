#include "opcoda_core/disasm/x86_disasm.h"

namespace opcoda::disasm {
namespace {

// escrita num buffer fixo: o que nao cabe, cai fora. nenhuma funcao daqui
// retorna erro porque a coluna ASM desenha texto e texto cortado e' o pior
// caso; o comprimento da instrucao, esse, nunca depende do texto.
class Writer {
public:
    explicit Writer(std::array<char, 80>& text) noexcept : buffer_ {text} {
        buffer_[0] = '\0';
    }

    void add(char character) noexcept {
        if (used_ + 1 < buffer_.size()) {
            buffer_[used_++] = character;
            buffer_[used_] = '\0';
        }
    }

    void add(const char* text) noexcept {
        if (text == nullptr) {
            return;
        }
        while (*text != '\0') {
            add(*text);
            ++text;
        }
    }

    void clear() noexcept {
        used_ = 0;
        buffer_[0] = '\0';
    }

    // hex com digitos maiusculos, igual ao resto da grelha ("%02X").
    void addHex(std::uint64_t value) noexcept {
        add("0x");
        if (value == 0) {
            add('0');
            return;
        }
        char digits[16];
        int count = 0;
        while (value != 0 && count < 16) {
            const auto digit = static_cast<int>(value & 0xF);
            digits[count++] = static_cast<char>(digit < 10 ? '0' + digit : 'A' + digit - 10);
            value >>= 4;
        }
        while (count > 0) {
            add(digits[--count]);
        }
    }

    void addDecimal(std::uint64_t value) noexcept {
        char digits[20];
        int count = 0;
        do {
            digits[count++] = static_cast<char>('0' + static_cast<int>(value % 10));
            value /= 10;
        } while (value != 0 && count < 20);
        while (count > 0) {
            add(digits[--count]);
        }
    }

private:
    std::array<char, 80>& buffer_;
    std::size_t used_ {0};
};

class Reader {
public:
    Reader(const std::uint8_t* data, std::size_t size) noexcept : data_ {data}, size_ {size} {}

    [[nodiscard]] bool byte(std::uint8_t& out) noexcept {
        if (cursor_ >= size_) {
            return false;
        }
        out = data_[cursor_++];
        return true;
    }

    [[nodiscard]] bool imm32(std::uint32_t& out) noexcept {
        std::uint8_t bytes[4];
        for (auto& piece : bytes) {
            if (!byte(piece)) {
                return false;
            }
        }
        out = static_cast<std::uint32_t>(bytes[0]) |
              (static_cast<std::uint32_t>(bytes[1]) << 8) |
              (static_cast<std::uint32_t>(bytes[2]) << 16) |
              (static_cast<std::uint32_t>(bytes[3]) << 24);
        return true;
    }

    [[nodiscard]] bool imm64(std::uint64_t& out) noexcept {
        std::uint32_t low = 0;
        std::uint32_t high = 0;
        if (!imm32(low) || !imm32(high)) {
            return false;
        }
        out = static_cast<std::uint64_t>(low) | (static_cast<std::uint64_t>(high) << 32);
        return true;
    }

    [[nodiscard]] std::size_t consumed() const noexcept { return cursor_; }

private:
    const std::uint8_t* data_;
    std::size_t size_;
    std::size_t cursor_ {0};
};

// nomes de registo. as versoes com REX.B/REX.R somam 8 ao indice.
const char* const kReg64[16] {"rax", "rcx", "rdx", "rbx", "rsp", "rbp", "rsi", "rdi",
                              "r8",  "r9",  "r10", "r11", "r12", "r13", "r14", "r15"};
const char* const kReg32[16] {"eax", "ecx", "edx", "ebx", "esp", "ebp", "esi", "edi",
                              "r8d", "r9d", "r10d", "r11d", "r12d", "r13d", "r14d", "r15d"};
const char* const kReg16[16] {"ax",   "cx",   "dx",   "bx",   "sp",   "bp",   "si",   "di",
                              "r8w",  "r9w",  "r10w", "r11w", "r12w", "r13w", "r14w", "r15w"};
// sem REX, os indices 4..7 sao os meios-registos (ah..bh); com REX sao os
// novos (spl..dil) e r8b..r15b.
const char* const kReg8Low[8] {"al", "cl", "dl", "bl", "ah", "ch", "dh", "bh"};
const char* const kReg8Rex[16] {"al", "cl", "dl", "bl", "spl", "bpl", "sil", "dil",
                                "r8b", "r9b", "r10b", "r11b", "r12b", "r13b", "r14b", "r15b"};

const char* const kAlu[8] {"add", "or", "adc", "sbb", "and", "sub", "xor", "cmp"};
const char* const kShift[8] {"rol", "ror", "rcl", "rcr", "shl", "shr", "sal", "sar"};
const char* const kCondition[16] {"o",  "no", "b",  "ae", "e",  "ne", "be", "a",
                                  "s",  "ns", "p",  "np", "l",  "ge", "le", "g"};

// indice de registo das formas com registo embutido no opcode (50-5F, 90-97,
// B0-BF, bswap): o bit B do REX soma oito. sem ele, "41 50" (push r8) lia-se
// como "push rax".
[[nodiscard]] std::uint8_t opcodeRegister(std::uint8_t low3, std::uint8_t rex) noexcept {
    return static_cast<std::uint8_t>(low3 | ((rex & 0x1) != 0 ? 8u : 0u));
}

struct Rm {
    bool memory {false};
    std::uint8_t reg {0};      // campo reg, ja' com REX.R
    std::uint8_t rm {0};       // reg de destino, ja' com REX.B
    std::uint8_t base {0};
    std::uint8_t index {0};
    std::uint8_t scale {0};
    std::int32_t displacement {0};
    std::uint32_t displacementSize {0};
    bool ripRelative {false};
};

// le modrm + sib + deslocamento, sempre no modo longo. o prefixo 67 e'
// consumido antes; enderecamento de 32 bits e' raro no material e cai no caso
// geral de 64 bits, com o comprimento certo na mesma.
[[nodiscard]] bool readModRm(Reader& reader, std::uint8_t rex, Rm& out) noexcept {
    std::uint8_t modrm = 0;
    if (!reader.byte(modrm)) {
        return false;
    }

    const auto mod = static_cast<std::uint8_t>((modrm >> 6) & 3);
    out.reg = static_cast<std::uint8_t>(((modrm >> 3) & 7) | ((rex & 0x4) != 0 ? 8u : 0u));
    auto rm = static_cast<std::uint8_t>((modrm & 7) | ((rex & 0x1) != 0 ? 8u : 0u));

    if (mod == 3) {
        out.memory = false;
        out.rm = rm;
        return true;
    }

    out.memory = true;

    if ((modrm & 7) == 4) {
        // byte SIB: escala, indice e base.
        std::uint8_t sib = 0;
        if (!reader.byte(sib)) {
            return false;
        }
        out.scale = static_cast<std::uint8_t>((sib >> 6) & 3);
        out.index = static_cast<std::uint8_t>(((sib >> 3) & 7) | ((rex & 0x2) != 0 ? 8u : 0u));
        rm = static_cast<std::uint8_t>((sib & 7) | ((rex & 0x1) != 0 ? 8u : 0u));
        if ((sib & 7) == 5 && mod == 0) {
            // base nula: o deslocamento de 32 bits e' o endereco inteiro.
            std::uint32_t value = 0;
            if (!reader.imm32(value)) {
                return false;
            }
            out.displacement = static_cast<std::int32_t>(value);
            out.displacementSize = 4;
            out.base = 0xFF; // sem base
            return true;
        }
        out.base = ((sib & 7) == 5) ? 5 : rm;
    } else if ((modrm & 7) == 5 && mod == 0) {
        // rip relativo
        std::uint32_t value = 0;
        if (!reader.imm32(value)) {
            return false;
        }
        out.displacement = static_cast<std::int32_t>(value);
        out.displacementSize = 4;
        out.ripRelative = true;
        return true;
    } else {
        out.base = rm;
    }

    if (mod == 1) {
        std::uint8_t value = 0;
        if (!reader.byte(value)) {
            return false;
        }
        out.displacement = static_cast<std::int8_t>(value);
        out.displacementSize = 1;
    } else if (mod == 2) {
        std::uint32_t value = 0;
        if (!reader.imm32(value)) {
            return false;
        }
        out.displacement = static_cast<std::int32_t>(value);
        out.displacementSize = 4;
    }

    return true;
}

void writeRegister(Writer& writer, std::uint8_t index, int bits, bool rexUsed) noexcept {
    if (bits == 8) {
        writer.add(rexUsed ? kReg8Rex[index & 0xF] : kReg8Low[index & 7]);
    } else if (bits == 16) {
        writer.add(kReg16[index & 0xF]);
    } else if (bits == 32) {
        writer.add(kReg32[index & 0xF]);
    } else {
        writer.add(kReg64[index & 0xF]);
    }
}

void writeXmm(Writer& writer, std::uint8_t index) noexcept {
    writer.add("xmm");
    writer.addDecimal(index & 0xF);
}

const char* sizeKeyword(int bits) noexcept {
    if (bits == 8) {
        return "byte ptr ";
    }
    if (bits == 16) {
        return "word ptr ";
    }
    if (bits == 32) {
        return "dword ptr ";
    }
    return "qword ptr ";
}

void writeMemory(Writer& writer, const Rm& rm, const char* segment) noexcept {
    writer.add('[');
    if (segment != nullptr) {
        writer.add(segment);
    }
    if (rm.ripRelative) {
        writer.add("rip");
        if (rm.displacementSize != 0) {
            writer.add(rm.displacement < 0 ? '-' : '+');
            const auto magnitude = rm.displacement < 0
                                       ? -static_cast<std::int64_t>(rm.displacement)
                                       : static_cast<std::int64_t>(rm.displacement);
            writer.addHex(static_cast<std::uint64_t>(magnitude));
        }
        writer.add(']');
        return;
    }

    bool first = true;
    if (rm.base != 0xFF) {
        writeRegister(writer, rm.base, 64, true);
        first = false;
    }
    if (rm.index != 0x4 || rm.scale != 0) {
        if (!first) {
            writer.add('+');
        }
        writeRegister(writer, rm.index, 64, true);
        if (rm.scale != 0) {
            writer.add('*');
            writer.add(static_cast<char>('0' + (1 << rm.scale)));
        }
        first = false;
    }
    if (rm.displacementSize != 0) {
        if (!first || rm.base == 0xFF) {
            writer.add(rm.displacement < 0 ? '-' : '+');
        }
        const auto magnitude = rm.displacement < 0
                                   ? -static_cast<std::int64_t>(rm.displacement)
                                   : static_cast<std::int64_t>(rm.displacement);
        writer.addHex(static_cast<std::uint64_t>(magnitude));
    } else if (rm.base == 0xFF) {
        writer.add('0');
    }
    writer.add(']');
}

// operando r/m inteiro: registo ou memoria com a palavra de tamanho pedida.
void writeRm(Writer& writer, const Rm& rm, int bits, bool withSizeKeyword,
             const char* segment) noexcept {
    if (!rm.memory) {
        writeRegister(writer, rm.rm, bits, (rm.rm & 0x8) != 0);
        return;
    }
    if (withSizeKeyword) {
        writer.add(sizeKeyword(bits));
    }
    writeMemory(writer, rm, segment);
}

// operando r/m de SSE: registo xmm ou memoria sem palavra de tamanho.
void writeXmmRm(Writer& writer, const Rm& rm, const char* segment) noexcept {
    if (!rm.memory) {
        writeXmm(writer, rm.rm);
        return;
    }
    writeMemory(writer, rm, segment);
}

[[nodiscard]] bool readImmediate(Reader& reader, int bits, std::int64_t& out) noexcept {
    if (bits == 8) {
        std::uint8_t value = 0;
        if (!reader.byte(value)) {
            return false;
        }
        out = value;
        return true;
    }
    if (bits == 16) {
        std::uint8_t bytes[2];
        for (auto& piece : bytes) {
            if (!reader.byte(piece)) {
                return false;
            }
        }
        const auto value = static_cast<std::uint32_t>(bytes[0]) |
                           (static_cast<std::uint32_t>(bytes[1]) << 8);
        out = static_cast<std::uint16_t>(value);
        return true;
    }
    std::uint32_t value = 0;
    if (!reader.imm32(value)) {
        return false;
    }
    out = value;
    return true;
}

} // namespace

Decoded decode(const std::uint8_t* data, std::size_t size) noexcept {
    Decoded out;
    Reader reader {data, size};
    Writer writer {out.text};

    // -- prefixos legados e REX --
    bool opSize16 {false};
    bool rep {false};
    bool repne {false};
    std::uint8_t rex {0};
    const char* segment {nullptr};
    std::uint8_t opcode = 0;

    for (;;) {
        if (!reader.byte(opcode)) {
            return out; // nada a decodificar
        }
        if (opcode == 0x66) {
            opSize16 = true;
            continue;
        }
        if (opcode == 0x67 || opcode == 0xF0 || opcode == 0x2E || opcode == 0x36 ||
            opcode == 0x3E || opcode == 0x26) {
            continue;
        }
        if (opcode == 0x64) {
            segment = "fs:";
            continue;
        }
        if (opcode == 0x65) {
            segment = "gs:";
            continue;
        }
        if (opcode == 0xF2) {
            repne = true;
            continue;
        }
        if (opcode == 0xF3) {
            rep = true;
            continue;
        }
        if (opcode >= 0x40 && opcode <= 0x4F) {
            rex = opcode;
            continue;
        }
        break;
    }

    const bool rexUsed = rex != 0;
    const bool rexW = (rex & 0x8) != 0;
    const int defaultBits = rexW ? 64 : (opSize16 ? 16 : 32);
    const auto aluIndex = static_cast<std::uint8_t>((opcode >> 3) & 7);
    const auto condition = static_cast<std::uint8_t>(opcode & 0xF);

    // -- tabela principal, modo longo --
    if (opcode <= 0x3F && (opcode & 7) <= 5) {
        // grupo ALU: 00-3F, oito mnemonicos x seis formas.
        const int low = opcode & 7;
        Rm rm;
        switch (low) {
        case 0: // r/m8, r8
            if (!readModRm(reader, rex, rm)) {
                return out;
            }
            writer.add(kAlu[aluIndex]);
            writer.add(' ');
            writeRm(writer, rm, 8, true, segment);
            writer.add(", ");
            writeRegister(writer, rm.reg, 8, rexUsed);
            break;
        case 1: // r/m, r
            if (!readModRm(reader, rex, rm)) {
                return out;
            }
            writer.add(kAlu[aluIndex]);
            writer.add(' ');
            writeRm(writer, rm, defaultBits, true, segment);
            writer.add(", ");
            writeRegister(writer, rm.reg, defaultBits, rexUsed);
            break;
        case 2: // r8, r/m8
            if (!readModRm(reader, rex, rm)) {
                return out;
            }
            writer.add(kAlu[aluIndex]);
            writer.add(' ');
            writeRegister(writer, rm.reg, 8, rexUsed);
            writer.add(", ");
            writeRm(writer, rm, 8, true, segment);
            break;
        case 3: // r, r/m
            if (!readModRm(reader, rex, rm)) {
                return out;
            }
            writer.add(kAlu[aluIndex]);
            writer.add(' ');
            writeRegister(writer, rm.reg, defaultBits, rexUsed);
            writer.add(", ");
            writeRm(writer, rm, defaultBits, true, segment);
            break;
        case 4: { // al, imm8
            writer.add(kAlu[aluIndex]);
            writer.add(" al, ");
            std::int64_t value = 0;
            if (!readImmediate(reader, 8, value)) {
                return out;
            }
            writer.addHex(static_cast<std::uint64_t>(value));
            break;
        }
        default: { // eAX, imm
            writer.add(kAlu[aluIndex]);
            writer.add(' ');
            writeRegister(writer, 0, defaultBits, rexUsed);
            writer.add(", ");
            std::int64_t value = 0;
            if (!readImmediate(reader, defaultBits == 16 ? 16 : 32, value)) {
                return out;
            }
            writer.addHex(static_cast<std::uint64_t>(value));
            break;
        }
        }
    } else if (opcode >= 0x50 && opcode <= 0x5F) {
        writer.add((opcode & 8) == 0 ? "push " : "pop ");
        writeRegister(writer, opcodeRegister(static_cast<std::uint8_t>(opcode & 7), rex), 64,
                      true);
    } else if (opcode == 0x63) {
        Rm rm;
        if (!readModRm(reader, rex, rm)) {
            return out;
        }
        writer.add("movsxd ");
        writeRegister(writer, rm.reg, rexW ? 64 : 32, rexUsed);
        writer.add(", ");
        writeRm(writer, rm, 32, false, segment);
    } else if (opcode >= 0x70 && opcode <= 0x7F) {
        std::uint8_t value = 0;
        if (!reader.byte(value)) {
            return out;
        }
        writer.add("j");
        writer.add(kCondition[condition]);
        out.hasTarget = true;
        out.target = static_cast<std::int8_t>(value);
    } else if (opcode == 0x80 || opcode == 0x81 || opcode == 0x83) {
        Rm rm;
        if (!readModRm(reader, rex, rm)) {
            return out;
        }
        writer.add(kAlu[rm.reg & 7]);
        writer.add(' ');
        writeRm(writer, rm, opcode == 0x80 ? 8 : defaultBits, true, segment);
        writer.add(", ");
        std::int64_t value = 0;
        if (!readImmediate(reader, opcode == 0x81 ? (defaultBits == 16 ? 16 : 32) : 8, value)) {
            return out;
        }
        writer.addHex(static_cast<std::uint64_t>(value));
    } else if (opcode == 0x84 || opcode == 0x85) {
        Rm rm;
        if (!readModRm(reader, rex, rm)) {
            return out;
        }
        writer.add("test ");
        writeRm(writer, rm, opcode == 0x84 ? 8 : defaultBits, true, segment);
        writer.add(", ");
        writeRegister(writer, rm.reg, opcode == 0x84 ? 8 : defaultBits, rexUsed);
    } else if (opcode == 0x86 || opcode == 0x87) {
        Rm rm;
        if (!readModRm(reader, rex, rm)) {
            return out;
        }
        writer.add("xchg ");
        writeRm(writer, rm, opcode == 0x86 ? 8 : defaultBits, false, segment);
        writer.add(", ");
        writeRegister(writer, rm.reg, opcode == 0x86 ? 8 : defaultBits, rexUsed);
    } else if (opcode >= 0x88 && opcode <= 0x8B) {
        Rm rm;
        if (!readModRm(reader, rex, rm)) {
            return out;
        }
        const int bits = ((opcode & 1) != 0) ? defaultBits : 8;
        const bool toRm = (opcode & 2) == 0;
        writer.add("mov ");
        if (toRm) {
            writeRm(writer, rm, bits, true, segment);
            writer.add(", ");
            writeRegister(writer, rm.reg, bits, rexUsed);
        } else {
            writeRegister(writer, rm.reg, bits, rexUsed);
            writer.add(", ");
            writeRm(writer, rm, bits, true, segment);
        }
    } else if (opcode == 0x8D) {
        Rm rm;
        if (!readModRm(reader, rex, rm)) {
            return out;
        }
        writer.add("lea ");
        writeRegister(writer, rm.reg, defaultBits, rexUsed);
        writer.add(", ");
        writeMemory(writer, rm, segment);
    } else if (opcode == 0x8F) {
        Rm rm;
        if (!readModRm(reader, rex, rm)) {
            return out;
        }
        writer.add("pop ");
        writeRm(writer, rm, defaultBits, false, segment);
    } else if (opcode == 0x90 && !rexUsed) {
        writer.add("nop");
    } else if (opcode >= 0x90 && opcode <= 0x97) {
        writer.add("xchg rax, ");
        writeRegister(writer, opcodeRegister(static_cast<std::uint8_t>(opcode & 7), rex), 64,
                      true);
    } else if (opcode == 0x98) {
        writer.add(rexW ? "cdqe" : "cwde");
    } else if (opcode == 0x99) {
        writer.add(rexW ? "cqo" : "cdq");
    } else if (opcode == 0x9B) {
        writer.add("wait");
    } else if (opcode == 0x9C) {
        writer.add("pushf");
    } else if (opcode == 0x9D) {
        writer.add("popf");
    } else if (opcode >= 0xA0 && opcode <= 0xA3) {
        // mov moffs: A0 al<-[m], A1 eAX<-[m], A2 [m]<-al, A3 [m]<-eAX. o
        // endereco e' de 64 bits no modo longo e sai inteiro no texto.
        std::uint64_t address = 0;
        if (!reader.imm64(address)) {
            return out;
        }
        writer.add("mov ");
        const int bits = ((opcode & 1) != 0) ? defaultBits : 8;
        const bool toAccumulator = (opcode & 2) == 0;
        if (toAccumulator) {
            writeRegister(writer, 0, bits, rexUsed);
            writer.add(", ");
        }
        writer.add(sizeKeyword(bits));
        writer.add('[');
        writer.addHex(address);
        writer.add(']');
        if (!toAccumulator) {
            writer.add(", ");
            writeRegister(writer, 0, bits, rexUsed);
        }
    } else if (opcode >= 0xA4 && opcode <= 0xA7) {
        writer.add(rep ? "rep " : "");
        writer.add((opcode & 1) == 0 ? "movsb" : (opSize16 ? "movsw" : "movsd"));
    } else if (opcode == 0xA8 || opcode == 0xA9) {
        writer.add("test ");
        writeRegister(writer, 0, opcode == 0xA8 ? 8 : defaultBits, rexUsed);
        writer.add(", ");
        std::int64_t value = 0;
        if (!readImmediate(reader, opcode == 0xA8 ? 8 : (defaultBits == 16 ? 16 : 32), value)) {
            return out;
        }
        writer.addHex(static_cast<std::uint64_t>(value));
    } else if (opcode >= 0xAA && opcode <= 0xAF) {
        writer.add(rep ? "rep " : "");
        switch (opcode) {
        case 0xAA: writer.add("stosb"); break;
        case 0xAB: writer.add(opSize16 ? "stosw" : "stosd"); break;
        case 0xAC: writer.add("lodsb"); break;
        case 0xAD: writer.add(opSize16 ? "lodsw" : "lodsd"); break;
        case 0xAE: writer.add("scasb"); break;
        default: writer.add(opSize16 ? "scasw" : "scasd"); break;
        }
    } else if (opcode >= 0xB0 && opcode <= 0xB7) {
        writer.add("mov ");
        writeRegister(writer, opcodeRegister(static_cast<std::uint8_t>(opcode & 7), rex), 8,
                      true);
        writer.add(", ");
        std::int64_t value = 0;
        if (!readImmediate(reader, 8, value)) {
            return out;
        }
        writer.addHex(static_cast<std::uint64_t>(value));
    } else if (opcode >= 0xB8 && opcode <= 0xBF) {
        writer.add("mov ");
        writeRegister(writer, opcodeRegister(static_cast<std::uint8_t>(opcode & 7), rex),
                      defaultBits, true);
        writer.add(", ");
        if (rexW) {
            std::uint64_t value = 0;
            if (!reader.imm64(value)) {
                return out;
            }
            writer.addHex(value);
        } else {
            std::int64_t value = 0;
            if (!readImmediate(reader, defaultBits == 16 ? 16 : 32, value)) {
                return out;
            }
            writer.addHex(static_cast<std::uint64_t>(value));
        }
    } else if (opcode == 0xC0 || opcode == 0xC1 || opcode == 0xD0 || opcode == 0xD1 ||
               opcode == 0xD2 || opcode == 0xD3) {
        Rm rm;
        if (!readModRm(reader, rex, rm)) {
            return out;
        }
        const int bits = (opcode == 0xC0 || opcode == 0xD0 || opcode == 0xD2) ? 8
                                                                              : defaultBits;
        writer.add(kShift[rm.reg & 7]);
        writer.add(' ');
        writeRm(writer, rm, bits, true, segment);
        if (opcode == 0xC0 || opcode == 0xC1) {
            writer.add(", ");
            std::int64_t value = 0;
            if (!readImmediate(reader, 8, value)) {
                return out;
            }
            writer.addHex(static_cast<std::uint64_t>(value));
        } else if (opcode == 0xD2 || opcode == 0xD3) {
            writer.add(", cl");
        }
    } else if (opcode == 0xC2) {
        std::uint8_t low = 0;
        std::uint8_t high = 0;
        if (!reader.byte(low) || !reader.byte(high)) {
            return out;
        }
        writer.add("ret ");
        writer.addHex(static_cast<std::uint64_t>(low) |
                      (static_cast<std::uint64_t>(high) << 8));
    } else if (opcode == 0xC3) {
        writer.add("ret");
    } else if (opcode == 0xC6 || opcode == 0xC7) {
        Rm rm;
        if (!readModRm(reader, rex, rm)) {
            return out;
        }
        writer.add("mov ");
        writeRm(writer, rm, opcode == 0xC6 ? 8 : defaultBits, true, segment);
        writer.add(", ");
        std::int64_t value = 0;
        if (!readImmediate(reader, opcode == 0xC6 ? 8 : (defaultBits == 16 ? 16 : 32), value)) {
            return out;
        }
        writer.addHex(static_cast<std::uint64_t>(value));
    } else if (opcode == 0xC9) {
        writer.add("leave");
    } else if (opcode == 0xCC) {
        writer.add("int3");
    } else if (opcode == 0xCD) {
        std::uint8_t value = 0;
        if (!reader.byte(value)) {
            return out;
        }
        writer.add("int ");
        writer.addHex(value);
    } else if (opcode == 0xE0 || opcode == 0xE1 || opcode == 0xE2 || opcode == 0xE3) {
        std::uint8_t value = 0;
        if (!reader.byte(value)) {
            return out;
        }
        writer.add(opcode == 0xE0 ? "loopne"
                                  : opcode == 0xE1 ? "loope" : opcode == 0xE2 ? "loop" : "jrcxz");
        out.hasTarget = true;
        out.target = static_cast<std::int8_t>(value);
    } else if (opcode == 0xE8) {
        std::uint32_t value = 0;
        if (!reader.imm32(value)) {
            return out;
        }
        writer.add("call");
        out.hasTarget = true;
        out.target = static_cast<std::int32_t>(value);
    } else if (opcode == 0xE9) {
        std::uint32_t value = 0;
        if (!reader.imm32(value)) {
            return out;
        }
        writer.add("jmp");
        out.hasTarget = true;
        out.target = static_cast<std::int32_t>(value);
    } else if (opcode == 0xEB) {
        std::uint8_t value = 0;
        if (!reader.byte(value)) {
            return out;
        }
        writer.add("jmp");
        out.hasTarget = true;
        out.target = static_cast<std::int8_t>(value);
    } else if (opcode == 0xF4) {
        writer.add("hlt");
    } else if (opcode == 0xF5) {
        writer.add("cmc");
    } else if (opcode == 0xF6 || opcode == 0xF7) {
        Rm rm;
        if (!readModRm(reader, rex, rm)) {
            return out;
        }
        const int bits = opcode == 0xF6 ? 8 : defaultBits;
        switch (rm.reg & 7) {
        case 0:
        case 1: {
            writer.add("test ");
            writeRm(writer, rm, bits, true, segment);
            writer.add(", ");
            std::int64_t value = 0;
            if (!readImmediate(reader, bits == 16 ? 16 : (bits == 8 ? 8 : 32), value)) {
                return out;
            }
            writer.addHex(static_cast<std::uint64_t>(value));
            break;
        }
        case 2: writer.add("not "); writeRm(writer, rm, bits, true, segment); break;
        case 3: writer.add("neg "); writeRm(writer, rm, bits, true, segment); break;
        case 4: writer.add("mul "); writeRm(writer, rm, bits, true, segment); break;
        case 5: writer.add("imul "); writeRm(writer, rm, bits, true, segment); break;
        case 6: writer.add("div "); writeRm(writer, rm, bits, true, segment); break;
        default: writer.add("idiv "); writeRm(writer, rm, bits, true, segment); break;
        }
    } else if (opcode == 0xF8) {
        writer.add("clc");
    } else if (opcode == 0xF9) {
        writer.add("stc");
    } else if (opcode == 0xFA) {
        writer.add("cli");
    } else if (opcode == 0xFB) {
        writer.add("sti");
    } else if (opcode == 0xFC) {
        writer.add("cld");
    } else if (opcode == 0xFD) {
        writer.add("std");
    } else if (opcode == 0xFE || opcode == 0xFF) {
        Rm rm;
        if (!readModRm(reader, rex, rm)) {
            return out;
        }
        const int bits = opcode == 0xFE ? 8 : defaultBits;
        switch (rm.reg & 7) {
        case 0: writer.add("inc "); writeRm(writer, rm, bits, true, segment); break;
        case 1: writer.add("dec "); writeRm(writer, rm, bits, true, segment); break;
        case 2: writer.add("call "); writeRm(writer, rm, bits, false, segment); break;
        case 3: writer.add("call far "); writeRm(writer, rm, bits, false, segment); break;
        case 4: writer.add("jmp "); writeRm(writer, rm, bits, false, segment); break;
        case 5: writer.add("jmp far "); writeRm(writer, rm, bits, false, segment); break;
        case 6: writer.add("push "); writeRm(writer, rm, bits, false, segment); break;
        default: writer.add("bad"); break;
        }
    } else if (opcode == 0x0F) {
        // -- escape two-byte --
        std::uint8_t second = 0;
        if (!reader.byte(second)) {
            return out;
        }
        const auto cond2 = static_cast<std::uint8_t>(second & 0xF);

        if (second == 0x05) {
            writer.add("syscall");
        } else if (second == 0x0B) {
            writer.add("ud2");
        } else if (second == 0x1E) {
            std::uint8_t modrm = 0;
            if (!reader.byte(modrm)) {
                return out;
            }
            writer.add(modrm == 0xFA ? "endbr64" : modrm == 0xFB ? "endbr32" : "nop");
        } else if (second == 0x1F) {
            Rm rm;
            if (!readModRm(reader, rex, rm)) {
                return out;
            }
            writer.add("nop ");
            writeRm(writer, rm, defaultBits, true, segment);
        } else if (second == 0x10 || second == 0x11) {
            Rm rm;
            if (!readModRm(reader, rex, rm)) {
                return out;
            }
            writer.add(rep ? "movss" : repne ? "movsd" : opSize16 ? "movupd" : "movups");
            if (second == 0x11) {
                writer.add(' ');
                writeXmmRm(writer, rm, segment);
                writer.add(", ");
                writeXmm(writer, rm.reg);
            } else {
                writer.add(' ');
                writeXmm(writer, rm.reg);
                writer.add(", ");
                writeXmmRm(writer, rm, segment);
            }
        } else if (second == 0x28 || second == 0x29) {
            Rm rm;
            if (!readModRm(reader, rex, rm)) {
                return out;
            }
            writer.add(opSize16 ? "movapd" : "movaps");
            if (second == 0x29) {
                writer.add(' ');
                writeXmmRm(writer, rm, segment);
                writer.add(", ");
                writeXmm(writer, rm.reg);
            } else {
                writer.add(' ');
                writeXmm(writer, rm.reg);
                writer.add(", ");
                writeXmmRm(writer, rm, segment);
            }
        } else if (second == 0x2E || second == 0x2F) {
            Rm rm;
            if (!readModRm(reader, rex, rm)) {
                return out;
            }
            writer.add(second == 0x2E ? (opSize16 ? "ucomisd" : "ucomiss")
                                      : (opSize16 ? "comisd" : "comiss"));
            writer.add(' ');
            writeXmm(writer, rm.reg);
            writer.add(", ");
            writeXmmRm(writer, rm, segment);
        } else if (second >= 0x54 && second <= 0x57) {
            Rm rm;
            if (!readModRm(reader, rex, rm)) {
                return out;
            }
            static const char* const kLogic[4] {"andps", "andnps", "orps", "xorps"};
            writer.add(opSize16 ? "andpd" : kLogic[second - 0x54]);
            writer.add(' ');
            writeXmm(writer, rm.reg);
            writer.add(", ");
            writeXmmRm(writer, rm, segment);
        } else if (second == 0x6E) {
            Rm rm;
            if (!readModRm(reader, rex, rm)) {
                return out;
            }
            writer.add(opSize16 ? "movq" : "movd");
            writer.add(' ');
            writeXmm(writer, rm.reg);
            writer.add(", ");
            writeRm(writer, rm, opSize16 ? 64 : 32, false, segment);
        } else if (second == 0x6F) {
            Rm rm;
            if (!readModRm(reader, rex, rm)) {
                return out;
            }
            writer.add(rep ? "movdqu" : opSize16 ? "movdqa" : "movq");
            writer.add(' ');
            writeXmm(writer, rm.reg);
            writer.add(", ");
            writeXmmRm(writer, rm, segment);
        } else if (second == 0x7E) {
            Rm rm;
            if (!readModRm(reader, rex, rm)) {
                return out;
            }
            writer.add(opSize16 ? "movq" : "movd");
            writer.add(' ');
            writeXmm(writer, rm.reg);
            writer.add(", ");
            writeRm(writer, rm, opSize16 ? 64 : 32, false, segment);
        } else if (second == 0x7F) {
            Rm rm;
            if (!readModRm(reader, rex, rm)) {
                return out;
            }
            writer.add(rep ? "movdqu" : opSize16 ? "movdqa" : "movq");
            writer.add(' ');
            writeXmmRm(writer, rm, segment);
            writer.add(", ");
            writeXmm(writer, rm.reg);
        } else if (second == 0xEF) {
            Rm rm;
            if (!readModRm(reader, rex, rm)) {
                return out;
            }
            writer.add(opSize16 ? "pxor" : "emms");
            writer.add(' ');
            writeXmm(writer, rm.reg);
            writer.add(", ");
            writeXmmRm(writer, rm, segment);
        } else if (second == 0x31) {
            writer.add("rdtsc");
        } else if (second == 0xA2) {
            writer.add("cpuid");
        } else if (second >= 0x80 && second <= 0x8F) {
            std::uint32_t value = 0;
            if (!reader.imm32(value)) {
                return out;
            }
            writer.add("j");
            writer.add(kCondition[cond2]);
            out.hasTarget = true;
            out.target = static_cast<std::int32_t>(value);
        } else if (second >= 0x90 && second <= 0x9F) {
            Rm rm;
            if (!readModRm(reader, rex, rm)) {
                return out;
            }
            writer.add("set");
            writer.add(kCondition[cond2]);
            writer.add(' ');
            writeRm(writer, rm, 8, false, segment);
        } else if (second >= 0x40 && second <= 0x4F) {
            Rm rm;
            if (!readModRm(reader, rex, rm)) {
                return out;
            }
            writer.add("cmov");
            writer.add(kCondition[cond2]);
            writer.add(' ');
            writeRegister(writer, rm.reg, defaultBits, rexUsed);
            writer.add(", ");
            writeRm(writer, rm, defaultBits, true, segment);
        } else if (second == 0xA0) {
            writer.add("push fs");
        } else if (second == 0xA1) {
            writer.add("pop fs");
        } else if (second == 0xA8) {
            writer.add("push gs");
        } else if (second == 0xA9) {
            writer.add("pop gs");
        } else if (second == 0xA3 || second == 0xAB) {
            Rm rm;
            if (!readModRm(reader, rex, rm)) {
                return out;
            }
            writer.add(second == 0xA3 ? "bt " : "bts ");
            writeRm(writer, rm, defaultBits, false, segment);
            writer.add(", ");
            writeRegister(writer, rm.reg, defaultBits, rexUsed);
        } else if (second == 0xA4 || second == 0xAC) {
            Rm rm;
            if (!readModRm(reader, rex, rm)) {
                return out;
            }
            writer.add("shld ");
            writeRm(writer, rm, defaultBits, false, segment);
            writer.add(", ");
            writeRegister(writer, rm.reg, defaultBits, rexUsed);
            if (second == 0xA4) {
                writer.add(", ");
                std::int64_t value = 0;
                if (!readImmediate(reader, 8, value)) {
                    return out;
                }
                writer.addHex(static_cast<std::uint64_t>(value));
            } else {
                writer.add(", cl");
            }
        } else if (second == 0xA5 || second == 0xAD) {
            Rm rm;
            if (!readModRm(reader, rex, rm)) {
                return out;
            }
            writer.add("shld ");
            writeRm(writer, rm, defaultBits, false, segment);
            writer.add(", ");
            writeRegister(writer, rm.reg, defaultBits, rexUsed);
            writer.add(", cl");
        } else if (second == 0xAF) {
            Rm rm;
            if (!readModRm(reader, rex, rm)) {
                return out;
            }
            writer.add("imul ");
            writeRegister(writer, rm.reg, defaultBits, rexUsed);
            writer.add(", ");
            writeRm(writer, rm, defaultBits, true, segment);
        } else if (second == 0xB0 || second == 0xB1) {
            Rm rm;
            if (!readModRm(reader, rex, rm)) {
                return out;
            }
            writer.add("cmpxchg ");
            writeRm(writer, rm, second == 0xB0 ? 8 : defaultBits, true, segment);
            writer.add(", ");
            writeRegister(writer, rm.reg, second == 0xB0 ? 8 : defaultBits, rexUsed);
        } else if (second == 0xB3 || second == 0xBB) {
            Rm rm;
            if (!readModRm(reader, rex, rm)) {
                return out;
            }
            writer.add(second == 0xB3 ? "btr " : "btc ");
            writeRm(writer, rm, defaultBits, false, segment);
            writer.add(", ");
            writeRegister(writer, rm.reg, defaultBits, rexUsed);
        } else if (second == 0xB6 || second == 0xB7 || second == 0xBE || second == 0xBF) {
            Rm rm;
            if (!readModRm(reader, rex, rm)) {
                return out;
            }
            writer.add(second <= 0xB7 ? "movzx " : "movsx ");
            const int sourceBits = (second == 0xB6 || second == 0xBE) ? 8 : 16;
            writeRegister(writer, rm.reg, defaultBits, rexUsed);
            writer.add(", ");
            writeRm(writer, rm, sourceBits, true, segment);
        } else if (second == 0xBC || second == 0xBD) {
            Rm rm;
            if (!readModRm(reader, rex, rm)) {
                return out;
            }
            writer.add(second == 0xBC ? "bsf " : "bsr ");
            writeRegister(writer, rm.reg, defaultBits, rexUsed);
            writer.add(", ");
            writeRm(writer, rm, defaultBits, true, segment);
        } else if (second == 0xB8) {
            Rm rm;
            if (!readModRm(reader, rex, rm)) {
                return out;
            }
            if (rep) {
                writer.add("popcnt ");
                writeRegister(writer, rm.reg, defaultBits, rexUsed);
                writer.add(", ");
                writeRm(writer, rm, defaultBits, true, segment);
            } else {
                // grupo 8, formas indefinidas para reg 0..3
                writer.add("bad");
            }
        } else if (second == 0xBA) {
            Rm rm;
            if (!readModRm(reader, rex, rm)) {
                return out;
            }
            if ((rm.reg & 7) < 4) {
                writer.add("bad");
            } else {
                writer.add((rm.reg & 7) == 4 ? "bt "
                                             : (rm.reg & 7) == 5 ? "bts "
                                                                 : (rm.reg & 7) == 6 ? "btr"
                                                                                     : "btc ");
                writeRm(writer, rm, defaultBits, false, segment);
                writer.add(", ");
                std::int64_t value = 0;
                if (!readImmediate(reader, 8, value)) {
                    return out;
                }
                writer.addHex(static_cast<std::uint64_t>(value));
            }
        } else if (second == 0xC0 || second == 0xC1) {
            Rm rm;
            if (!readModRm(reader, rex, rm)) {
                return out;
            }
            writer.add("xadd ");
            writeRm(writer, rm, second == 0xC0 ? 8 : defaultBits, true, segment);
            writer.add(", ");
            writeRegister(writer, rm.reg, second == 0xC0 ? 8 : defaultBits, rexUsed);
        } else if (second >= 0xC8 && second <= 0xCF) {
            writer.add("bswap ");
            writeRegister(writer, opcodeRegister(static_cast<std::uint8_t>(second & 7), rex),
                          32, true);
        } else {
            writer.add("db 0x0F, ");
            writer.addHex(second);
        }
    } else {
        writer.add("db ");
        writer.addHex(opcode);
    }

    out.length = static_cast<std::uint32_t>(reader.consumed());
    if (out.length > 15) {
        // instrucao maior que o teto da arquitetura: o decode errou o caminho.
        out.length = 0;
        out.hasTarget = false;
        out.text.fill('\0');
    }
    return out;
}

} // namespace opcoda::disasm
