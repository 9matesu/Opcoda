#include <gtest/gtest.h>

#include "opcoda_core/pe/pe_parser.h"
#include "pe_builder.h"

#include <cstddef>
#include <cstdint>
#include <vector>

using opcoda::pe::PeError;
using opcoda::pe::parse;
using opcoda::pe::toString;
using opcoda::test::PeBuilder;

// As 50 mutacoes do T3.
//
// O portao D exige que 100% das entradas malformadas sejam rejeitadas com erro
// tipado. Este ficheiro e a versao deterministica dessa exigencia: nao ha
// libFuzzer nesta maquina (docs/17), ha 50 entradas fixas que corroem cada
// campo que o parser le.
//
// Cada mutacao afirma duas coisas, e as duas importam. Quando a mutacao tem
// erro esperado, o erro tem de ser esse e nao outro. Quando o parser aceita,
// os limites da imagem tem de respeitar o ficheiro que lhe foi dado.
//
// A segunda verificacao e a que impede que este ficheiro vire decoracao. Um
// parser que devolvesse kOk com uma secao a ler 4 GB alem do fim passaria num
// teste que so confirmasse "nao crashou", e o ASan nao veria nada: a leitura
// errada devolve bytes do heap e passa despercebida. E por isso que a
// verificacao de limites vive no resultado, e nao apenas na sobrevivencia do
// processo.

namespace {

// Invariantes que valem para qualquer imagem que o parser aceite. Nao sao
// documentacao: sao as condicoes que o resto do nucleo assume ao ler
// PeImage, e um parser que as violasse leva a uma leitura fora do buffer a
// jusante.
void expectImageWithinFile(const opcoda::pe::PeParseResult& result, std::size_t size) {
    ASSERT_TRUE(result.ok()) << toString(result.error);
    ASSERT_LE(result.image.numberOfSections, 96u);

    std::size_t summedRaw = 0;
    for (std::size_t i = 0; i < result.image.numberOfSections; ++i) {
        const auto& section = result.image.sections[i];
        const auto offset = static_cast<std::size_t>(section.rawOffset);
        const auto rawSize = static_cast<std::size_t>(section.rawSize);
        EXPECT_LE(offset, size) << "secao " << i << " (" << section.name << ") comecia alem do ficheiro";
        EXPECT_LE(rawSize, size - offset)
            << "secao " << i << " (" << section.name << ") terminava alem do ficheiro";
        EXPECT_GE(section.entropy, 0.0);
        EXPECT_LE(section.entropy, 8.0);
        EXPECT_EQ(section.name[8], '\0') << "nome de secao sem terminador";
        summedRaw += rawSize;
    }
    EXPECT_EQ(result.image.sampleCount, summedRaw)
        << "sampleCount diverge da soma das secoes";
}

struct Mutation {
    const char* name;
    void (*apply)(PeBuilder&);
    PeError expected;
};

// --- DOS header -----------------------------------------------------------

void dosFlipFirstMagicByte(PeBuilder& b) { b.bytes[0] = 'm'; }
void dosFlipSecondMagicByte(PeBuilder& b) { b.bytes[1] = 0x00; }
void dosReplaceSecondMagicByte(PeBuilder& b) { b.bytes[1] = 'Y'; }
void dosELfanewToZero(PeBuilder& b) { b.writeU32(0x3C, 0); }
void dosELfanewLowBit(PeBuilder& b) { b.writeU32(0x3C, PeBuilder::kELfanew + 1); }
void dosELfanewHighBit(PeBuilder& b) { b.writeU32(0x3C, PeBuilder::kELfanew ^ 0x80000000u); }
void dosELfanewAllOnes(PeBuilder& b) { b.writeU32(0x3C, 0xFFFFFFFFu); }
void dosELfanewMaxInt31(PeBuilder& b) { b.writeU32(0x3C, 0x7FFFFFFFu); }
void dosELfanewPastEnd(PeBuilder& b) { b.writeU32(0x3C, static_cast<std::uint32_t>(PeBuilder::kTotalSize)); }
void dosELfanewLastValidByte(PeBuilder& b) { b.writeU32(0x3C, static_cast<std::uint32_t>(PeBuilder::kTotalSize - 24)); }
void dosELfanewZeroByte(PeBuilder& b) { b.bytes[0x3C] = 0; }
void dosELfanewMaxByte(PeBuilder& b) { b.bytes[0x3C] = 0xFF; }
void dosTruncateBeforeSignature(PeBuilder& b) { b.bytes.resize(0x40); }
void dosTruncateToHeaderOnly(PeBuilder& b) { b.bytes.resize(PeBuilder::kSignatureOffset); }

// --- assinatura PE --------------------------------------------------------

void peFlipFirstByte(PeBuilder& b) { b.bytes[PeBuilder::kSignatureOffset] = 'X'; }
void peFlipSecondByte(PeBuilder& b) { b.bytes[PeBuilder::kSignatureOffset + 1] = 0; }
void peSignatureThirdByteSet(PeBuilder& b) { b.bytes[PeBuilder::kSignatureOffset + 2] = 1; }
void peSignatureFourthByteSet(PeBuilder& b) { b.bytes[PeBuilder::kSignatureOffset + 3] = 0xFF; }
void peSignatureSwapBytes(PeBuilder& b) {
    b.bytes[PeBuilder::kSignatureOffset] = 'E';
    b.bytes[PeBuilder::kSignatureOffset + 1] = 'P';
}

// --- COFF header ----------------------------------------------------------

void coffSectionsZero(PeBuilder& b) { b.writeU16(PeBuilder::kCoffOffset + 2, 0); }
void coffSectionsOneAboveMax(PeBuilder& b) { b.writeU16(PeBuilder::kCoffOffset + 2, 97); }
void coffSectionsMaxUint16(PeBuilder& b) { b.writeU16(PeBuilder::kCoffOffset + 2, 0xFFFF); }
void coffSectionsThirtyTwo(PeBuilder& b) { b.writeU16(PeBuilder::kCoffOffset + 2, 32); }
void coffOptionalHeaderZero(PeBuilder& b) { b.writeU16(PeBuilder::kCoffOffset + 16, 0); }
void coffOptionalHeaderOneBelow(PeBuilder& b) {
    b.writeU16(PeBuilder::kCoffOffset + 16, static_cast<std::uint16_t>(PeBuilder::kOptionalSize - 1));
}
void coffOptionalHeaderLowByteZero(PeBuilder& b) { b.bytes[PeBuilder::kCoffOffset + 16] = 0; }
void coffOptionalHeaderMaxUint16(PeBuilder& b) { b.writeU16(PeBuilder::kCoffOffset + 16, 0xFFFF); }

// --- tabela de secoes -----------------------------------------------------

void tableSecondSectionCountRaises(PeBuilder& b) {
    // Anuncia 64 secoes num ficheiro que tem espaco para uma. A tabela
    // comeca em 0xF8 e cada entrada tem 40 bytes, portanto 64 entradas
    // acabariam em 0xF8 + 2560, muito alem dos 0x300 do ficheiro. Com 8 nao
    // transbordava: 0xF8 + 320 = 0x238 ainda cabia, e o parser devolvia E_OK
    // porque a leitura cabia mesmo. O limite que o parser tem de apanhar e
    // a soma, nao o numero de secoes.
    b.writeU16(PeBuilder::kCoffOffset + 2, 64);
}
void tableRawSizeMax24Bit(PeBuilder& b) {
    b.writeU32(PeBuilder::kTableOffset + 16, 0x00FFFFFFu);
}
void tableRawSizePastFile(PeBuilder& b) {
    b.writeU32(PeBuilder::kTableOffset + 16, static_cast<std::uint32_t>(PeBuilder::kTotalSize + 1));
}
void tableRawSizeAllOnes(PeBuilder& b) { b.writeU32(PeBuilder::kTableOffset + 16, 0xFFFFFFFFu); }
void tableRawOffsetAllOnes(PeBuilder& b) { b.writeU32(PeBuilder::kTableOffset + 20, 0xFFFFFFFFu); }
void tableRawOffsetAtEndOfFile(PeBuilder& b) {
    b.writeU32(PeBuilder::kTableOffset + 20, static_cast<std::uint32_t>(PeBuilder::kTotalSize));
}
void tableRawOffsetOneByteOfData(PeBuilder& b) {
    b.writeU32(PeBuilder::kTableOffset + 20, static_cast<std::uint32_t>(PeBuilder::kTotalSize - 1));
}
void tableRawSizeHighByteSet(PeBuilder& b) { b.bytes[PeBuilder::kTableOffset + 19] = 0x80; }
void tableVirtualSizeAllOnes(PeBuilder& b) { b.writeU32(PeBuilder::kTableOffset + 8, 0xFFFFFFFFu); }
void tableVirtualAddressAllOnes(PeBuilder& b) { b.writeU32(PeBuilder::kTableOffset + 12, 0xFFFFFFFFu); }
void tableSecondSectionRawSizePastFile(PeBuilder& b) {
    b.grow(0x100);
    b.addSection(".data", static_cast<std::uint32_t>(PeBuilder::kTotalSize), 0x100);
    b.writeU32(PeBuilder::kTableOffset + PeBuilder::kEntrySize + 16, 0x00FFFFFFu);
}
void tableSecondSectionRawOffsetPastFile(PeBuilder& b) {
    b.grow(0x100);
    b.addSection(".data", static_cast<std::uint32_t>(PeBuilder::kTotalSize), 0x100);
    b.writeU32(PeBuilder::kTableOffset + PeBuilder::kEntrySize + 20, 0x00FFFFFFu);
}
void tableZeroRawSizeStaysAccepted(PeBuilder& b) {
    // Nao e uma rejeicao: uma secao sem dados brutos, como .bss, e' valida.
    // Entra na lista porque o portao D conta tambem os casos que o parser deve
    // aceitar, e aceitar e' uma propriedade a verificar tanto como rejeitar.
    b.writeU32(PeBuilder::kTableOffset + 16, 0);
    b.writeU32(PeBuilder::kTableOffset + 20, 0);
}
void tableSizeOfHeadersAllOnes(PeBuilder& b) { b.writeU32(PeBuilder::kOptionalOffset + 60, 0xFFFFFFFFu); }
void tableSizeOfImageAllOnes(PeBuilder& b) { b.writeU32(PeBuilder::kOptionalOffset + 56, 0xFFFFFFFFu); }
void tableSizeOfImageZero(PeBuilder& b) { b.writeU32(PeBuilder::kOptionalOffset + 56, 0); }

// --- truncamento progressivo ----------------------------------------------

void truncateAtHalfFile(PeBuilder& b) { b.bytes.resize(PeBuilder::kTotalSize / 2); }
void truncateJustAfterSignature(PeBuilder& b) {
    // Corta logo depois dos 4 bytes da assinatura. A assinatura passa a estar
    // dentro do ficheiro, mas o parser exige os 4 + 20 do COFF para a
    // assinatura contar como valida, e o ficheiro acaba antes: o erro e' de
    // e_lfanew e nao de assinatura. E o que o teste discover e o que importa
    // registar, porque sao dois erros diferentes para o mesmo campo.
    b.bytes.resize(PeBuilder::kCoffOffset + 4);
}
void truncateInsideSectionTable(PeBuilder& b) { b.bytes.resize(PeBuilder::kTableOffset + 20); }
void truncateInsideFirstEntry(PeBuilder& b) { b.bytes.resize(PeBuilder::kTableOffset + 8); }
void truncateAt63Bytes(PeBuilder& b) { b.bytes.resize(63); }
void truncateTo64Bytes(PeBuilder& b) { b.bytes.resize(64); }
void truncateLeavingHeadersOnly(PeBuilder& b) { b.bytes.resize(PeBuilder::kHeaderEnd); }
void truncateInsideText(PeBuilder& b) { b.bytes.resize(PeBuilder::kHeaderEnd + 1); }

} // namespace

TEST(PeMutation, FiftyMutationsNeverCrashAndStayTyped) {
    // A lista e o ensaio. Cada entrada declara o erro que espera, e o teste
    // falha se o parser devolver outro, inclusive kOk onde se espera erro.
    const Mutation mutations[] = {
        // DOS header
        {"dos/magic-first-lowercase", dosFlipFirstMagicByte, PeError::kBadMz},
        {"dos/magic-second-zeroed", dosFlipSecondMagicByte, PeError::kBadMz},
        {"dos/magic-second-replaced", dosReplaceSecondMagicByte, PeError::kBadMz},
        {"dos/e_lfanew-zero", dosELfanewToZero, PeError::kBadPe},
        {"dos/e_lfanew-low-bit", dosELfanewLowBit, PeError::kBadPe},
        {"dos/e_lfanew-high-bit", dosELfanewHighBit, PeError::kBadELfanew},
        {"dos/e_lfanew-all-ones", dosELfanewAllOnes, PeError::kBadELfanew},
        {"dos/e_lfanew-max-int31", dosELfanewMaxInt31, PeError::kBadELfanew},
        {"dos/e_lfanew-past-end", dosELfanewPastEnd, PeError::kBadELfanew},
        {"dos/e_lfanew-last-valid-byte", dosELfanewLastValidByte, PeError::kBadPe},
        {"dos/e_lfanew-zero-byte", dosELfanewZeroByte, PeError::kBadPe},
        {"dos/truncated-before-signature", dosTruncateBeforeSignature, PeError::kBadELfanew},
        {"dos/truncated-to-header-only", dosTruncateToHeaderOnly, PeError::kBadELfanew},

        // assinatura PE
        {"pe/signature-first-byte", peFlipFirstByte, PeError::kBadPe},
        {"pe/signature-second-byte", peFlipSecondByte, PeError::kBadPe},
        {"pe/signature-third-byte", peSignatureThirdByteSet, PeError::kBadPe},
        {"pe/signature-fourth-byte", peSignatureFourthByteSet, PeError::kBadPe},
        {"pe/signature-swapped", peSignatureSwapBytes, PeError::kBadPe},

        // COFF header
        {"coff/number-of-sections-zero", coffSectionsZero, PeError::kTooManySections},
        {"coff/number-of-sections-97", coffSectionsOneAboveMax, PeError::kTooManySections},
        {"coff/number-of-sections-max-uint16", coffSectionsMaxUint16, PeError::kTooManySections},
        {"coff/number-of-sections-32", coffSectionsThirtyTwo, PeError::kBadSectionTable},
        {"coff/size-of-optional-header-zero", coffOptionalHeaderZero, PeError::kBadOptionalHeader},
        {"coff/size-of-optional-header-95", coffOptionalHeaderOneBelow, PeError::kBadOptionalHeader},
        {"coff/size-of-optional-header-low-zero", coffOptionalHeaderLowByteZero, PeError::kBadOptionalHeader},
        {"coff/size-of-optional-header-max", coffOptionalHeaderMaxUint16, PeError::kBadOptionalHeader},

        // tabela de secoes
        {"table/count-raises-past-table", tableSecondSectionCountRaises, PeError::kBadSectionTable},
        {"table/raw-size-max-24bit", tableRawSizeMax24Bit, PeError::kOob},
        {"table/raw-size-past-file", tableRawSizePastFile, PeError::kOob},
        {"table/raw-size-all-ones", tableRawSizeAllOnes, PeError::kOob},
        {"table/raw-offset-all-ones", tableRawOffsetAllOnes, PeError::kOob},
        {"table/raw-offset-at-eof", tableRawOffsetAtEndOfFile, PeError::kOob},
        {"table/raw-offset-one-byte-of-data", tableRawOffsetOneByteOfData, PeError::kOob},
        {"table/raw-size-high-byte", tableRawSizeHighByteSet, PeError::kOob},
        {"table/virtual-size-all-ones", tableVirtualSizeAllOnes, PeError::kOk},
        {"table/virtual-address-all-ones", tableVirtualAddressAllOnes, PeError::kOk},
        {"table/second-raw-size-past-file", tableSecondSectionRawSizePastFile, PeError::kOob},
        {"table/second-raw-offset-past-file", tableSecondSectionRawOffsetPastFile, PeError::kOob},
        {"table/zero-raw-size", tableZeroRawSizeStaysAccepted, PeError::kOk},
        {"table/size-of-headers-all-ones", tableSizeOfHeadersAllOnes, PeError::kOk},
        {"table/size-of-image-all-ones", tableSizeOfImageAllOnes, PeError::kOk},
        {"table/size-of-image-zero", tableSizeOfImageZero, PeError::kOk},

        // truncamento progressivo
        {"truncate/half-file", truncateAtHalfFile, PeError::kOob},
        {"truncate/after-signature", truncateJustAfterSignature, PeError::kBadELfanew},
        {"truncate/inside-section-table", truncateInsideSectionTable, PeError::kBadSectionTable},
        {"truncate/inside-first-entry", truncateInsideFirstEntry, PeError::kBadSectionTable},
        {"truncate/63-bytes", truncateAt63Bytes, PeError::kTooSmall},
        {"truncate/64-bytes", truncateTo64Bytes, PeError::kBadELfanew},
        {"truncate/headers-only", truncateLeavingHeadersOnly, PeError::kOob},
        {"truncate/inside-text", truncateInsideText, PeError::kOob},
    };

    ASSERT_EQ(std::size(mutations), 50u)
        << "o portao D conta 50 mutacoes; mudar o numero e mudar o criterio";

    for (const auto& mutation : mutations) {
        PeBuilder builder;
        mutation.apply(builder);

        const auto result = parse(builder.bytes.data(), builder.bytes.size());
        EXPECT_EQ(result.error, mutation.expected)
            << mutation.name << ": esperava " << toString(mutation.expected)
            << " e o parser devolveu " << toString(result.error);

        // Os limites so se verificam quando o parser aceita. Numa mutacao que
        // deve ser rejeitada, o resultado nao tem imagem para confirmar.
        if (result.ok()) {
            expectImageWithinFile(result, builder.bytes.size());
        }
    }
}

TEST(PeMutation, AcceptedMutationsStillRespectFileBounds) {
    // As quatro mutações que o parser aceita têm de continuar a produzir uma
    // imagem utilizável. Sao as que mais valem: sao as entradas em que um
    // parser permissivo aceitaria e o nucleo leria alem do fim.
    const Mutation accepted[] = {
        {"table/virtual-size-all-ones", tableVirtualSizeAllOnes, PeError::kOk},
        {"table/virtual-address-all-ones", tableVirtualAddressAllOnes, PeError::kOk},
        {"table/zero-raw-size", tableZeroRawSizeStaysAccepted, PeError::kOk},
        {"table/size-of-image-all-ones", tableSizeOfImageAllOnes, PeError::kOk},
    };

    for (const auto& mutation : accepted) {
        PeBuilder builder;
        mutation.apply(builder);

        const auto result = parse(builder.bytes.data(), builder.bytes.size());
        ASSERT_TRUE(result.ok()) << mutation.name << ": " << toString(result.error);
        expectImageWithinFile(result, builder.bytes.size());
    }
}

TEST(PeMutation, NoMutationLeaksSectionsBeyondFile) {
    // Varredura de um bit sobre todos os bytes que o parser le. O limite e' o
    // tamanho do buffer e nao um numero redondo: PeBuilder tem 0x300 bytes e os
    // campos do PEacabam antes disso, mas um literal aqui passaria do fim e o
    // assert do vector apareceria em vez do parser.
    PeBuilder reference;
    reference.fillTextWithGradient(0x00, 16);
    const std::size_t scanBytes = reference.bytes.size();

    std::size_t accepted = 0;
    std::size_t rejected = 0;

    for (std::size_t offset = 0; offset < scanBytes; ++offset) {
        for (int bit = 0; bit < 8; ++bit) {
            PeBuilder mutated;
            mutated.fillTextWithGradient(0x00, 16);
            mutated.bytes[offset] ^= static_cast<std::uint8_t>(1u << bit);

            const auto result = parse(mutated.bytes.data(), mutated.bytes.size());
            if (result.ok()) {
                ++accepted;
                expectImageWithinFile(result, mutated.bytes.size());
            } else {
                ++rejected;
                EXPECT_NE(result.error, PeError::kOk);
            }
        }
    }

    // Uma varredura que so aceita rejeitaria tudo e nao provaria nada; uma que
    // so aceita provaria que o parser recusa o que e valido. Exigimos as duas.
    EXPECT_GT(accepted, 0) << "nenhuma mutacao de 1 bit foi aceite: o parser recusou ate o que e legitimo";
    EXPECT_GT(rejected, 0) << "nenhuma mutacao de 1 bit foi rejeitada: o parser nao esta a verificar nada";
}
