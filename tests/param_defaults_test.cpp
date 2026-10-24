#include "param_defaults.h"

#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <set>
#include <string>

// O editor (Knob::Spec) e o layout (createParameterLayout) leem a mesma tabela
// de param_defaults.h. Estes testes guardam a tabela contra os dois defeitos que
// ela existe para fechar: ID repetido ou em falta, e default diferente do
// layout — o caso do lfodiv, que dizia "1/1" no editor e "1/4" no layout.
namespace {

// Oraculo independente: os 41 pares escritos a mao, nao derivados da tabela.
// Se um valor ou um ID mudar sem intencao, este teste cai. E' de proposito uma
// segunda lista: comparar a tabela com ela propria nao provaria nada.
struct ExpectedDefault {
    const char* id;
    float value;
};

constexpr std::array<ExpectedDefault, 41> kExpected {{
    {"grain", 40.0f},
    {"density", 20.0f},
    {"position", 0.5f},
    {"spray", 0.0f},
    {"pitch", 0.0f},
    {"volume", 0.0f},
    {"window", 0.0f},
    {"pan", 0.0f},
    {"grainlevel", 1.0f},
    {"pitchrand", 0.0f},
    {"cloudsize", 0.0f},
    {"pitchspread", 0.0f},
    {"grainjit", 0.0f},
    {"scanspeed", 1.0f},
    {"attack", 0.005f},
    {"decay", 0.1f},
    {"sustain", 1.0f},
    {"release", 0.05f},
    {"atkcurve", 0.0f},
    {"deccurve", 0.0f},
    {"relcurve", 0.0f},
    {"f1type", 0.0f},
    {"f1cutoff", 20000.0f},
    {"f1q", 0.7071f},
    {"f1slope", 0.0f},
    {"f1drive", 0.0f},
    {"f1keytrack", 0.0f},
    {"f1env", 0.0f},
    {"f2type", 0.0f},
    {"f2cutoff", 20000.0f},
    {"f2q", 0.7071f},
    {"lforate", 1.0f},
    {"lfodepth", 0.0f},
    {"lfotarget", 0.0f},
    {"lfowave", 0.0f},
    {"scale", 0.0f},
    {"root", 0.0f},
    {"glide", 0.0f},
    {"lfosync", 0.0f},
    {"lfodiv", 2.0f},
    {"lfosteps", 16.0f},
}};

const opcoda::params::Default* findById(const char* id) {
    for (const auto& def : opcoda::params::kDefaults) {
        if (def.id != nullptr && std::string {def.id} == id) {
            return &def;
        }
    }
    return nullptr;
}

TEST(ParamDefaults, AllIdsAreUnique) {
    std::set<std::string> ids;
    for (const auto& def : opcoda::params::kDefaults) {
        ASSERT_NE(def.id, nullptr);
        EXPECT_TRUE(ids.insert(def.id).second) << "ID repetido: " << def.id;
    }
    EXPECT_EQ(ids.size(), opcoda::params::kDefaults.size());
}

TEST(ParamDefaults, TableHasEveryParameter) {
    // 25 da Tabela 8, tres da F018, tres da F019, tres da F020, tres da F021,
    // quatro de filtro/curvas. Total: 41 IDs unicos; o editor acrescenta seis
    // macros sobre IDs que ja existem.
    EXPECT_EQ(opcoda::params::kDefaults.size(), 41u);
}

TEST(ParamDefaults, EveryDefaultMatchesTheOracle) {
    // Cobre o conjunto exacto: cada ID do oraculo tem de existir, e como a
    // tabela tem 41 entradas unicas, tambem nao sobra nenhuma.
    for (const auto& expected : kExpected) {
        const auto* found = findById(expected.id);
        ASSERT_NE(found, nullptr) << "ID em falta na tabela: " << expected.id;
        EXPECT_FLOAT_EQ(found->value, expected.value) << expected.id;
    }
}

TEST(ParamDefaults, LfoDivDefaultsToQuarterNote) {
    // O bug original: o layout criava indice 2 ("1/4") e o Knob::Spec dizia
    // 0.0 ("1/1"), por isso o duplo-clique repunha a divisao errada.
    EXPECT_STREQ(opcoda::params::lfodiv.id, "lfodiv");
    EXPECT_FLOAT_EQ(opcoda::params::lfodiv.value, 2.0f);
    EXPECT_EQ(opcoda::params::lfodiv.index(), 2);
}

TEST(ParamDefaults, IndexRoundsInsteadOfTruncating) {
    // O +0.5 existe para 1.9999 nao virar 1 por causa do float.
    constexpr opcoda::params::Default fractional {"frac", 1.9999f};
    EXPECT_EQ(fractional.index(), 2);
    EXPECT_EQ(opcoda::params::lfodiv.index(), 2);
    EXPECT_EQ(opcoda::params::lfosteps.index(), 16);
}

TEST(ParamDefaults, EveryDefaultIsFinite) {
    for (const auto& def : opcoda::params::kDefaults) {
        ASSERT_NE(def.id, nullptr);
        EXPECT_TRUE(std::isfinite(def.value)) << def.id;
    }
}

} // namespace
