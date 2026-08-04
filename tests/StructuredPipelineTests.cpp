#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

#include <AtariStudio/Core/Project.h>
#include <AtariStudio/Disassembler/BasicBlockAnalyzer.h>
#include <AtariStudio/Disassembler/Disassembler.h>
#include <AtariStudio/Disassembler/DisassemblyMetadata.h>
#include <AtariStudio/Disassembler/LoopStructureAnalyzer.h>
#include <AtariStudio/Disassembler/Listing.h>
#include <AtariStudio/Disassembler/SemanticConditionAnalyzer.h>
#include <AtariStudio/Disassembler/StructuredCodeGenerator.h>
#include <AtariStudio/Disassembler/StructuredTranslationUnitGenerator.h>
#include <AtariStudio/Disassembler/StructuredControlFlowAnalyzer.h>
#include <AtariStudio/Disassembler/StructuredExpressionBuilder.h>
#include <AtariStudio/Disassembler/StructuredStatementFormatter.h>
#include <AtariStudio/Formats/XEX/XexLoader.h>

namespace
{

bool Contains(
    const std::string& text,
    const std::string& fragment)
{
    return
        text.find(fragment) !=
        std::string::npos;
}

std::size_t CountOccurrences(
    const std::string& text,
    const std::string& fragment)
{
    if (fragment.empty())
    {
        return 0;
    }

    std::size_t count = 0;
    std::size_t position = 0;

    while ((position =
                text.find(
                    fragment,
                    position)) !=
           std::string::npos)
    {
        ++count;
        position += fragment.size();
    }

    return count;
}

bool Expect(
    bool condition,
    const char* message)
{
    if (condition)
    {
        return true;
    }

    std::cerr
        << "FAILED: "
        << message
        << '\n';

    return false;
}

} // namespace

int main()
{
    constexpr atari::u16 EntryAddress =
        0x1000;

    constexpr atari::u16 ProducerAddress =
        0x1007;

    constexpr atari::u16 BranchAddress =
        0x1008;

    constexpr atari::u16 ThenAddress =
        0x100A;

    atari::Project project;

    project.SetRunAddress(
        EntryAddress);

    auto& memory =
        project.GetMemory();

    // LDX #$00
    memory.Write8(0x1000, 0xA2);
    memory.Write8(0x1001, 0x00);

    // LDA #$42
    memory.Write8(0x1002, 0xA9);
    memory.Write8(0x1003, 0x42);

    // STA $2000
    memory.Write8(0x1004, 0x8D);
    memory.Write8(0x1005, 0x00);
    memory.Write8(0x1006, 0x20);

    // INX
    memory.Write8(ProducerAddress, 0xE8);

    // BNE $100A
    memory.Write8(BranchAddress, 0xD0);
    memory.Write8(0x1009, 0x00);

    // JSR $1234
    memory.Write8(ThenAddress, 0x20);
    memory.Write8(0x100B, 0x34);
    memory.Write8(0x100C, 0x12);

    // RTS
    memory.Write8(0x100D, 0x60);

    // Additional instructions used to verify the formatter
    // independently from basic-block placement.

    // LDA $1234,X
    memory.Write8(0x1100, 0xBD);
    memory.Write8(0x1101, 0x34);
    memory.Write8(0x1102, 0x12);

    // STA $40,X
    memory.Write8(0x1103, 0x95);
    memory.Write8(0x1104, 0x40);

    // ADC #$01 -- carry-sensitive, must use fallback.
    memory.Write8(0x1105, 0x69);
    memory.Write8(0x1106, 0x01);

    // LDA $3000
    memory.Write8(0x1200, 0xAD);
    memory.Write8(0x1201, 0x00);
    memory.Write8(0x1202, 0x30);

    // STA $3001
    memory.Write8(0x1203, 0x8D);
    memory.Write8(0x1204, 0x01);
    memory.Write8(0x1205, 0x30);

    // LDA #$02 -- kills the previous accumulator value.
    memory.Write8(0x1206, 0xA9);
    memory.Write8(0x1207, 0x02);

    // LDX #$03 / STX $40 / LDX #$04
    memory.Write8(0x1208, 0xA2);
    memory.Write8(0x1209, 0x03);
    memory.Write8(0x120A, 0x86);
    memory.Write8(0x120B, 0x40);
    memory.Write8(0x120C, 0xA2);
    memory.Write8(0x120D, 0x04);

    // LDY #$05 / STY $41 / LDY #$06
    memory.Write8(0x120E, 0xA0);
    memory.Write8(0x120F, 0x05);
    memory.Write8(0x1210, 0x84);
    memory.Write8(0x1211, 0x41);
    memory.Write8(0x1212, 0xA0);
    memory.Write8(0x1213, 0x06);

    // CLD / CLC / NOP / ADC #$01
    memory.Write8(0x1214, 0xD8);
    memory.Write8(0x1215, 0x18);
    memory.Write8(0x1216, 0xEA);
    memory.Write8(0x1217, 0x69);
    memory.Write8(0x1218, 0x01);

    // SEC / SBC #$02
    memory.Write8(0x1219, 0x38);
    memory.Write8(0x121A, 0xE9);
    memory.Write8(0x121B, 0x02);

    // CLC / ROL A
    memory.Write8(0x121C, 0x18);
    memory.Write8(0x121D, 0x2A);

    // SEC / ROR $50
    memory.Write8(0x121E, 0x38);
    memory.Write8(0x121F, 0x66);
    memory.Write8(0x1220, 0x50);

    // RTS observes the final registers and flags.
    memory.Write8(0x1221, 0x60);

    // LDX #$00 / STX $42 / BNE $1306
    memory.Write8(0x1300, 0xA2);
    memory.Write8(0x1301, 0x00);
    memory.Write8(0x1302, 0x86);
    memory.Write8(0x1303, 0x42);
    memory.Write8(0x1304, 0xD0);
    memory.Write8(0x1305, 0x00);

    // LDX #$01 / RTS
    memory.Write8(0x1306, 0xA2);
    memory.Write8(0x1307, 0x01);
    memory.Write8(0x1308, 0x60);

    // Collapsible: CLC / LDA $3100 / ADC #$01 /
    // STA $3101.
    memory.Write8(0x1400, 0x18);
    memory.Write8(0x1401, 0xAD);
    memory.Write8(0x1402, 0x00);
    memory.Write8(0x1403, 0x31);
    memory.Write8(0x1404, 0x69);
    memory.Write8(0x1405, 0x01);
    memory.Write8(0x1406, 0x8D);
    memory.Write8(0x1407, 0x01);
    memory.Write8(0x1408, 0x31);

    // Kill the first result and its remaining flags.
    memory.Write8(0x1409, 0xA9);
    memory.Write8(0x140A, 0x00);
    memory.Write8(0x140B, 0xB8);

    // Not collapsible: PHA observes A after STA.
    memory.Write8(0x140C, 0x18);
    memory.Write8(0x140D, 0xAD);
    memory.Write8(0x140E, 0x00);
    memory.Write8(0x140F, 0x32);
    memory.Write8(0x1410, 0x69);
    memory.Write8(0x1411, 0x01);
    memory.Write8(0x1412, 0x8D);
    memory.Write8(0x1413, 0x01);
    memory.Write8(0x1414, 0x32);
    memory.Write8(0x1415, 0x48);
    memory.Write8(0x1416, 0x60);

    // 16-bit SEC / LDA / SBC / STA chain.
    memory.Write8(0x1500, 0x38);
    memory.Write8(0x1501, 0xAD);
    memory.Write8(0x1502, 0x00);
    memory.Write8(0x1503, 0x40);
    memory.Write8(0x1504, 0xED);
    memory.Write8(0x1505, 0x00);
    memory.Write8(0x1506, 0x41);
    memory.Write8(0x1507, 0x8D);
    memory.Write8(0x1508, 0x00);
    memory.Write8(0x1509, 0x42);
    memory.Write8(0x150A, 0xAD);
    memory.Write8(0x150B, 0x01);
    memory.Write8(0x150C, 0x40);
    memory.Write8(0x150D, 0xED);
    memory.Write8(0x150E, 0x01);
    memory.Write8(0x150F, 0x41);
    memory.Write8(0x1510, 0x8D);
    memory.Write8(0x1511, 0x01);
    memory.Write8(0x1512, 0x42);
    memory.Write8(0x1513, 0x60);

    // Dead N/Z: INC $60 twice.
    memory.Write8(0x1600, 0xE6);
    memory.Write8(0x1601, 0x60);
    memory.Write8(0x1602, 0xE6);
    memory.Write8(0x1603, 0x60);

    // Overwrite N/Z.
    memory.Write8(0x1604, 0xA9);
    memory.Write8(0x1605, 0x00);

    // Live N/Z: DEC $61 twice, then BMI.
    memory.Write8(0x1606, 0xC6);
    memory.Write8(0x1607, 0x61);
    memory.Write8(0x1608, 0xC6);
    memory.Write8(0x1609, 0x61);
    memory.Write8(0x160A, 0x30);
    memory.Write8(0x160B, 0x00);
    memory.Write8(0x160C, 0x60);

    // INC $70 / BNE join / INC $71 / RTS
    memory.Write8(0x1700, 0xE6);
    memory.Write8(0x1701, 0x70);
    memory.Write8(0x1702, 0xD0);
    memory.Write8(0x1703, 0x02);
    memory.Write8(0x1704, 0xE6);
    memory.Write8(0x1705, 0x71);
    memory.Write8(0x1706, 0x60);

    // LDA $80 / BNE join / DEC $81 / DEC $80 / RTS
    memory.Write8(0x1800, 0xA5);
    memory.Write8(0x1801, 0x80);
    memory.Write8(0x1802, 0xD0);
    memory.Write8(0x1803, 0x02);
    memory.Write8(0x1804, 0xC6);
    memory.Write8(0x1805, 0x81);
    memory.Write8(0x1806, 0xC6);
    memory.Write8(0x1807, 0x80);
    memory.Write8(0x1808, 0x60);

    // DEC $90 / LDY $90 / TXA / CMP #$7F /
    // STA $92 / INX / STA $93 / TXA /
    // AND #$7F / STA $91 / TXA / ORA #$80 /
    // PHA / TYA / CLC / ADC #$01 / PHA /
    // TXA / CLC / ROL A / PHA / TYA /
    // ASL A / PHA / TXA / TAY / TAX / TYA / RTS
    memory.Write8(0x1900, 0xC6);
    memory.Write8(0x1901, 0x90);
    memory.Write8(0x1902, 0xA4);
    memory.Write8(0x1903, 0x90);
    memory.Write8(0x1904, 0x8A);
    memory.Write8(0x1905, 0xC9);
    memory.Write8(0x1906, 0x7F);
    memory.Write8(0x1907, 0x85);
    memory.Write8(0x1908, 0x92);
    memory.Write8(0x1909, 0xE8);
    memory.Write8(0x190A, 0x85);
    memory.Write8(0x190B, 0x93);
    memory.Write8(0x190C, 0x8A);
    memory.Write8(0x190D, 0x29);
    memory.Write8(0x190E, 0x7F);
    memory.Write8(0x190F, 0x85);
    memory.Write8(0x1910, 0x91);
    memory.Write8(0x1911, 0x8A);
    memory.Write8(0x1912, 0x09);
    memory.Write8(0x1913, 0x80);
    memory.Write8(0x1914, 0x48);
    memory.Write8(0x1915, 0x98);
    memory.Write8(0x1916, 0x18);
    memory.Write8(0x1917, 0x69);
    memory.Write8(0x1918, 0x01);
    memory.Write8(0x1919, 0x48);
    memory.Write8(0x191A, 0x8A);
    memory.Write8(0x191B, 0x18);
    memory.Write8(0x191C, 0x2A);
    memory.Write8(0x191D, 0x48);
    memory.Write8(0x191E, 0x98);
    memory.Write8(0x191F, 0x0A);
    memory.Write8(0x1920, 0x48);
    memory.Write8(0x1921, 0x8A);
    memory.Write8(0x1922, 0xA8);
    memory.Write8(0x1923, 0xAA);
    memory.Write8(0x1924, 0x98);
    memory.Write8(0x1925, 0x60);

    // LDA $A0 / ORA $A1 / BNE join / LDX #$01 / RTS
    memory.Write8(0x1A00, 0xA5);
    memory.Write8(0x1A01, 0xA0);
    memory.Write8(0x1A02, 0x05);
    memory.Write8(0x1A03, 0xA1);
    memory.Write8(0x1A04, 0xD0);
    memory.Write8(0x1A05, 0x02);
    memory.Write8(0x1A06, 0xA2);
    memory.Write8(0x1A07, 0x01);
    memory.Write8(0x1A08, 0x60);

    // Standalone formatter inputs: TYA / EOR #$FF /
    // PLA / AND #$0F.
    memory.Write8(0x1B00, 0x98);
    memory.Write8(0x1B01, 0x49);
    memory.Write8(0x1B02, 0xFF);
    memory.Write8(0x1B03, 0x68);
    memory.Write8(0x1B04, 0x29);
    memory.Write8(0x1B05, 0x0F);
    memory.Write8(0x1B06, 0x24);
    memory.Write8(0x1B07, 0x20);
    memory.Write8(0x1B08, 0x46);
    memory.Write8(0x1B09, 0x21);
    memory.Write8(0x1B0A, 0xBA);
    memory.Write8(0x1B0B, 0xE8);
    memory.Write8(0x1B0C, 0xC6);
    memory.Write8(0x1B0D, 0x22);
    memory.Write8(0x1B0E, 0xE0);
    memory.Write8(0x1B0F, 0x10);
    memory.Write8(0x1B10, 0xC0);
    memory.Write8(0x1B11, 0x11);

    // Two constant-propagation diamonds. The first merge
    // receives $22 from both inputs; the second receives
    // different constants and must become unknown.
    memory.Write8(0x1C00, 0xA9);
    memory.Write8(0x1C01, 0x11);
    memory.Write8(0x1C02, 0xD0);
    memory.Write8(0x1C03, 0x05);
    memory.Write8(0x1C04, 0xA9);
    memory.Write8(0x1C05, 0x22);
    memory.Write8(0x1C06, 0x4C);
    memory.Write8(0x1C07, 0x0B);
    memory.Write8(0x1C08, 0x1C);
    memory.Write8(0x1C09, 0xA9);
    memory.Write8(0x1C0A, 0x22);
    memory.Write8(0x1C0B, 0x85);
    memory.Write8(0x1C0C, 0x94);
    memory.Write8(0x1C0D, 0xD0);
    memory.Write8(0x1C0E, 0x05);
    memory.Write8(0x1C0F, 0xA9);
    memory.Write8(0x1C10, 0x33);
    memory.Write8(0x1C11, 0x4C);
    memory.Write8(0x1C12, 0x16);
    memory.Write8(0x1C13, 0x1C);
    memory.Write8(0x1C14, 0xA9);
    memory.Write8(0x1C15, 0x44);
    memory.Write8(0x1C16, 0x85);
    memory.Write8(0x1C17, 0x95);
    memory.Write8(0x1C18, 0x60);

    atari::BasicBlockAnalysisResult basicBlocks;

    atari::RoutineBasicBlocks routineBlocks;

    routineBlocks.routineEntryAddress =
        EntryAddress;

    routineBlocks.routineName =
        "TestRoutine";

    atari::BasicBlock headerBlock;

    headerBlock.beginAddress =
        EntryAddress;

    headerBlock.endAddress =
        0x1009;

    headerBlock.instructionAddresses =
        {
            0x1000,
            0x1002,
            0x1004,
            ProducerAddress,
            BranchAddress};

    atari::BasicBlockEdge thenEdge;

    thenEdge.targetAddress =
        ThenAddress;

    thenEdge.type =
        atari::BasicBlockEdgeType::BranchTaken;

    headerBlock.successors.push_back(
        thenEdge);

    atari::BasicBlock thenBlock;

    thenBlock.beginAddress =
        ThenAddress;

    thenBlock.endAddress =
        0x100D;

    thenBlock.instructionAddresses =
        {ThenAddress, 0x100D};

    thenBlock.terminal =
        true;

    routineBlocks.blocks =
        {headerBlock, thenBlock};

    basicBlocks.routines.push_back(
        routineBlocks);

    atari::RoutineBasicBlocks copyRoutineBlocks;

    copyRoutineBlocks.routineEntryAddress =
        0x1200;

    copyRoutineBlocks.routineName =
        "CopyRoutine";

    atari::BasicBlock copyBlock;

    copyBlock.beginAddress =
        0x1200;

    copyBlock.endAddress =
        0x1214;

    copyBlock.instructionAddresses =
        {
            0x1200,
            0x1203,
            0x1206,
            0x1208,
            0x120A,
            0x120C,
            0x120E,
            0x1210,
            0x1212,
            0x1214};

    atari::BasicBlockEdge arithmeticEdge;

    arithmeticEdge.targetAddress =
        0x1215;

    arithmeticEdge.type =
        atari::BasicBlockEdgeType::FallThrough;

    copyBlock.successors.push_back(
        arithmeticEdge);

    atari::BasicBlock arithmeticBlock;

    arithmeticBlock.beginAddress =
        0x1215;

    arithmeticBlock.endAddress =
        0x1221;

    arithmeticBlock.instructionAddresses =
        {
            0x1215,
            0x1216,
            0x1217,
            0x1219,
            0x121A,
            0x121C,
            0x121D,
            0x121E,
            0x121F,
            0x1221};

    arithmeticBlock.terminal =
        true;

    copyRoutineBlocks.blocks =
        {copyBlock, arithmeticBlock};

    basicBlocks.routines.push_back(
        copyRoutineBlocks);

    atari::RoutineBasicBlocks flagRoutineBlocks;

    flagRoutineBlocks.routineEntryAddress =
        0x1300;

    flagRoutineBlocks.routineName =
        "FlagRoutine";

    atari::BasicBlock flagHeaderBlock;

    flagHeaderBlock.beginAddress =
        0x1300;

    flagHeaderBlock.endAddress =
        0x1305;

    flagHeaderBlock.instructionAddresses =
        {0x1300, 0x1302, 0x1304};

    atari::BasicBlockEdge flagEdge;

    flagEdge.targetAddress =
        0x1306;

    flagEdge.type =
        atari::BasicBlockEdgeType::BranchTaken;

    flagHeaderBlock.successors.push_back(
        flagEdge);

    atari::BasicBlock flagExitBlock;

    flagExitBlock.beginAddress =
        0x1306;

    flagExitBlock.endAddress =
        0x1308;

    flagExitBlock.instructionAddresses =
        {0x1306, 0x1308};

    flagExitBlock.terminal =
        true;

    flagRoutineBlocks.blocks =
        {flagHeaderBlock, flagExitBlock};

    basicBlocks.routines.push_back(
        flagRoutineBlocks);

    atari::RoutineBasicBlocks arithmeticRoutineBlocks;

    arithmeticRoutineBlocks.routineEntryAddress =
        0x1400;

    arithmeticRoutineBlocks.routineName =
        "ArithmeticRoutine";

    atari::BasicBlock arithmeticRoutineBlock;

    arithmeticRoutineBlock.beginAddress =
        0x1400;

    arithmeticRoutineBlock.endAddress =
        0x1416;

    arithmeticRoutineBlock.instructionAddresses =
        {
            0x1400,
            0x1401,
            0x1404,
            0x1406,
            0x1409,
            0x140B,
            0x140C,
            0x140D,
            0x1410,
            0x1412,
            0x1415,
            0x1416};

    arithmeticRoutineBlock.terminal =
        true;

    arithmeticRoutineBlocks.blocks.push_back(
        arithmeticRoutineBlock);

    basicBlocks.routines.push_back(
        arithmeticRoutineBlocks);

    atari::RoutineBasicBlocks wideRoutineBlocks;

    wideRoutineBlocks.routineEntryAddress =
        0x1500;

    wideRoutineBlocks.routineName =
        "WideRoutine";

    atari::BasicBlock wideBlock;

    wideBlock.beginAddress =
        0x1500;

    wideBlock.endAddress =
        0x1513;

    wideBlock.instructionAddresses =
        {
            0x1500,
            0x1501,
            0x1504,
            0x1507,
            0x150A,
            0x150D,
            0x1510,
            0x1513};

    wideBlock.terminal =
        true;

    wideRoutineBlocks.blocks.push_back(
        wideBlock);

    basicBlocks.routines.push_back(
        wideRoutineBlocks);

    atari::RoutineBasicBlocks repetitionRoutineBlocks;

    repetitionRoutineBlocks.routineEntryAddress =
        0x1600;

    repetitionRoutineBlocks.routineName =
        "RepetitionRoutine";

    atari::BasicBlock repetitionHeader;

    repetitionHeader.beginAddress =
        0x1600;

    repetitionHeader.endAddress =
        0x160B;

    repetitionHeader.instructionAddresses =
        {
            0x1600,
            0x1602,
            0x1604,
            0x1606,
            0x1608,
            0x160A};

    atari::BasicBlockEdge repetitionEdge;

    repetitionEdge.targetAddress =
        0x160C;

    repetitionEdge.type =
        atari::BasicBlockEdgeType::BranchTaken;

    repetitionHeader.successors.push_back(
        repetitionEdge);

    atari::BasicBlock repetitionExit;

    repetitionExit.beginAddress =
        0x160C;

    repetitionExit.endAddress =
        0x160C;

    repetitionExit.instructionAddresses =
        {0x160C};

    repetitionExit.terminal =
        true;

    repetitionRoutineBlocks.blocks =
        {repetitionHeader, repetitionExit};

    basicBlocks.routines.push_back(
        repetitionRoutineBlocks);

    atari::RoutineBasicBlocks pointerRoutineBlocks;

    pointerRoutineBlocks.routineEntryAddress =
        0x1700;

    pointerRoutineBlocks.routineName =
        "PointerRoutine";

    atari::BasicBlock pointerHeader;

    pointerHeader.beginAddress =
        0x1700;

    pointerHeader.endAddress =
        0x1703;

    pointerHeader.instructionAddresses =
        {0x1700, 0x1702};

    atari::BasicBlockEdge pointerSkipEdge;

    pointerSkipEdge.targetAddress =
        0x1706;

    pointerSkipEdge.type =
        atari::BasicBlockEdgeType::BranchTaken;

    pointerHeader.successors.push_back(
        pointerSkipEdge);

    atari::BasicBlockEdge pointerBodyEdge;

    pointerBodyEdge.targetAddress =
        0x1704;

    pointerBodyEdge.type =
        atari::BasicBlockEdgeType::FallThrough;

    pointerHeader.successors.push_back(
        pointerBodyEdge);

    atari::BasicBlock pointerBody;

    pointerBody.beginAddress =
        0x1704;

    pointerBody.endAddress =
        0x1705;

    pointerBody.instructionAddresses =
        {0x1704};

    atari::BasicBlockEdge pointerJoinEdge;

    pointerJoinEdge.targetAddress =
        0x1706;

    pointerJoinEdge.type =
        atari::BasicBlockEdgeType::FallThrough;

    pointerBody.successors.push_back(
        pointerJoinEdge);

    atari::BasicBlock pointerJoin;

    pointerJoin.beginAddress =
        0x1706;

    pointerJoin.endAddress =
        0x1706;

    pointerJoin.instructionAddresses =
        {0x1706};

    pointerJoin.terminal =
        true;

    pointerRoutineBlocks.blocks =
        {pointerHeader, pointerBody, pointerJoin};

    basicBlocks.routines.push_back(
        pointerRoutineBlocks);

    atari::RoutineBasicBlocks decrementRoutineBlocks;

    decrementRoutineBlocks.routineEntryAddress =
        0x1800;

    decrementRoutineBlocks.routineName =
        "DecrementRoutine";

    atari::BasicBlock decrementHeader;

    decrementHeader.beginAddress =
        0x1800;

    decrementHeader.endAddress =
        0x1803;

    decrementHeader.instructionAddresses =
        {0x1800, 0x1802};

    atari::BasicBlockEdge decrementSkipEdge;

    decrementSkipEdge.targetAddress =
        0x1806;

    decrementSkipEdge.type =
        atari::BasicBlockEdgeType::BranchTaken;

    decrementHeader.successors.push_back(
        decrementSkipEdge);

    atari::BasicBlockEdge decrementBodyEdge;

    decrementBodyEdge.targetAddress =
        0x1804;

    decrementBodyEdge.type =
        atari::BasicBlockEdgeType::FallThrough;

    decrementHeader.successors.push_back(
        decrementBodyEdge);

    atari::BasicBlock decrementBody;

    decrementBody.beginAddress =
        0x1804;

    decrementBody.endAddress =
        0x1805;

    decrementBody.instructionAddresses =
        {0x1804};

    atari::BasicBlockEdge decrementJoinEdge;

    decrementJoinEdge.targetAddress =
        0x1806;

    decrementJoinEdge.type =
        atari::BasicBlockEdgeType::FallThrough;

    decrementBody.successors.push_back(
        decrementJoinEdge);

    atari::BasicBlock decrementJoin;

    decrementJoin.beginAddress =
        0x1806;

    decrementJoin.endAddress =
        0x1808;

    decrementJoin.instructionAddresses =
        {0x1806, 0x1808};

    decrementJoin.terminal =
        true;

    decrementRoutineBlocks.blocks =
        {
            decrementHeader,
            decrementBody,
            decrementJoin};

    basicBlocks.routines.push_back(
        decrementRoutineBlocks);

    atari::RoutineBasicBlocks fusionRoutineBlocks;

    fusionRoutineBlocks.routineEntryAddress =
        0x1900;

    fusionRoutineBlocks.routineName =
        "FusionRoutine";

    atari::BasicBlock fusionBlock;

    fusionBlock.beginAddress =
        0x1900;

    fusionBlock.endAddress =
        0x1904;

    fusionBlock.instructionAddresses =
        {
            0x1900,
            0x1902,
            0x1904};

    atari::BasicBlockEdge fusionEdge;

    fusionEdge.targetAddress =
        0x1905;

    fusionEdge.type =
        atari::BasicBlockEdgeType::FallThrough;

    fusionBlock.successors.push_back(
        fusionEdge);

    atari::BasicBlock fusionTailBlock;

    fusionTailBlock.beginAddress =
        0x1905;

    fusionTailBlock.endAddress =
        0x1911;

    fusionTailBlock.instructionAddresses =
        {
            0x1905,
            0x1907,
            0x1909,
            0x190A,
            0x190C,
            0x190D,
            0x190F,
            0x1911};

    atari::BasicBlockEdge bitwiseTailEdge;

    bitwiseTailEdge.targetAddress =
        0x1912;

    bitwiseTailEdge.type =
        atari::BasicBlockEdgeType::FallThrough;

    fusionTailBlock.successors.push_back(
        bitwiseTailEdge);

    atari::BasicBlock fusionBitwiseTailBlock;

    fusionBitwiseTailBlock.beginAddress =
        0x1912;

    fusionBitwiseTailBlock.endAddress =
        0x1915;

    fusionBitwiseTailBlock.instructionAddresses =
        {
            0x1912,
            0x1914,
            0x1915};

    atari::BasicBlockEdge arithmeticTailEdge;

    arithmeticTailEdge.targetAddress =
        0x1916;

    arithmeticTailEdge.type =
        atari::BasicBlockEdgeType::FallThrough;

    fusionBitwiseTailBlock.successors.push_back(
        arithmeticTailEdge);

    atari::BasicBlock fusionArithmeticTailBlock;

    fusionArithmeticTailBlock.beginAddress =
        0x1916;

    fusionArithmeticTailBlock.endAddress =
        0x191A;

    fusionArithmeticTailBlock.instructionAddresses =
        {
            0x1916,
            0x1917,
            0x1919,
            0x191A};

    atari::BasicBlockEdge rotateTailEdge;

    rotateTailEdge.targetAddress =
        0x191B;

    rotateTailEdge.type =
        atari::BasicBlockEdgeType::FallThrough;

    fusionArithmeticTailBlock.successors.push_back(
        rotateTailEdge);

    atari::BasicBlock fusionRotateTailBlock;

    fusionRotateTailBlock.beginAddress =
        0x191B;

    fusionRotateTailBlock.endAddress =
        0x191E;

    fusionRotateTailBlock.instructionAddresses =
        {
            0x191B,
            0x191C,
            0x191D,
            0x191E};

    atari::BasicBlockEdge shiftTailEdge;

    shiftTailEdge.targetAddress =
        0x191F;

    shiftTailEdge.type =
        atari::BasicBlockEdgeType::FallThrough;

    fusionRotateTailBlock.successors.push_back(
        shiftTailEdge);

    atari::BasicBlock fusionShiftTailBlock;

    fusionShiftTailBlock.beginAddress =
        0x191F;

    fusionShiftTailBlock.endAddress =
        0x1921;

    fusionShiftTailBlock.instructionAddresses =
        {
            0x191F,
            0x1920,
            0x1921};

    atari::BasicBlockEdge transferTailEdge;

    transferTailEdge.targetAddress =
        0x1922;

    transferTailEdge.type =
        atari::BasicBlockEdgeType::FallThrough;

    fusionShiftTailBlock.successors.push_back(
        transferTailEdge);

    atari::BasicBlock fusionTransferTailBlock;

    fusionTransferTailBlock.beginAddress =
        0x1922;

    fusionTransferTailBlock.endAddress =
        0x1925;

    fusionTransferTailBlock.instructionAddresses =
        {
            0x1922,
            0x1923,
            0x1924,
            0x1925};

    fusionTransferTailBlock.terminal =
        true;

    fusionRoutineBlocks.blocks =
        {
            fusionBlock,
            fusionTailBlock,
            fusionBitwiseTailBlock,
            fusionArithmeticTailBlock,
            fusionRotateTailBlock,
            fusionShiftTailBlock,
            fusionTransferTailBlock};

    basicBlocks.routines.push_back(
        fusionRoutineBlocks);

    atari::RoutineBasicBlocks bitwiseRoutineBlocks;

    bitwiseRoutineBlocks.routineEntryAddress =
        0x1A00;

    bitwiseRoutineBlocks.routineName =
        "BitwiseRoutine";

    atari::BasicBlock bitwiseHeader;

    bitwiseHeader.beginAddress =
        0x1A00;

    bitwiseHeader.endAddress =
        0x1A05;

    bitwiseHeader.instructionAddresses =
        {0x1A00, 0x1A02, 0x1A04};

    atari::BasicBlockEdge bitwiseSkipEdge;

    bitwiseSkipEdge.targetAddress =
        0x1A08;

    bitwiseSkipEdge.type =
        atari::BasicBlockEdgeType::BranchTaken;

    bitwiseHeader.successors.push_back(
        bitwiseSkipEdge);

    atari::BasicBlockEdge bitwiseBodyEdge;

    bitwiseBodyEdge.targetAddress =
        0x1A06;

    bitwiseBodyEdge.type =
        atari::BasicBlockEdgeType::FallThrough;

    bitwiseHeader.successors.push_back(
        bitwiseBodyEdge);

    atari::BasicBlock bitwiseBody;

    bitwiseBody.beginAddress =
        0x1A06;

    bitwiseBody.endAddress =
        0x1A07;

    bitwiseBody.instructionAddresses =
        {0x1A06};

    atari::BasicBlockEdge bitwiseJoinEdge;

    bitwiseJoinEdge.targetAddress =
        0x1A08;

    bitwiseJoinEdge.type =
        atari::BasicBlockEdgeType::FallThrough;

    bitwiseBody.successors.push_back(
        bitwiseJoinEdge);

    atari::BasicBlock bitwiseJoin;

    bitwiseJoin.beginAddress =
        0x1A08;

    bitwiseJoin.endAddress =
        0x1A08;

    bitwiseJoin.instructionAddresses =
        {0x1A08};

    bitwiseJoin.terminal =
        true;

    bitwiseRoutineBlocks.blocks =
        {bitwiseHeader, bitwiseBody, bitwiseJoin};

    basicBlocks.routines.push_back(
        bitwiseRoutineBlocks);

    atari::RoutineBasicBlocks constantMergeRoutine;

    constantMergeRoutine.routineEntryAddress =
        0x1C00;

    constantMergeRoutine.routineName =
        "ConstantMergeRoutine";

    atari::BasicBlock constantEntry;

    constantEntry.beginAddress = 0x1C00;
    constantEntry.endAddress = 0x1C03;
    constantEntry.instructionAddresses =
        {0x1C00, 0x1C02};
    constantEntry.successors =
        {
            {0x1C04,
             atari::BasicBlockEdgeType::FallThrough},
            {0x1C09,
             atari::BasicBlockEdgeType::BranchTaken}};

    atari::BasicBlock sameConstantLeft;

    sameConstantLeft.beginAddress = 0x1C04;
    sameConstantLeft.endAddress = 0x1C08;
    sameConstantLeft.instructionAddresses =
        {0x1C04, 0x1C06};
    sameConstantLeft.successors =
        {
            {0x1C0B,
             atari::BasicBlockEdgeType::Jump}};

    atari::BasicBlock sameConstantRight;

    sameConstantRight.beginAddress = 0x1C09;
    sameConstantRight.endAddress = 0x1C0A;
    sameConstantRight.instructionAddresses =
        {0x1C09};
    sameConstantRight.successors =
        {
            {0x1C0B,
             atari::BasicBlockEdgeType::FallThrough}};

    atari::BasicBlock sameConstantJoin;

    sameConstantJoin.beginAddress = 0x1C0B;
    sameConstantJoin.endAddress = 0x1C0E;
    sameConstantJoin.instructionAddresses =
        {0x1C0B, 0x1C0D};
    sameConstantJoin.successors =
        {
            {0x1C0F,
             atari::BasicBlockEdgeType::FallThrough},
            {0x1C14,
             atari::BasicBlockEdgeType::BranchTaken}};

    atari::BasicBlock differentConstantLeft;

    differentConstantLeft.beginAddress = 0x1C0F;
    differentConstantLeft.endAddress = 0x1C13;
    differentConstantLeft.instructionAddresses =
        {0x1C0F, 0x1C11};
    differentConstantLeft.successors =
        {
            {0x1C16,
             atari::BasicBlockEdgeType::Jump}};

    atari::BasicBlock differentConstantRight;

    differentConstantRight.beginAddress = 0x1C14;
    differentConstantRight.endAddress = 0x1C15;
    differentConstantRight.instructionAddresses =
        {0x1C14};
    differentConstantRight.successors =
        {
            {0x1C16,
             atari::BasicBlockEdgeType::FallThrough}};

    atari::BasicBlock differentConstantJoin;

    differentConstantJoin.beginAddress = 0x1C16;
    differentConstantJoin.endAddress = 0x1C18;
    differentConstantJoin.instructionAddresses =
        {0x1C16, 0x1C18};
    differentConstantJoin.terminal = true;

    constantMergeRoutine.blocks =
        {
            constantEntry,
            sameConstantLeft,
            sameConstantRight,
            sameConstantJoin,
            differentConstantLeft,
            differentConstantRight,
            differentConstantJoin};

    basicBlocks.routines.push_back(
        constantMergeRoutine);

    atari::StructuredControlFlowAnalysisResult
        structuredFlow;

    atari::RoutineStructuredControlFlowAnalysis
        structuredRoutine;

    structuredRoutine.routineEntryAddress =
        EntryAddress;

    structuredRoutine.routineName =
        "TestRoutine";

    atari::StructuredIf statement;

    statement.headerAddress =
        EntryAddress;

    statement.instructionAddress =
        BranchAddress;

    statement.thenState =
        atari::FlagState::Clear;

    statement.thenEntryAddress =
        ThenAddress;

    statement.thenBlocks =
        {ThenAddress};

    structuredRoutine.ifStatements.push_back(
        statement);

    structuredFlow.routines.push_back(
        structuredRoutine);

    atari::RoutineStructuredControlFlowAnalysis
        pointerStructuredRoutine;

    pointerStructuredRoutine.routineEntryAddress =
        0x1700;

    pointerStructuredRoutine.routineName =
        "PointerRoutine";

    atari::StructuredIf pointerStatement;

    pointerStatement.headerAddress =
        0x1700;

    pointerStatement.instructionAddress =
        0x1702;

    pointerStatement.joinAddress =
        0x1706;

    pointerStatement.sourceInstruction =
        atari::cpu6502::Instruction::BNE;

    pointerStatement.flag =
        atari::ProcessorFlag::Zero;

    pointerStatement.thenState =
        atari::FlagState::Set;

    pointerStatement.branchConditionInverted =
        true;

    pointerStatement.thenEntryAddress =
        0x1704;

    pointerStatement.thenBlocks =
        {0x1704};

    pointerStructuredRoutine.ifStatements.push_back(
        pointerStatement);

    structuredFlow.routines.push_back(
        pointerStructuredRoutine);

    atari::RoutineStructuredControlFlowAnalysis
        decrementStructuredRoutine;

    decrementStructuredRoutine.routineEntryAddress =
        0x1800;

    decrementStructuredRoutine.routineName =
        "DecrementRoutine";

    atari::StructuredIf decrementStatement;

    decrementStatement.headerAddress =
        0x1800;

    decrementStatement.instructionAddress =
        0x1802;

    decrementStatement.joinAddress =
        0x1806;

    decrementStatement.sourceInstruction =
        atari::cpu6502::Instruction::BNE;

    decrementStatement.flag =
        atari::ProcessorFlag::Zero;

    decrementStatement.thenState =
        atari::FlagState::Set;

    decrementStatement.branchConditionInverted =
        true;

    decrementStatement.thenEntryAddress =
        0x1804;

    decrementStatement.thenBlocks =
        {0x1804};

    decrementStructuredRoutine.ifStatements.push_back(
        decrementStatement);

    structuredFlow.routines.push_back(
        decrementStructuredRoutine);

    atari::RoutineStructuredControlFlowAnalysis
        bitwiseStructuredRoutine;

    bitwiseStructuredRoutine.routineEntryAddress =
        0x1A00;

    bitwiseStructuredRoutine.routineName =
        "BitwiseRoutine";

    atari::StructuredIf bitwiseStatement;

    bitwiseStatement.headerAddress =
        0x1A00;

    bitwiseStatement.instructionAddress =
        0x1A04;

    bitwiseStatement.joinAddress =
        0x1A08;

    bitwiseStatement.sourceInstruction =
        atari::cpu6502::Instruction::BNE;

    bitwiseStatement.flag =
        atari::ProcessorFlag::Zero;

    bitwiseStatement.thenState =
        atari::FlagState::Set;

    bitwiseStatement.branchConditionInverted =
        true;

    bitwiseStatement.thenEntryAddress =
        0x1A06;

    bitwiseStatement.thenBlocks =
        {0x1A06};

    bitwiseStructuredRoutine.ifStatements.push_back(
        bitwiseStatement);

    structuredFlow.routines.push_back(
        bitwiseStructuredRoutine);

    atari::SemanticConditionAnalysisResult
        semanticConditions;

    atari::RoutineSemanticConditionAnalysis
        semanticRoutine;

    semanticRoutine.routineEntryAddress =
        EntryAddress;

    atari::SemanticCondition condition;

    condition.branchAddress =
        BranchAddress;

    condition.branchTakenState =
        atari::FlagState::Clear;

    condition.fallthroughState =
        atari::FlagState::Set;

    condition.producerFound =
        true;

    condition.producerAddress =
        ProducerAddress;

    condition.semanticResolved =
        true;

    condition.branchTakenExpression =
        "X != 0";

    condition.fallthroughExpression =
        "X == 0";

    semanticRoutine.conditions.push_back(
        condition);

    semanticConditions.routines.push_back(
        semanticRoutine);

    atari::RoutineSemanticConditionAnalysis
        pointerSemanticRoutine;

    pointerSemanticRoutine.routineEntryAddress =
        0x1700;

    atari::SemanticCondition pointerCondition;

    pointerCondition.branchAddress =
        0x1702;

    pointerCondition.branchInstruction =
        atari::cpu6502::Instruction::BNE;

    pointerCondition.flag =
        atari::ProcessorFlag::Zero;

    pointerCondition.branchTakenState =
        atari::FlagState::Clear;

    pointerCondition.fallthroughState =
        atari::FlagState::Set;

    pointerCondition.producerFound =
        true;

    pointerCondition.producerAddress =
        0x1700;

    pointerCondition.producerInstruction =
        atari::cpu6502::Instruction::INC;

    pointerCondition.semanticResolved =
        true;

    pointerCondition.branchTakenExpression =
        "memory[$70] != 0";

    pointerCondition.fallthroughExpression =
        "memory[$70] == 0";

    pointerSemanticRoutine.conditions.push_back(
        pointerCondition);

    semanticConditions.routines.push_back(
        pointerSemanticRoutine);

    atari::RoutineSemanticConditionAnalysis
        decrementSemanticRoutine;

    decrementSemanticRoutine.routineEntryAddress =
        0x1800;

    atari::SemanticCondition decrementCondition;

    decrementCondition.branchAddress =
        0x1802;

    decrementCondition.branchInstruction =
        atari::cpu6502::Instruction::BNE;

    decrementCondition.flag =
        atari::ProcessorFlag::Zero;

    decrementCondition.branchTakenState =
        atari::FlagState::Clear;

    decrementCondition.fallthroughState =
        atari::FlagState::Set;

    decrementCondition.producerFound =
        true;

    decrementCondition.producerAddress =
        0x1800;

    decrementCondition.producerInstruction =
        atari::cpu6502::Instruction::LDA;

    decrementCondition.semanticResolved =
        true;

    decrementCondition.branchTakenExpression =
        "memory[$80] != 0";

    decrementCondition.fallthroughExpression =
        "memory[$80] == 0";

    decrementSemanticRoutine.conditions.push_back(
        decrementCondition);

    semanticConditions.routines.push_back(
        decrementSemanticRoutine);

    atari::RoutineSemanticConditionAnalysis
        bitwiseSemanticRoutine;

    bitwiseSemanticRoutine.routineEntryAddress =
        0x1A00;

    atari::SemanticCondition bitwiseCondition;

    bitwiseCondition.branchAddress =
        0x1A04;

    bitwiseCondition.branchInstruction =
        atari::cpu6502::Instruction::BNE;

    bitwiseCondition.flag =
        atari::ProcessorFlag::Zero;

    bitwiseCondition.branchTakenState =
        atari::FlagState::Clear;

    bitwiseCondition.fallthroughState =
        atari::FlagState::Set;

    bitwiseCondition.producerFound =
        true;

    bitwiseCondition.producerAddress =
        0x1A02;

    bitwiseCondition.producerInstruction =
        atari::cpu6502::Instruction::ORA;

    bitwiseCondition.semanticResolved =
        true;

    bitwiseCondition.branchTakenExpression =
        "A != 0";

    bitwiseCondition.fallthroughExpression =
        "A == 0";

    bitwiseSemanticRoutine.conditions.push_back(
        bitwiseCondition);

    semanticConditions.routines.push_back(
        bitwiseSemanticRoutine);

    atari::LoopStructureAnalysisResult
        loopStructures;

    atari::DisassemblyMetadata metadata;

    atari::StructuredExpressionBuilder builder;

    const auto expressions =
        builder.Build(
            project,
            basicBlocks,
            metadata,
            structuredFlow,
            semanticConditions,
            loopStructures);

    atari::StructuredCodeGenerator generator;

    const std::string code =
        generator.Generate(
            expressions);

    const std::string translationUnit =
        atari::StructuredTranslationUnitGenerator{}
            .Generate(
                project,
                expressions);

    bool passed = true;

    passed &=
        Expect(
            expressions.RootCount() == 11,
            "all routine roots must be generated");

    passed &=
        Expect(
            expressions.ExpressionCount() == 82,
            "all routines must form eighty-two expressions");

    passed &=
        Expect(
            expressions.StatementCount() == 80,
            "all routines must contain eighty statements");

    passed &=
        Expect(
            Contains(
                code,
                "void TestRoutine()"),
            "routine name must be preserved");

    passed &=
        Expect(
            Contains(
                translationUnit,
                "std::array<byte6502, 65536> memory{};") &&
            Contains(
                translationUnit,
                "void TestRoutine();") &&
            Contains(
                translationUnit,
                "memory[0x94] = 0x22;") &&
            !Contains(
                translationUnit,
                "memory[$94]"),
            "translation unit must include runtime, declarations, and C++ hex literals");

    passed &=
        Expect(
            Contains(
                translationUnit,
                "void initialize_image6502()") &&
            Contains(
                translationUnit,
                "void run_entry6502()"),
            "translation unit API must expose initialization and entry wrappers");

    passed &=
        Expect(
            Contains(
                translationUnit,
                "void run_entry6502()\n{\n    TestRoutine();"),
            "translation unit entry wrapper must invoke the recovered RUNAD routine");

    passed &=
        Expect(
            Contains(
                code,
                "memory[$94] = $22;"),
            "equal incoming constants must survive CFG merge");

    passed &=
        Expect(
            Contains(
                code,
                "memory[$95] = A;"),
            "different incoming constants must become unknown");

    passed &=
        Expect(
            Contains(
                code,
                "X = $00;"),
            "LDX must become a register assignment");

    passed &=
        Expect(
            Contains(
                code,
                "A = $42;"),
            "LDA must become a register assignment");

    passed &=
        Expect(
            Contains(
                code,
                "memory[$2000] = $42;"),
            "STA must use a known accumulator constant");

    passed &=
        Expect(
            Contains(
                code,
                "if (X != 0)"),
            "semantic branch expression must control IF");

    passed &=
        Expect(
            Contains(
                code,
                "return;"),
            "RTS must be emitted as return");

    passed &=
        Expect(
            Contains(
                code,
                "sub_1234();"),
            "JSR must become a routine call");

    passed &=
        Expect(
            Contains(
                code,
                "memory[$3001] = memory[$3000];"),
            "dead LDA/STA accumulator transfer must collapse");

    passed &=
        Expect(
            !Contains(
                code,
                "A = memory[$3000];"),
            "collapsed transfer must omit the dead A assignment");

    passed &=
        Expect(
            Contains(
                code,
                "A = $02;"),
            "live accumulator value before RTS must be preserved");

    passed &=
        Expect(
            Contains(
                code,
                "memory[$40] = $03;"),
            "dead LDX/STX transfer must collapse");

    passed &=
        Expect(
            Contains(
                code,
                "memory[$41] = $05;"),
            "dead LDY/STY transfer must collapse");

    passed &=
        Expect(
            Contains(
                code,
                "X = $04;"),
            "live X value before RTS must be preserved");

    passed &=
        Expect(
            Contains(
                code,
                "Y = $06;"),
            "live Y value before RTS must be preserved");

    passed &=
        Expect(
            Contains(
                code,
                "load6502($00, &X, &N, &Z);\n"
                "    memory[$42] = X;\n    BNE"),
            "load/store feeding N/Z branch must not collapse");

    passed &=
        Expect(
            !Contains(
                code,
                "memory[$42] = $00;"),
            "live N/Z flags must prevent register transfer collapse");

    passed &=
        Expect(
            Contains(
                code,
                "A = byte($02 + $01);"),
            "known A and D=0 ADC must become binary arithmetic");

    passed &=
        Expect(
            Contains(
                code,
                "sbc6502(A, $02, 1, D, &A, &C, &V, &N, &Z);"),
            "SEC and SBC must preserve live output flags");

    passed &=
        Expect(
            Contains(
                code,
                "A = rol6502_value(A, 0);"),
            "CLC and ROL must omit dead output flags");

    passed &=
        Expect(
            Contains(
                code,
                "ror6502(memory[$50], 1"),
            "SEC and memory ROR must fold into carry-in one");

    passed &=
        Expect(
            CountOccurrences(
                code,
                "C = 0;") == 0,
            "carry setup must fold through neutral instructions");

    passed &=
        Expect(
            Contains(
                code,
                "memory[$3101] = adc6502_value("
                "memory[$3100], $01, 0, D);"),
            "dead A arithmetic transfer must collapse");

    passed &=
        Expect(
            !Contains(
                code,
                "A = memory[$3100];"),
            "collapsed arithmetic transfer must omit A load");

    passed &=
        Expect(
            Contains(
                code,
                "A = memory[$3200];"),
            "live A arithmetic transfer must retain its load");

    passed &=
        Expect(
            Contains(
                code,
                "memory[$3201] = A;"),
            "live A arithmetic transfer must retain its store");

    passed &=
        Expect(
            Contains(
                code,
                "sbc16_6502(memory16[$4000], "
                "memory16[$4100], 1, D, "
                "&memory16[$4200], &A, &C, &V, &N, &Z);"),
            "consecutive little-endian subtraction must collapse");

    passed &=
        Expect(
            !Contains(
                code,
                "A = memory[$4000];"),
            "wide subtraction must omit byte-wise A loads");

    passed &=
        Expect(
            Contains(
                code,
                "memory[$60] += 2;"),
            "dead N/Z repeated INC must become addition");

    passed &=
        Expect(
            Contains(
                code,
                "dec6502_n(&memory[$61], 2, &N, &Z);"),
            "live N/Z repeated DEC must preserve final flags");

    passed &=
        Expect(
            !Contains(
                code,
                "++memory[$60];"),
            "collapsed INC run must omit individual increments");

    passed &=
        Expect(
            Contains(
                code,
                "inc16_6502(&memory16[$70], &N, &Z);"),
            "conditional high-byte INC must become wide increment");

    passed &=
        Expect(
            !Contains(
                code,
                "if (memory[$70] == 0)"),
            "lowered wide increment must remove synthetic IF");

    passed &=
        Expect(
            !Contains(
                code,
                "++memory[$70]"),
            "lowered wide increment must remove byte operations");

    passed &=
        Expect(
            Contains(
                code,
                "dec16_6502(&memory16[$80], &A, &N, &Z);"),
            "borrow-style decrement must become wide decrement");

    passed &=
        Expect(
            !Contains(
                code,
                "if (memory[$80] == 0)"),
            "lowered wide decrement must remove synthetic IF");

    passed &=
        Expect(
            !Contains(
                code,
                "--memory[$80]"),
            "lowered wide decrement must remove byte operations");

    passed &=
        Expect(
            Contains(
                code,
                "Y = --memory[$90];"),
            "DEC followed by matching LDY must fuse");

    passed &=
        Expect(
            !Contains(
                code,
                "Y = memory[$90];"),
            "fused DEC/LDY must omit duplicate load");

    passed &=
        Expect(
            Contains(
                code,
                "memory[$91] = X & $7F;"),
            "TXA/AND/STA must become one memory expression");

    passed &=
        Expect(
            Contains(
                code,
                "memory[$92] = X;"),
            "inter-block A equals X state must simplify STA");

    passed &=
        Expect(
            Contains(
                code,
                "compare(X, $7F);"),
            "inter-block A equals X state must simplify CMP");

    passed &=
        Expect(
            Contains(
                code,
                "A = X;\n    compare(X, $7F);"),
            "TXA with dead N/Z must remain a compact assignment");

    passed &=
        Expect(
            Contains(
                code,
                "A = X | $80;\n    push(A);"),
            "standalone inter-block ORA must expose its source "
            "and invalidate it afterwards");

    passed &=
        Expect(
            Contains(
                code,
                "adc6502(Y, $01, 0, D, "
                "&A, &C, &V, &N, &Z);\n    push(A);"),
            "inter-block TYA source must feed ADC and be "
            "invalidated by its result");

    passed &=
        Expect(
            Contains(
                code,
                "A = rol6502_value(X, 0);\n"
                "    push(A);"),
            "inter-block TXA source must feed accumulator ROL "
            "and omit dead output flags");

    passed &=
        Expect(
            Contains(
                code,
                "asl6502(Y, &A, &C, &N, &Z);\n"
                "    push(A);"),
            "inter-block TYA source must feed accumulator ASL "
            "and preserve its output flags");

    passed &=
        Expect(
            Contains(
                code,
                "Y = X;\n"
                "    transfer6502(Y, &A, &N, &Z);"),
            "TAY must use the propagated X source and dead "
            "self TAX must be removed");

    passed &=
        Expect(
            !Contains(
                code,
                "X = X;"),
            "dead self register transfer must not be emitted");

    passed &=
        Expect(
            Contains(
                code,
                "transfer6502(Y, &A, &N, &Z);\n"
                "    return;"),
            "TYA must preserve N/Z when they are live at exit");

    passed &=
        Expect(
            Contains(
                code,
                "++X;\n    memory[$93] = A;"),
            "writing X must invalidate the propagated A source");

    passed &=
        Expect(
            !Contains(
                code,
                "A &= $7F;"),
            "fused TXA/AND/STA must omit the accumulator operation");

    passed &=
        Expect(
            Contains(
                code,
                "A = memory[$A0] | memory[$A1];"),
            "LDA/ORA must become one accumulator expression");

    passed &=
        Expect(
            Contains(
                code,
                "if (A == 0)"),
            "structured condition must use the fused A result");

    passed &=
        Expect(
            !Contains(
                code,
                "A |= memory[$A1];"),
            "fused ORA must not be emitted twice");

    passed &=
        Expect(
            Contains(
                code,
                "A = $42;\n    memory[$2000] = $42;"),
            "live accumulator definition must remain while its "
            "store uses the propagated constant");

    passed &=
        Expect(
            !Contains(
                code,
                "INX"),
            "semantic producer must not be duplicated");

    passed &=
        Expect(
            CountOccurrences(
                code,
                "BNE") == 3,
            "only the three intentional fallback BNEs must remain");

    atari::Disassembler disassembler;

    atari::StructuredStatementFormatter formatter;

    passed &=
        Expect(
            formatter.FormatAccumulatorBitwise(
                disassembler.Decode(
                    memory,
                    0x1B00),
                disassembler.Decode(
                    memory,
                    0x1B01)) ==
                "A = Y ^ $FF",
            "TYA must be accepted as a bitwise source");

    passed &=
        Expect(
            formatter.FormatAccumulatorBitwise(
                disassembler.Decode(
                    memory,
                    0x1B03),
                disassembler.Decode(
                    memory,
                    0x1B04)) ==
                "A = pop() & $0F",
            "PLA must remain a single-evaluation bitwise source");

    passed &=
        Expect(
            formatter.FormatStackPull(
                disassembler.Decode(
                    memory,
                    0x1B03),
                false) ==
                "A = pop()",
            "PLA with dead N/Z must remain compact");

    passed &=
        Expect(
            formatter.FormatStackPull(
                disassembler.Decode(
                    memory,
                    0x1B03),
                true) ==
                "pull6502(&A, &N, &Z)",
            "PLA must preserve live N/Z");

    passed &=
        Expect(
            formatter.FormatAccumulatorConsumer(
                disassembler.Decode(
                    memory,
                    0x1415),
                "Y") ==
                "push(Y)",
            "PHA must accept a propagated accumulator source");

    passed &=
        Expect(
            formatter.FormatAccumulatorConsumer(
                disassembler.Decode(
                    memory,
                    0x1B06),
                "X") ==
                "test_bits(X, memory[$20])",
            "BIT must accept a propagated accumulator source");

    passed &=
        Expect(
            formatter.FormatAccumulatorConsumer(
                disassembler.Decode(
                    memory,
                    0x1905),
                "X",
                true,
                false,
                true) ==
                "compare6502(X, $7F, &C, &N, &Z)",
            "CMP must expose live C/N/Z outputs");

    passed &=
        Expect(
            formatter.FormatAccumulatorConsumer(
                disassembler.Decode(
                    memory,
                    0x1B06),
                "Y",
                false,
                true,
                true) ==
                "test_bits6502(Y, memory[$20], "
                "&V, &N, &Z)",
            "BIT must expose live V/N/Z outputs");

    passed &=
        Expect(
            formatter.FormatAccumulatorConsumer(
                disassembler.Decode(
                    memory,
                    0x1B0E),
                "A",
                true,
                false,
                true) ==
                "compare6502(X, $10, &C, &N, &Z)",
            "CPX must use X and expose live flags");

    passed &=
        Expect(
            formatter.FormatAccumulatorConsumer(
                disassembler.Decode(
                    memory,
                    0x1B10),
                "A") ==
                "compare(Y, $11)",
            "CPY with dead flags must remain compact");

    passed &=
        Expect(
            formatter.FormatCarryOperation(
                disassembler.Decode(
                    memory,
                    0x121A),
                "1",
                true,
                true,
                true,
                false,
                "X") ==
                "sbc6502(X, $02, 1, D, "
                "&A, &C, &V, &N, &Z)",
            "SBC must accept a propagated accumulator source");

    passed &=
        Expect(
            formatter.FormatCarryOperation(
                disassembler.Decode(
                    memory,
                    0x1B08),
                "C",
                true,
                false,
                true,
                false,
                "X") ==
                "lsr6502(memory[$21], &memory[$21], "
                "&C, &N, &Z)",
            "memory LSR must preserve its own source and "
                "destination");

    passed &=
        Expect(
            formatter.FormatSourceRegisterTransfer(
                disassembler.Decode(
                    memory,
                    0x1B0A),
                true) ==
                "transfer6502(SP, &X, &N, &Z)",
            "TSX must preserve live N/Z");

    passed &=
        Expect(
            formatter.FormatLoad(
                disassembler.Decode(
                    memory,
                    0x1100),
                true) ==
                "load6502(memory[$1234 + X], "
                "&A, &N, &Z)",
            "LDA must preserve live N/Z");

    passed &=
        Expect(
            formatter.FormatIncrementDecrement(
                disassembler.Decode(
                    memory,
                    0x1B0B),
                true) ==
                "inc6502_n(&X, 1, &N, &Z)",
            "INX must preserve live N/Z");

    passed &=
        Expect(
            formatter.FormatIncrementDecrement(
                disassembler.Decode(
                    memory,
                    0x1B0C),
                false) ==
                "--memory[$22]",
            "DEC with dead N/Z must remain compact");

    passed &=
        Expect(
            formatter.FormatIncrementDecrement(
                disassembler.Decode(
                    memory,
                    0x1B0C),
                true) ==
                "dec6502_n(&memory[$22], 1, &N, &Z)",
            "DEC must preserve live N/Z");

    passed &=
        Expect(
            formatter.FormatAccumulatorRegisterTransfer(
                disassembler.Decode(
                    memory,
                    0x1923),
                "X",
                true) ==
                "transfer6502(X, &X, &N, &Z)",
            "self TAX must remain when its N/Z are live");

    std::size_t officialOpcodeCount = 0;
    bool allOfficialOpcodesFormatted = true;

    for (unsigned int opcode = 0;
         opcode <= 0xFF;
         ++opcode)
    {
        memory.Write8(
            0x1D00,
            static_cast<atari::u8>(
                opcode));

        memory.Write8(0x1D01, 0x34);
        memory.Write8(0x1D02, 0x12);

        const auto decodedOpcode =
            disassembler.Decode(
                memory,
                0x1D00);

        if (decodedOpcode.instruction ==
            atari::cpu6502::Instruction::Illegal)
        {
            continue;
        }

        ++officialOpcodeCount;

        allOfficialOpcodesFormatted =
            allOfficialOpcodesFormatted &&
            !formatter.Format(
                 metadata,
                 decodedOpcode).empty();
    }

    passed &=
        Expect(
            officialOpcodeCount == 151,
            "all 151 official 6502 opcodes must decode");

    passed &=
        Expect(
            allOfficialOpcodesFormatted,
            "every official opcode must have formatter output");

    memory.Write8(0x1D00, 0x00);

    passed &=
        Expect(
            formatter.Format(
                metadata,
                disassembler.Decode(
                    memory,
                    0x1D00)) ==
                "brk6502($1D00); return /* BRK */",
            "BRK must lower to explicit interrupt runtime semantics");

    memory.Write8(0x1D00, 0x40);

    passed &=
        Expect(
            formatter.Format(
                metadata,
                disassembler.Decode(
                    memory,
                    0x1D00)) ==
                "rti6502(); return /* RTI */",
            "RTI must lower to explicit interrupt return semantics");

    passed &=
        Expect(
            formatter.FormatAccumulatorBitwiseTransfer(
                disassembler.Decode(
                    memory,
                    0x190D),
                disassembler.Decode(
                    memory,
                    0x190F)) ==
                "memory[$91] = A & $7F",
            "bitwise accumulator result must transfer directly "
            "when its value and flags are dead");

    auto nonConsecutiveHighLoad =
        disassembler.Decode(
            memory,
            0x150A);

    auto mismatchedLowDecrement =
        disassembler.Decode(
            memory,
            0x1806);

    mismatchedLowDecrement.bytes[1] =
        0x82;

    passed &=
        Expect(
            formatter.FormatWideDecrement(
                disassembler.Decode(
                    memory,
                    0x1800),
                disassembler.Decode(
                    memory,
                    0x1804),
                mismatchedLowDecrement).empty(),
            "mismatched tested and decremented low byte "
            "must reject wide collapse");

    nonConsecutiveHighLoad.bytes[1] =
        0x02;

    passed &=
        Expect(
            formatter.FormatWideArithmeticTransfer(
                disassembler.Decode(
                    memory,
                    0x1501),
                disassembler.Decode(
                    memory,
                    0x1504),
                disassembler.Decode(
                    memory,
                    0x1507),
                nonConsecutiveHighLoad,
                disassembler.Decode(
                    memory,
                    0x150D),
                disassembler.Decode(
                    memory,
                    0x1510),
                "1").empty(),
            "non-consecutive high byte must reject wide collapse");

    passed &=
        Expect(
            formatter.Format(
                metadata,
                disassembler.Decode(
                    memory,
                    0x1100)) ==
                "A = memory[$1234 + X]",
            "absolute indexed load must preserve X addressing");

    passed &=
        Expect(
            formatter.Format(
                metadata,
                disassembler.Decode(
                    memory,
                    0x1103)) ==
                "memory[byte($40 + X)] = A",
            "zero-page indexed store must preserve wrapping");

    passed &=
        Expect(
            formatter.Format(
                metadata,
                disassembler.Decode(
                    memory,
                    0x1105)) ==
                "adc6502(A, $01, C, D, &A, &C, &V, &N, &Z)",
            "standalone ADC must preserve input and output flags");

    memory.Write8(0xFFFE, 0xEA);
    memory.Write8(0xFFFF, 0xEA);

    atari::Listing listing;

    const auto endOfMemoryListing =
        listing.Build(
            memory,
            0xFFFE,
            0xFFFF);

    passed &=
        Expect(
            endOfMemoryListing.size() == 2 &&
            endOfMemoryListing[0].address == 0xFFFE &&
            endOfMemoryListing[1].address == 0xFFFF,
            "listing ending at $FFFF must terminate without wrap");

    passed &=
        Expect(
            listing.Build(
                memory,
                0x2000,
                0x1FFF).empty(),
            "reversed listing range must be empty");

    const std::filesystem::path truncatedXexPath =
        std::filesystem::temp_directory_path() /
        "atari_studio_truncated_segment_test.xex";

    {
        const unsigned char truncatedXex[] =
            {
                0xFF,
                0xFF,
                0x00,
                0x20,
                0x01,
                0x20,
                0xAA};

        std::ofstream truncatedXexStream(
            truncatedXexPath,
            std::ios::binary |
                std::ios::trunc);

        truncatedXexStream.write(
            reinterpret_cast<const char*>(
                truncatedXex),
            sizeof(truncatedXex));
    }

    project.GetMemory().Write8(
        0x1234,
        0x55);

    project.AddSegment(
        atari::Segment{
            0x1234,
            0x1234});

    atari::XexLoader truncatedLoader;

    passed &=
        Expect(
            !truncatedLoader.Load(
                truncatedXexPath,
                project),
            "truncated XEX segment must fail");

    passed &=
        Expect(
            project.Segments().empty() &&
            !project.GetMemory().
                Cell(0x1234).initialized &&
            !project.GetMemory().
                Cell(0x2000).initialized,
            "failed XEX load must leave Project empty");

    std::error_code removeError;

    std::filesystem::remove(
        truncatedXexPath,
        removeError);

    const std::filesystem::path emptyXexPath =
        std::filesystem::temp_directory_path() /
        "atari_studio_empty_test.xex";

    {
        std::ofstream emptyXexStream(
            emptyXexPath,
            std::ios::binary |
                std::ios::trunc);
    }

    project.GetMemory().Write8(
        0x3456,
        0xAA);

    atari::XexLoader emptyLoader;

    passed &=
        Expect(
            !emptyLoader.Load(
                emptyXexPath,
                project),
            "empty XEX file must fail");

    passed &=
        Expect(
            project.Segments().empty() &&
            !project.GetMemory().
                Cell(0x3456).initialized,
            "empty XEX failure must leave Project empty");

    std::filesystem::remove(
        emptyXexPath,
        removeError);

    if (!passed)
    {
        std::cerr
            << "\nGenerated code:\n"
            << code;

        return 1;
    }

    return 0;
}
