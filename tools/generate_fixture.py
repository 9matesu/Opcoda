#!/usr/bin/env python3
# -*- coding: utf-8 -*-

"""Gera o binario de teste da otimizacao: um PE minimo com uma seccao `.text` de
8 KiB, dividida em quatro quadrantes com distribuicoes de byte distintas:

  1. zeros puros      (0.000-0.025): histograma concentrado num simbolo -> entropia 0
  2. gradiente 0-63   (0.025-0.050): distribuicao uniforme larga
  3. pseudo-aleatorio (0.050-0.075): LCG de semente fixa
  4. texto ASCII      (0.075-1.000): letras maiusculas e pontuacao

O binario e' intencionalmente nao executavel como programa; e' material
binario com cabecalho PE para ser lido pelo Opcoda. A semente e' 20260101 e o
numero de blocos e' 16 de 512 bytes. Reproduzir o experimento: arrastar o
ficheiro para o Opcoda (vista FIXA, 16 blocos) ou rodar
`opcoda_experiment` com --fixture.

Uso: tools/generate_fixture.py
Saída: specs/025-pesquisa-e-otimizacao/fixtures/material-fixo.bin
"""

import struct
import sys

SEED = 20260101
SECTION_BYTES = 8 * 1024          # 0x2000
HEADER_SIZE = 0x200
TOTAL_SIZE = HEADER_SIZE + SECTION_BYTES
ENTRY_OFFSET = 0x30               # 0x60 da spec do PeBuilder: 96 bytes de opcional
BASE_ADDRESS = 0x1000

# --- construtores PE minimos, espelhando o que o parser lê -----------------

def u16(v):
    return struct.pack('<H', v & 0xFFFF)

def u32(v):
    return struct.pack('<I', v & 0xFFFFFFFF)

def section_entry(name, raw_offset, raw_size):
    name_bytes = name.encode('ascii')[:7].ljust(8, b'\x00')
    return (name_bytes
            + u32(raw_size)      # RawPointerToRawData (16)
            + u32(raw_offset)    # PointerToRawData (20)
            + u32(0) + u32(0) + u32(0) + u32(0)   # alignements ignorados pelo parser
            + u32(0) + u32(0))   # mais campos ignorados

def build(content):
    data = bytearray(TOTAL_SIZE)

    # MZ
    data[0:2] = b'MZ'
    # e_lfanew no offset 0x3C
    struct.pack_into('<I', data, 0x3C, 0x80)
    # PE\0\0
    data[0x80:0x84] = b'PE\x00\x00'
    # COFF header: Machine=0x8664, Sections=1, TimeDateStamp,
    # PointerToSymbolTable=0, NumberOfSymbols=0, SizeOfOptionalHeader=96, Characteristics=0x22
    data[0x84:0x86] = u16(0x8664)
    data[0x86:0x88] = u16(1)
    struct.pack_into('<I', data, 0x88, 0xDEADBEEF)
    struct.pack_into('<I', data, 0x8C, 0)
    struct.pack_into('<I', data, 0x90, 0)
    struct.pack_into('<H', data, 0x94, 96)
    struct.pack_into('<H', data, 0x96, 0x22)

    # Optional header (dos campos que o parser lê): Magic=0x20b (64-bit PE),
    # SizeOfImage, SizeOfHeaders
    struct.pack_into('<H', data, ENTRY_OFFSET, 0x20B)
    struct.pack_into('<I', data, ENTRY_OFFSET + 56, TOTAL_SIZE)   # SizeOfImage
    struct.pack_into('<I', data, ENTRY_OFFSET + 60, HEADER_SIZE)  # SizeOfHeaders

    # Tabela de seções
    sec_off = ENTRY_OFFSET + 96
    data[sec_off:sec_off + 40] = section_entry('.text\x00\x00\x00', HEADER_SIZE, SECTION_BYTES)

    return bytes(data)

# --- conteudo dos quatro quadrantes ---------------------------------------

def quadrant_zeros(size):
    return bytes(size)

def quadrant_gradient(size):
    return bytes(i % 64 for i in range(size))

def quadrant_lcg(size):
    out = bytearray(size)
    x = SEED & 0xFFFFFFFF
    for i in range(size):
        x = (x * 1103515245 + 12345) & 0x7FFFFFFF
        out[i] = (x >> 16) & 0xFF
    return bytes(out)

def quadrant_ascii(size):
    chars = list(range(0x21, 0x30)) + list(range(0x41, 0x5A))  # pontuacao e A-Z
    return bytes(chars[i % len(chars)] for i in range(size))

def main():
    section = quadrant_zeros(SECTION_BYTES // 4)
    section += quadrant_gradient(SECTION_BYTES // 4)
    section += quadrant_lcg(SECTION_BYTES // 4)
    section += quadrant_ascii(SECTION_BYTES - 3 * SECTION_BYTES // 4)
    assert len(section) == SECTION_BYTES, len(section)
    data = build(section)
    assert len(data) == TOTAL_SIZE, len(data)

    out_path = 'specs/025-pesquisa-e-otimizacao/fixtures/material-fixo.bin'
    with open(out_path, 'wb') as f:
        f.write(data)
    print(f'escrito {out_path}  ({len(data)} bytes, 16 blocos de 512 bytes)')

if __name__ == '__main__':
    main()
