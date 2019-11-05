//
// Created by 甘尧 on 2019-09-24.
//

// Branch
Type(Branches)
INST(B_cond,  "B", (cond_branch, label_cond_branch), "01010100xxxxxxxxxxxxxxxxxxx0xxxx")
INST(BR,      "BR", (Xn),                            "1101011000011111000000xxxxx00000")
INST(BLR,     "BLR", (Xn),                           "1101011000111111000000xxxxx00000")
INST(RET,     "RET", (Xn),                           "1101011001011111000000xxxxx00000")
INST(ERET,    "ERET", (Xn),                          "11010110100111110000001111100000")
INST(DRPS,    "DRPS", (Xn),                          "11010110101111110000001111100000")
INST(B,       "B",    (Xn),                          "000101xxxxxxxxxxxxxxxxxxxxxxxxxx")
INST(BL,      "BL",   (Xn),                          "100101xxxxxxxxxxxxxxxxxxxxxxxxxx")
INST(CBZ_32,  "CBZ", (Wt, label_cond_branch),        "00110100xxxxxxxxxxxxxxxxxxxxxxxx")
INST(CBNZ_32, "CBNZ", (Wt, label_cond_branch),       "00110101xxxxxxxxxxxxxxxxxxxxxxxx")
INST(CBZ_64,  "CBZ", (Xt, label_cond_branch),        "10110100xxxxxxxxxxxxxxxxxxxxxxxx")
INST(CBNZ_64, "CBNZ", (Xt, label_cond_branch),       "10110101xxxxxxxxxxxxxxxxxxxxxxxx")
INST(TBZ,     "TBZ", (Xt, imm_tbz, label_tbz),       "00110110xxxxxxxxxxxxxxxxxxxxxxxx")
INST(TBNZ,    "TBNZ", (Xt, imm_tbz, label_tbz),      "00110111xxxxxxxxxxxxxxxxxxxxxxxx")


// Exception Generating
Type(ExpGen)
INST(SVC, "SVC", (imm_exceptions),                   "110101000000xxxxxxxxxxxxxxx00001")
INST(HVC, "HVC", (imm_exceptions),                   "110101000000xxxxxxxxxxxxxxx00010")
INST(SMC, "SMC", (imm_exceptions),                   "110101000000xxxxxxxxxxxxxxx00011")
INST(BRK, "BRK", (imm_exceptions),                   "110101000010xxxxxxxxxxxxxxx00000")
INST(HLT, "HLT", (imm_exceptions),                   "110101000100xxxxxxxxxxxxxxx00000")
INST(DCPS1, "DCPS1", (imm_exceptions),               "110101001010xxxxxxxxxxxxxxx00001")
INST(DCPS2, "DCPS2", (imm_exceptions),               "110101001010xxxxxxxxxxxxxxx00010")
INST(DCPS3, "DCPS3", (imm_exceptions),               "110101001010xxxxxxxxxxxxxxx00011")


//System
Type(System)
INST(MRS, "MRS", (TODO),                             "110101010011xxxxxxxxxxxxxxxxxxxx")
INST(MSR_imm, "MSR", (TODO),                         "1101010100000xxx0100xxxxxxx11111")
INST(MSR_reg, "MSR", (TODO),                         "110101010001xxxxxxxxxxxxxxxxxxxx")
INST(NOP, "NOP", (),                                 "11010101000000110010000000011111")
INST(YIELD, "YIELD", (),                             "11010101000000110010000000111111")


// Data Proccessing Immediate
// PC-rel. addressing
Type(PCRelAdr)
INST(ADR, "ADR", (Xd, imm_adr),                      "0xx10000xxxxxxxxxxxxxxxxxxxxxxxx")
INST(ADRP, "ADRP", (Xd, imm_adrp),                   "1xx10000xxxxxxxxxxxxxxxxxxxxxxxx")
// Add/subtract (immediate)
Type(AddSubImmediate)
INST(ADD_imm, "ADD", (Wd_WSP, Wn_WSP, imm_addsub),   "x00100010xxxxxxxxxxxxxxxxxxxxxxx")
INST(ADDS_imm, "ADDS", (Wd, Wn_WSP, imm_addsub),     "x01100010xxxxxxxxxxxxxxxxxxxxxxx")
INST(SUB_imm, "SUB", (Wd_WSP, Wn_WSP, imm_addsub),   "x10100010xxxxxxxxxxxxxxxxxxxxxxx")
INST(SUBS_imm, "SUBS", (Wd, Wn_WSP, imm_addsub),     "x11100010xxxxxxxxxxxxxxxxxxxxxxx")
// Move wide (immediate)
Type(MovWide)
INST(MOVN, "MOVN", (Xd, imm_mov),                    "x00100101xxxxxxxxxxxxxxxxxxxxxxx")
INST(MOVZ, "MOVZ", (Xd, imm_mov),                    "x10100101xxxxxxxxxxxxxxxxxxxxxxx")
INST(MOVK, "MOVK", (Xd, imm_mov),                    "x11100101xxxxxxxxxxxxxxxxxxxxxxx")

// Logical (Immediate)
Type(LogicalImmediate)
INST(AND_imm, "AND",  (),                            "x00100100xxxxxxxxxxxxxxxxxxxxxxx")
INST(ORR_imm, "ORR",   (),                           "x01100100xxxxxxxxxxxxxxxxxxxxxxx")
INST(EOR_imm, "EOR",   (),                           "x10100100xxxxxxxxxxxxxxxxxxxxxxx")
INST(ANDS_imm, "ANDS",  (),                          "x11100100xxxxxxxxxxxxxxxxxxxxxxx")

// Immediate - Bitfield
INST(SBFM, "SBFM", (),                               "x00100110xxxxxxxxxxxxxxxxxxxxxxx")
INST(BFM,  "BFM",  (),                               "x01100110xxxxxxxxxxxxxxxxxxxxxxx")
INST(UBFM, "UBFM", (),                               "x10100110xxxxxxxxxxxxxxxxxxxxxxx")

// Immediate - Extract
INST(EXTR,  "EXTR",(),                               "x00100111x0xxxxxxxxxxxxxxxxxxxxx")

// Load/store register (immediate post-indexed) / (immediate post-indexed) / unsigned immediate
Type(StoreRegImm)
INST(STR, "STR", (Wt, loadstore_post_off),           "1x11100x000xxxxxxxxxxxxxxxxxxxxx")
INST(STRB, "STRB", (Wt, loadstore_post_off),         "0011100x000xxxxxxxxxxxxxxxxxxxxx")
INST(STR_float, "STR", (Bt, loadstore_post_off),     "xx11110x000xxxxxxxxxxxxxxxxxxxxx")
INST(STR128, "STR", (Qt, loadstore_post_off),        "0011110x100xxxxxxxxxxxxxxxxxxxxx")
INST(STRH, "STRH", (Wt, loadstore_post_off),         "0111100x000xxxxxxxxxxxxxxxxxxxxx")
Type(LoadRegImm)
INST(LDRB, "LDRB", (Wt, loadstore_post_off),         "0011100x010xxxxxxxxxxxxxxxxxxxxx")
INST(LDRSB, "LDRSB", (Xt, loadstore_post_off),       "0011100x1x0xxxxxxxxxx1xxxxxxxxxx")
INST(LDR_float, "LDR", (Bt, loadstore_post_off),     "xx11110x010xxxxxxxxxxxxxxxxxxxxx")
INST(LDR128, "LDR", (Qt, loadstore_post_off),        "0011110x110xxxxxxxxxxxxxxxxxxxxx")
INST(LDRH, "LDRH", (Wt, loadstore_post_off),         "0111100x010xxxxxxxxxxxxxxxxxxxxx")
INST(LDRSH, "LDRSH", (Xt, loadstore_post_off),       "0111100x1x0xxxxxxxxxxxxxxxxxxxxx")
INST(LDR, "LDR", (Wt, loadstore_post_off),           "1x11100x010xxxxxxxxxxxxxxxxxxxxx")
INST(LDRSW, "LDRSW", (Xt, loadstore_post_off),       "1011100x100xxxxxxxxxxxxxxxxxxxxx")


// Loads and stores - Load/Store Exclusive
Type(StoreRegImm)
INST(STXR,  "STXRB, STXRH, STXR",(),                 "xx001000000xxxxx011111xxxxxxxxxx")
INST(STLXR, "STLXRB, STLXRH, STLXR", (),             "xx001000000xxxxx111111xxxxxxxxxx")
INST(STXP,  "STXP", (),                              "1x001000001xxxxx0xxxxxxxxxxxxxxx")
INST(STLXP, "STLXP", (),                             "1x001000001xxxxx1xxxxxxxxxxxxxxx")
INST(STLLR, "STLLRB, STLLRH, STLLR", (),             "xx00100010011111011111xxxxxxxxxx")
INST(STLR,  "STLRB, STLRH, STLR", (),                "xx00100010011111111111xxxxxxxxxx")
Type(LoadRegImm)
INST(LDXR,  "LDXRB, LDXRH, LDXR", (),                "xx00100001011111011111xxxxxxxxxx")
INST(LDAXR, "LDAXRB, LDAXRH, LDAXR", (),             "xx00100001011111111111xxxxxxxxxx")
INST(LDXP,  "LDXP", (),                              "1x001000011111110xxxxxxxxxxxxxxx")
INST(LDAXP, "LDAXP",(),                              "1x001000011111111xxxxxxxxxxxxxxx")
INST(LDLAR, "LDLARB, LDLARH, LDLAR", (),             "xx00100011011111011111xxxxxxxxxx")
INST(LDAR,  "LDARB, LDARH, LDAR", (),                "xx00100011011111111111xxxxxxxxxx")

// Loads and stores - Load register (literal)
Type(LoadLiteral)
INST(LDR_lit_gen, "LDR (literal)", (),               "0x011000xxxxxxxxxxxxxxxxxxxxxxxx")
INST(LDRSW_lit,   "LDRSW (literal)", (),             "10011000xxxxxxxxxxxxxxxxxxxxxxxx")
INST(PRFM_lit,     "PRFM (literal)", (),             "11011000xxxxxxxxxxxxxxxxxxxxxxxx")
INST(LDR_lit_fpsimd, "LDR (literal, SIMD&FP)", (),   "xx011100xxxxxxxxxxxxxxxxxxxxxxxx")


// Loads and stores - Load/Store register (register offset)
INST(STRx_reg, "STRx (register)", (),                "xx111000x01xxxxxxxxx10xxxxxxxxxx")
INST(STR_reg_fpsimd, "STR (register, SIMD&FP)", (),  "xx111100x01xxxxxxxxx10xxxxxxxxxx")
INST(LDRx_reg, "LDRx (register)", (),                "xx111000x11xxxxxxxxx10xxxxxxxxxx")
INST(LDR_reg_fpsimd, "LDR (register, SIMD&FP)", (),  "xx111100x11xxxxxxxxx10xxxxxxxxxx")

// Loads and stores - Load/Store register pair
INST(STP_gen, "STP", (),                             "xx10100xx0xxxxxxxxxxxxxxxxxxxxxx")
INST(STP_fpsimd, "STP (SIMD&FP)", (),                "xx10110xx0xxxxxxxxxxxxxxxxxxxxxx")
INST(LDP_gen, "LDP", (),                             "xx10100xx1xxxxxxxxxxxxxxxxxxxxxx")
INST(LDP_fpsimd, "LDP (SIMD&FP)", (),                "xx10110xx1xxxxxxxxxxxxxxxxxxxxxx")

// Loads and stores - Load/Store register (unprivileged)
INST(STTRB, "STTRB", (),                             "00111000000xxxxxxxxx10xxxxxxxxxx")
INST(LDTRB, "LDTRB", (),                             "00111000010xxxxxxxxx10xxxxxxxxxx")
INST(LDTRSB, "LDTRSB", (),                           "00111000xx0xxxxxxxxx10xxxxxxxxxx")
INST(STTRH, "STTRH", (),                             "01111000000xxxxxxxxx10xxxxxxxxxx")
INST(LDTRH,  "LDTRH", (),                            "01111000010xxxxxxxxx10xxxxxxxxxx")
INST(LDTRSH, "LDTRSH", (),                           "01111000xx0xxxxxxxxx10xxxxxxxxxx")
INST(STTR, "STTR", (),                               "xx111000000xxxxxxxxx10xxxxxxxxxx")
INST(LDTR, "LDTR", (),                               "xx111000010xxxxxxxxx10xxxxxxxxxx")
INST(LDTRSW, "LDTRSW", (),                           "10111000100xxxxxxxxx10xxxxxxxxxx")

