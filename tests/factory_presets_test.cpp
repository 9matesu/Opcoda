// os presets de fabrica sao valores crus na unidade do parametro. um digito
// trocado atravessa convertTo0to1 e sai recortado em silencio (o "Cloud" com
// attack 40 em vez de 0,4 soava errado sem erro nenhum).
// este ensaio prende a tabela aos intervalos do createParameterLayout: se um lado
// mudar, o outro acusa.

#include "opcoda_plugin/source/factory_presets.h"

#include <gtest/gtest.h>

#include <set>
#include <string>

namespace {

using opcoda::factoryPresets;
using opcoda::FactoryPreset;

struct Range {
    float lo;
    float hi;
};

// intervalos de PluginProcessor::createParameterLayout, na mesma ordem.
void checkRanges(const FactoryPreset& p) {
    const auto in = [&p](float v, float lo, float hi, const char* id) {
        EXPECT_GE(v, lo) << id << " em " << p.name;
        EXPECT_LE(v, hi) << id << " em " << p.name;
    };
    in(p.grain, 1.0f, 100.0f, "grain");
    in(p.density, 1.0f, 200.0f, "density");
    in(p.position, 0.0f, 1.0f, "position");
    in(p.spray, 0.0f, 1.0f, "spray");
    in(p.pitch, -48.0f, 48.0f, "pitch");
    in(p.volume, -60.0f, 0.0f, "volume");
    in(p.window, 0.0f, 3.0f, "window");
    in(p.pan, -1.0f, 1.0f, "pan");
    in(p.grainlevel, 0.0f, 1.0f, "grainlevel");
    in(p.pitchrand, 0.0f, 12.0f, "pitchrand");
    in(p.scanspeed, 0.25f, 4.0f, "scanspeed");
    in(p.attack, 0.001f, 2.0f, "attack");
    in(p.decay, 0.001f, 2.0f, "decay");
    in(p.sustain, 0.0f, 1.0f, "sustain");
    in(p.release, 0.001f, 2.0f, "release");
    in(p.f1type, 0.0f, 3.0f, "f1type");
    in(p.f1cutoff, 20.0f, 20000.0f, "f1cutoff");
    in(p.f1q, 0.5f, 12.0f, "f1q");
    in(p.f2type, 0.0f, 3.0f, "f2type");
    in(p.f2cutoff, 20.0f, 20000.0f, "f2cutoff");
    in(p.f2q, 0.5f, 12.0f, "f2q");
    in(p.lforate, 0.0f, 20.0f, "lforate");
    in(p.lfodepth, 0.0f, 1.0f, "lfodepth");
    in(p.lfotarget, 0.0f, 3.0f, "lfotarget");
    in(p.lfowave, 0.0f, 4.0f, "lfowave");
    in(p.cloudsize, 0.0f, 1.0f, "cloudsize");
    in(p.pitchspread, 0.0f, 24.0f, "pitchspread");
    in(p.grainjit, 0.0f, 1.0f, "grainjit");
    in(p.width, 0.0f, 2.0f, "width");
    in(p.glide, 0.0f, 2.0f, "glide");
    in(p.f1slope, 0.0f, 1.0f, "f1slope");
    in(p.f1drive, 0.0f, 24.0f, "f1drive");
    in(p.f1keytrack, 0.0f, 1.0f, "f1keytrack");
    in(p.f1env, -1.0f, 1.0f, "f1env");
    in(p.atkcurve, -1.0f, 1.0f, "atkcurve");
    in(p.deccurve, -1.0f, 1.0f, "deccurve");
    in(p.relcurve, -1.0f, 1.0f, "relcurve");
}

TEST(FactoryPresets, AllValuesWithinLayoutRanges) {
    const auto& presets = factoryPresets();
    ASSERT_EQ(presets.size(), 10u);
    for (const auto& preset : presets) {
        checkRanges(preset);
    }
}

TEST(FactoryPresets, NamesAreUniqueAndNonEmpty) {
    // o menu mostra o nome e o editor usa-o como identidade: dois "Cloud" davam
    // um tick ambiguo e um nome vazio um botao sem texto.
    std::set<std::string> seen;
    for (const auto& preset : factoryPresets()) {
        EXPECT_NE(preset.name, nullptr);
        EXPECT_GT(std::string {preset.name}.size(), 0u);
        EXPECT_TRUE(seen.insert(preset.name).second) << preset.name;
    }
}

TEST(FactoryPresets, InitIsAllDefaults) {
    // o primeiro preset e' o zero auditavel: cada campo tem de ser o default do
    // layout, para "voltar ao Init" ter significado.
    const auto& init = factoryPresets()[0];
    EXPECT_STREQ(init.name, "Init");
    EXPECT_FLOAT_EQ(init.grain, 40.0f);
    EXPECT_FLOAT_EQ(init.density, 20.0f);
    EXPECT_FLOAT_EQ(init.position, 0.5f);
    EXPECT_FLOAT_EQ(init.spray, 0.0f);
    EXPECT_FLOAT_EQ(init.pitch, 0.0f);
    EXPECT_FLOAT_EQ(init.volume, 0.0f);
    EXPECT_FLOAT_EQ(init.window, 0.0f);
    EXPECT_FLOAT_EQ(init.pan, 0.0f);
    EXPECT_FLOAT_EQ(init.grainlevel, 1.0f);
    EXPECT_FLOAT_EQ(init.pitchrand, 0.0f);
    EXPECT_FLOAT_EQ(init.scanspeed, 1.0f);
    EXPECT_FLOAT_EQ(init.attack, 0.005f);
    EXPECT_FLOAT_EQ(init.decay, 0.1f);
    EXPECT_FLOAT_EQ(init.sustain, 1.0f);
    EXPECT_FLOAT_EQ(init.release, 0.05f);
    EXPECT_FLOAT_EQ(init.f1type, 0.0f);
    EXPECT_FLOAT_EQ(init.f1cutoff, 20000.0f);
    EXPECT_FLOAT_EQ(init.f1q, 0.7071f);
    EXPECT_FLOAT_EQ(init.f2type, 0.0f);
    EXPECT_FLOAT_EQ(init.f2cutoff, 20000.0f);
    EXPECT_FLOAT_EQ(init.f2q, 0.7071f);
    EXPECT_FLOAT_EQ(init.lforate, 1.0f);
    EXPECT_FLOAT_EQ(init.lfodepth, 0.0f);
    EXPECT_FLOAT_EQ(init.lfotarget, 0.0f);
    EXPECT_FLOAT_EQ(init.lfowave, 0.0f);
    EXPECT_FLOAT_EQ(init.cloudsize, 0.0f);
    EXPECT_FLOAT_EQ(init.pitchspread, 0.0f);
    EXPECT_FLOAT_EQ(init.grainjit, 0.0f);
    EXPECT_FLOAT_EQ(init.width, 1.0f);
    EXPECT_FLOAT_EQ(init.glide, 0.0f);
    EXPECT_FLOAT_EQ(init.f1slope, 0.0f);
    EXPECT_FLOAT_EQ(init.f1drive, 0.0f);
    EXPECT_FLOAT_EQ(init.f1keytrack, 0.0f);
    EXPECT_FLOAT_EQ(init.f1env, 0.0f);
    EXPECT_FLOAT_EQ(init.atkcurve, 0.0f);
    EXPECT_FLOAT_EQ(init.deccurve, 0.0f);
    EXPECT_FLOAT_EQ(init.relcurve, 0.0f);
}

} // namespace
