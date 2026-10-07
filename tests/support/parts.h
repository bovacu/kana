// Custom parts as a person builds them on the canvas and saves them (their part text): latches of gates, flip-flops of
// latches, registers, a memory, counters, a clock's counters, adders, an ALU, an LFSR, universal gates. Shared by the
// simulation's tests (tests/sim) and the canvas's (tests/zoom).
#ifndef TESTS_SUPPORT_PARTS_H
#define TESTS_SUPPORT_PARTS_H


static const c8* const PART_SRNAND =
    "fude-part 1\nid user/srnand\nname SR latch of NANDs\n"
    "port ~S logic in\nport ~R logic in\nport Q logic out 1 right\nport ~Q logic out 1 right\n"
    "inst nand G1 \"\" ~S ~Q Q\ninst nand G2 \"\" ~R Q ~Q\nend\n";
static const c8* const PART_DLATCH =
    "fude-part 1\nid user/dlatch\nname D latch\n"
    "port D logic in\nport EN logic in\nport Q logic out 1 right\nport ~Q logic out 1 right\n"
    "inst not N \"\" D nd\ninst nand G1 \"\" D EN s\ninst nand G2 \"\" nd EN r\ninst user/srnand L \"\" s r Q ~Q\nend\n";
static const c8* const PART_DFF =
    "fude-part 1\nid user/dff\nname D flip-flop of latches\n"
    "port D logic in\nport CLK logic in\nport Q logic out 1 right\nport ~Q logic out 1 right\n"
    "inst not N \"\" CLK nclk\ninst user/dlatch M \"\" D nclk m -\ninst user/dlatch S \"\" m CLK Q ~Q\nend\n";
static const c8* const PART_MUX2 =
    "fude-part 1\nid user/mux2\nname 2-to-1 multiplexer\n"
    "port A logic in\nport B logic in\nport S logic in\nport Y logic out 1 right\n"
    "inst not N \"\" S ns\ninst and G1 \"\" A ns a\ninst and G2 \"\" B S b\ninst or G3 \"\" a b Y\nend\n";
static const c8* const PART_REG4 =
    "fude-part 1\nid user/reg4\nname 4-bit register\n"
    "port D0 logic in\nport D1 logic in\nport D2 logic in\nport D3 logic in\nport LD logic in\nport CLK logic in\n"
    "port Q0 logic out 1 right\nport Q1 logic out 1 right\nport Q2 logic out 1 right\nport Q3 logic out 1 right\n"
    "inst user/mux2 M0 \"\" Q0 D0 LD d0\ninst user/dff F0 \"\" d0 CLK Q0 -\n"
    "inst user/mux2 M1 \"\" Q1 D1 LD d1\ninst user/dff F1 \"\" d1 CLK Q1 -\n"
    "inst user/mux2 M2 \"\" Q2 D2 LD d2\ninst user/dff F2 \"\" d2 CLK Q2 -\n"
    "inst user/mux2 M3 \"\" Q3 D3 LD d3\ninst user/dff F3 \"\" d3 CLK Q3 -\nend\n";
static const c8* const PART_DEC2 =
    "fude-part 1\nid user/dec2\nname 2-to-4 decoder\n"
    "port A0 logic in\nport A1 logic in\nport EN logic in\n"
    "port Y0 logic out 1 right\nport Y1 logic out 1 right\nport Y2 logic out 1 right\nport Y3 logic out 1 right\n"
    "inst not N0 \"\" A0 n0\ninst not N1 \"\" A1 n1\n"
    "inst and G0 \"inputs=3\" EN n0 n1 Y0\ninst and G1 \"inputs=3\" EN A0 n1 Y1\n"
    "inst and G2 \"inputs=3\" EN n0 A1 Y2\ninst and G3 \"inputs=3\" EN A0 A1 Y3\nend\n";
static const c8* const PART_MUX4 =
    "fude-part 1\nid user/mux4\nname 4-to-1 multiplexer\n"
    "port D0 logic in\nport D1 logic in\nport D2 logic in\nport D3 logic in\nport S0 logic in\nport S1 logic in\nport Y logic out 1 right\n"
    "inst not U1 \"\" S0 nS0\ninst not U2 \"\" S1 nS1\n"
    "inst and U3 \"inputs=3\" D0 nS0 nS1 t0\ninst and U4 \"inputs=3\" D1 S0 nS1 t1\n"
    "inst and U5 \"inputs=3\" D2 nS0 S1 t2\ninst and U6 \"inputs=3\" D3 S0 S1 t3\n"
    "inst or U7 \"inputs=4\" t0 t1 t2 t3 Y\nend\n";
static const c8* const PART_MUX4X4 =
    "fude-part 1\nid user/mux4x4\nname 4 words to 1\n"
    "port A0 logic in\nport A1 logic in\nport A2 logic in\nport A3 logic in\nport B0 logic in\nport B1 logic in\nport B2 logic in\nport B3 logic in\n"
    "port C0 logic in\nport C1 logic in\nport C2 logic in\nport C3 logic in\nport D0 logic in\nport D1 logic in\nport D2 logic in\nport D3 logic in\n"
    "port S0 logic in\nport S1 logic in\n"
    "port Y0 logic out 1 right\nport Y1 logic out 1 right\nport Y2 logic out 1 right\nport Y3 logic out 1 right\n"
    "inst user/mux4 M0 \"\" A0 B0 C0 D0 S0 S1 Y0\ninst user/mux4 M1 \"\" A1 B1 C1 D1 S0 S1 Y1\n"
    "inst user/mux4 M2 \"\" A2 B2 C2 D2 S0 S1 Y2\ninst user/mux4 M3 \"\" A3 B3 C3 D3 S0 S1 Y3\nend\n";
static const c8* const PART_RAM4X4 =
    "fude-part 1\nid user/ram4x4\nname 4 x 4 memory of registers\n"
    "port A0 logic in\nport A1 logic in\nport D0 logic in\nport D1 logic in\nport D2 logic in\nport D3 logic in\nport WE logic in\nport CLK logic in\n"
    "port Q0 logic out 1 right\nport Q1 logic out 1 right\nport Q2 logic out 1 right\nport Q3 logic out 1 right\n"
    "inst user/dec2 DEC \"\" A0 A1 WE w0 w1 w2 w3\n"
    "inst user/reg4 R0 \"\" D0 D1 D2 D3 w0 CLK a0 a1 a2 a3\ninst user/reg4 R1 \"\" D0 D1 D2 D3 w1 CLK b0 b1 b2 b3\n"
    "inst user/reg4 R2 \"\" D0 D1 D2 D3 w2 CLK c0 c1 c2 c3\ninst user/reg4 R3 \"\" D0 D1 D2 D3 w3 CLK d0 d1 d2 d3\n"
    "inst user/mux4x4 MX \"\" a0 a1 a2 a3 b0 b1 b2 b3 c0 c1 c2 c3 d0 d1 d2 d3 A0 A1 Q0 Q1 Q2 Q3\nend\n";
static const c8* const PART_HA =
    "fude-part 1\nid user/ha\nname Half adder\nport A logic in\nport B logic in\nport S logic out 1 right\nport C logic out 1 right\n"
    "inst xor X \"\" A B S\ninst and G \"\" A B C\nend\n";
static const c8* const PART_INC4 =
    "fude-part 1\nid user/inc4\nname 4-bit incrementer\n"
    "port A0 logic in\nport A1 logic in\nport A2 logic in\nport A3 logic in\nport CI logic in\n"
    "port S0 logic out 1 right\nport S1 logic out 1 right\nport S2 logic out 1 right\nport S3 logic out 1 right\nport CO logic out 1 right\n"
    "inst user/ha H0 \"\" A0 CI S0 c1\ninst user/ha H1 \"\" A1 c1 S1 c2\ninst user/ha H2 \"\" A2 c2 S2 c3\ninst user/ha H3 \"\" A3 c3 S3 CO\nend\n";
static const c8* const PART_COUNTER4 =
    "fude-part 1\nid user/counter4\nname 4-bit counter\ndesc Counts on each clock while EN is high; RST high clears it on the clock; TC high at 15 while EN is.\n"
    "port CLK logic in\nport RST logic in\nport EN logic in\n"
    "port Q0 logic out 1 right\nport Q1 logic out 1 right\nport Q2 logic out 1 right\nport Q3 logic out 1 right\nport TC logic out 1 right\n"
    "inst user/inc4 INC \"\" Q0 Q1 Q2 Q3 EN n0 n1 n2 n3 -\ninst not NR \"\" RST nrst\n"
    "inst and K0 \"\" n0 nrst d0\ninst and K1 \"\" n1 nrst d1\ninst and K2 \"\" n2 nrst d2\ninst and K3 \"\" n3 nrst d3\n"
    "inst const ONE \"1\" one\ninst user/reg4 R \"\" d0 d1 d2 d3 one CLK Q0 Q1 Q2 Q3\n"
    "inst and T \"inputs=5\" Q0 Q1 Q2 Q3 EN TC\nend\n";
static const c8* const PART_BCD =
    "fude-part 1\nid user/bcd\nname Decade counter\n"
    "port CLK logic in\nport RST logic in\nport EN logic in\n"
    "port Q0 logic out 1 right\nport Q1 logic out 1 right\nport Q2 logic out 1 right\nport Q3 logic out 1 right\nport TC logic out 1 right\n"
    "inst and NINE \"inputs=3\" Q0 Q3 EN TC\ninst or W \"\" RST TC wrap\ninst user/counter4 C \"\" CLK wrap EN Q0 Q1 Q2 Q3 -\nend\n";
static const c8* const PART_MOD6 =
    "fude-part 1\nid user/mod6\nname Counter to 6\n"
    "port CLK logic in\nport RST logic in\nport EN logic in\n"
    "port Q0 logic out 1 right\nport Q1 logic out 1 right\nport Q2 logic out 1 right\nport Q3 logic out 1 right\nport TC logic out 1 right\n"
    "inst and FIVE \"inputs=3\" Q0 Q2 EN TC\ninst or W \"\" RST TC wrap\ninst user/counter4 C \"\" CLK wrap EN Q0 Q1 Q2 Q3 -\nend\n";
static const c8* const PART_CLOCK60 =
    "fude-part 1\nid user/clock60\nname 00 to 59\n"
    "port CLK logic in\nport RST logic in\nport EN logic in\n"
    "port U0 logic out 1 right\nport U1 logic out 1 right\nport U2 logic out 1 right\nport U3 logic out 1 right\n"
    "port T0 logic out 1 right\nport T1 logic out 1 right\nport T2 logic out 1 right\nport T3 logic out 1 right\nport TC logic out 1 right\n"
    "inst user/bcd U \"\" CLK RST EN U0 U1 U2 U3 tu\ninst user/mod6 T \"\" CLK RST tu T0 T1 T2 T3 TC\nend\n";
static const c8* const PART_FA =
    "fude-part 1\nid user/fa\nname Full adder\nport A logic in\nport B logic in\nport CI logic in\nport S logic out 1 right\nport CO logic out 1 right\n"
    "inst xor X1 \"\" A B p\ninst xor X2 \"\" p CI S\ninst and A1 \"\" A B g\ninst and A2 \"\" p CI q\ninst or O1 \"\" g q CO\nend\n";
static const c8* const PART_ADD4 =
    "fude-part 1\nid user/add4\nname 4-bit adder\n"
    "port A0 logic in\nport A1 logic in\nport A2 logic in\nport A3 logic in\nport B0 logic in\nport B1 logic in\nport B2 logic in\nport B3 logic in\nport CI logic in\n"
    "port S0 logic out 1 right\nport S1 logic out 1 right\nport S2 logic out 1 right\nport S3 logic out 1 right\nport CO logic out 1 right\n"
    "inst user/fa F0 \"\" A0 B0 CI S0 c1\ninst user/fa F1 \"\" A1 B1 c1 S1 c2\ninst user/fa F2 \"\" A2 B2 c2 S2 c3\ninst user/fa F3 \"\" A3 B3 c3 S3 CO\nend\n";
// An ALU: AND, OR, XOR or ADD as OP says (0 1 2 3); its carry (ADD's only) and whether it is zero.
static const c8* const PART_ALU4 =
    "fude-part 1\nid user/alu4\nname 4-bit ALU\n"
    "port A0 logic in\nport A1 logic in\nport A2 logic in\nport A3 logic in\nport B0 logic in\nport B1 logic in\nport B2 logic in\nport B3 logic in\n"
    "port OP0 logic in\nport OP1 logic in\n"
    "port Y0 logic out 1 right\nport Y1 logic out 1 right\nport Y2 logic out 1 right\nport Y3 logic out 1 right\nport CO logic out 1 right\nport Z logic out 1 right\n"
    "inst const ZERO \"0\" zero\ninst user/add4 ADD \"\" A0 A1 A2 A3 B0 B1 B2 B3 zero s0 s1 s2 s3 co\n"
    "inst and N0 \"\" A0 B0 n0\ninst or O0 \"\" A0 B0 o0\ninst xor X0 \"\" A0 B0 x0\ninst user/mux4 M0 \"\" n0 o0 x0 s0 OP0 OP1 Y0\n"
    "inst and N1 \"\" A1 B1 n1\ninst or O1 \"\" A1 B1 o1\ninst xor X1 \"\" A1 B1 x1\ninst user/mux4 M1 \"\" n1 o1 x1 s1 OP0 OP1 Y1\n"
    "inst and N2 \"\" A2 B2 n2\ninst or O2 \"\" A2 B2 o2\ninst xor X2 \"\" A2 B2 x2\ninst user/mux4 M2 \"\" n2 o2 x2 s2 OP0 OP1 Y2\n"
    "inst and N3 \"\" A3 B3 n3\ninst or O3 \"\" A3 B3 o3\ninst xor X3 \"\" A3 B3 x3\ninst user/mux4 M3 \"\" n3 o3 x3 s3 OP0 OP1 Y3\n"
    "inst and C \"inputs=3\" OP0 OP1 co CO\ninst nor ZF \"inputs=4\" Y0 Y1 Y2 Y3 Z\nend\n";
// A linear-feedback shift register (x^4 + x^3 + 1: every number but 0 in turn), its seed loaded while LD is high.
static const c8* const PART_LFSR4 =
    "fude-part 1\nid user/lfsr4\nname 4-bit LFSR\n"
    "port CLK logic in\nport LD logic in\nport S0 logic in\nport S1 logic in\nport S2 logic in\nport S3 logic in\n"
    "port Q0 logic out 1 right\nport Q1 logic out 1 right\nport Q2 logic out 1 right\nport Q3 logic out 1 right\n"
    "inst xnor F \"\" Q3 Q2 nf\ninst not NF \"\" nf f\n"
    "inst user/mux2 M0 \"\" f S0 LD d0\ninst user/mux2 M1 \"\" Q0 S1 LD d1\ninst user/mux2 M2 \"\" Q1 S2 LD d2\ninst user/mux2 M3 \"\" Q2 S3 LD d3\n"
    "inst const ONE \"1\" one\ninst user/reg4 R \"\" d0 d1 d2 d3 one CLK Q0 Q1 Q2 Q3\nend\n";
// Universal gates: XOR of four NANDs, OR of NANDs, AND of NORs, NOT of a NAND, a full adder of nine NANDs.
static const c8* const PART_NXOR =
    "fude-part 1\nid user/nandxor\nname XOR of NANDs\nport A logic in\nport B logic in\nport Y logic out 1 right\n"
    "inst nand G1 \"\" A B m\ninst nand G2 \"\" A m p\ninst nand G3 \"\" B m q\ninst nand G4 \"\" p q Y\nend\n";
static const c8* const PART_NOR_AND =
    "fude-part 1\nid user/norand\nname AND of NORs\nport A logic in\nport B logic in\nport Y logic out 1 right\n"
    "inst nor G1 \"\" A A na\ninst nor G2 \"\" B B nb\ninst nor G3 \"\" na nb Y\nend\n";
static const c8* const PART_NAND_OR =
    "fude-part 1\nid user/nandor\nname OR of NANDs\nport A logic in\nport B logic in\nport Y logic out 1 right\n"
    "inst nand G1 \"\" A A na\ninst nand G2 \"\" B B nb\ninst nand G3 \"\" na nb Y\nend\n";
static const c8* const PART_NAND_FA =
    "fude-part 1\nid user/nandfa\nname Full adder of NANDs\nport A logic in\nport B logic in\nport CI logic in\nport S logic out 1 right\nport CO logic out 1 right\n"
    "inst nand G1 \"\" A B m\ninst nand G2 \"\" A m p\ninst nand G3 \"\" B m q\ninst nand G4 \"\" p q x\n"
    "inst nand G5 \"\" x CI n\ninst nand G6 \"\" x n r\ninst nand G7 \"\" CI n s\ninst nand G8 \"\" r s S\ninst nand G9 \"\" n m CO\nend\n";

static const c8* const PARTS_ALL[] = { PART_SRNAND, PART_DLATCH, PART_DFF, PART_MUX2, PART_REG4, PART_DEC2, PART_MUX4, PART_MUX4X4, PART_RAM4X4, PART_HA,
                                       PART_INC4, PART_COUNTER4, PART_BCD, PART_MOD6, PART_CLOCK60, PART_FA, PART_ADD4, PART_ALU4, PART_LFSR4,
                                       PART_NXOR, PART_NOR_AND, PART_NAND_OR, PART_NAND_FA };
#define PARTS_N ((u32)(sizeof(PARTS_ALL) / sizeof(PARTS_ALL[0])))

#endif
