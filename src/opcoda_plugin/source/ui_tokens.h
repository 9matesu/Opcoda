#pragma once

namespace opcoda::tokens {

// Escala de espacamento: grelha de 8 px. 4 e' o unico passo abaixo de 8, para o
// ajuste icone-rotulo. Tudo o resto sai daqui -- nao ha 6, 10, 12 nem 18 px
// espalhados pelo layout.
inline constexpr int space1 {4};   // micro: icone <-> rotulo
inline constexpr int space2 {8};   // interno de componente
inline constexpr int space3 {16};  // padding standard e separacao
inline constexpr int space4 {24};  // separacao entre seccoes
inline constexpr int space5 {32};  // espaco estrutural exterior

// Raios: dois valores, mais nada. Paineis 6, controlos 3.
inline constexpr int radiusPanel {6};
inline constexpr int radiusControl {3};

// Bordas: 1 px, sempre. O alpha fica aqui para todos os paineis usarem o mesmo.
inline constexpr float borderAlpha {0.09f};

inline constexpr int icon {24};
inline constexpr int iconSmall {18};
inline constexpr int controlHeight {22};

} // namespace opcoda::tokens
