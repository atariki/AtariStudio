#pragma once

#include <algorithm>
#include <array>
#include <string_view>

#include <AtariStudio/Core/Types.h>

namespace atari
{

struct AtariSymbol
{
    u16 address;
    std::string_view comment;
};

class AtariSymbols
{
public:

    [[nodiscard]]
    static std::string_view Find(u16 address) noexcept
    {
        const auto iterator =
            std::lower_bound(
                s_symbols.begin(),
                s_symbols.end(),
                address,
                [](const AtariSymbol& symbol, u16 value)
                {
                    return symbol.address < value;
                });

        if (iterator == s_symbols.end() ||
            iterator->address != address)
        {
            return {};
        }

        return iterator->comment;
    }

private:

    inline static constexpr std::array<AtariSymbol, 179> s_symbols =
    {{
        {0x0000, "LINZBS LINBUG STORAGE"},
        {0x0001, "NGFLAG"},
        {0x0002, "CASINI CASSETTE INIT LOC"},
        {0x0003, "CASINI+1"},
        {0x0004, "RAMLO RAM POINTER FOR MEM TEST"},
        {0x0005, "RAMLO+1"},
        {0x0006, "TRAMSZ TEMP LOC FOR RAM SIZE"},
        {0x0008, "WARMST WARM START FLAG"},
        {0x0009, "BOOTQ SUCCESSFUL BOOT FLAG"},
        {0x000A, "DOSVEC DOS START VECTOR"},
        {0x000B, "DOSVEC+1"},
        {0x000C, "DOSINI DOS INIT ADDRESS"},
        {0x000E, "APPMHI APPLICATION MEM HI LIMIT"},
        {0x000F, "APPMHI+1"},
        {0x0010, "POKMSK SYSTEM MASK FOR POKEY IRQ ENABLE"},
        {0x0011, "BRKKEY BREAK KEY FLAG"},
        {0x0012, "RTCLOK REAL TIME CLOCK (60HZ OR 16.66666 MS)"},
        {0x0014, "RTCLOK+2"},
        {0x0015, "BUFADR INDIRECT BUFFER ADDRESS REG"},
        {0x0016, "BUFADR+1"},
        {0x0018, "DSKFMS DISK FILE MANAGER POINTER"},
        {0x0019, "DSKFMS+1"},
        {0x001A, "DSKUTL DISK UTILITIES POINTER"},
        {0x001B, "DSKUTL+1"},
        {0x001D, "PBPNT PRINT BUFFER POINTER"},
        {0x001E, "PBUFSZ PRINT BUFFER SIZE"},
        {0x001F, "PTEMP TEMP REG"},
        {0x0020, "ICHIDZ HANDLER INDEX NUMBER ($FF := IOCB FREE)"},
        {0x0021, "ICDNOZ DEVICE NUMBER (DRIVE NUMBER)"},
        {0x0022, "ICCOMZ COMMAND CODE"},
        {0x0026, "ICPTLZ PUT BYTE ROUTINE ADDRESS - 1"},
        {0x0028, "ICBLLZ BUFFER LENGTH (LOW)"},
        {0x0029, "ICBLHZ BUFFER LENGTH (HIGH)"},
        {0x002A, "ICAX1Z AUX INFO"},
        {0x002B, "ICAX2Z"},
        {0x002C, "ICSPRZ SPARE BYTES (CIO LOCAL USE)"},
        {0x002D, "ICSPRZ+1"},
        {0x002E, "ICIDNO IOCB LUMBER * 16"},
        {0x0030, "STATUS INTERNAL STATUS STORAGE"},
        {0x0033, "BUFRHI POINTER TO DATA BUFFER (HI BYTE)"},
        {0x0034, "BFENLO NEXT BYTE PAST END OF BUFFER (LO BYTE)"},
        {0x0035, "BNENHI NEXT BYTE PAST END OF BUFFER (HI BYTE)"},
        {0x0038, "BUFRFL DATA BUFFER FULL FLAG"},
        {0x003A, "XMTDON XMIT DONE FLAG"},
        {0x003D, "BPTR BUFFER POINTER (CASSETTE)"},
        {0x003E, "FTYPE FILE TYPE (SHORT IRG/LONG IRG)"},
        {0x003F, "FEOF END OF FILE FLAG (CASSETTE)"},
        {0x0040, "FREQ FREQ COUNTER FOR CONSOLE SPEAKER"},
        {0x0041, "SOUNDR NOISY I/O FLAG. (ZERO IS QUIET)"},
        {0x0042, "CRITIC CRITICAL CODE IF NON-ZERO)"},
        {0x0044, "FMSZPG+1"},
        {0x0045, "FMSZPG+2"},
        {0x0046, "FMSZPG+3"},
        {0x0047, "FMSZPG+4"},
        {0x0048, "FMSZPG+5"},
        {0x0049, "FMSZPG+6"},
        {0x004A, "CKEY SET WHEN GAME START PRESSED"},
        {0x004B, "CASSBT CASSETTE BOOT FLAG"},
        {0x004D, "ATRACT ATTRACT MODE FLAG"},
        {0x004E, "DRKMSK DARK ATTRACT MASK"},
        {0x004F, "COLRSH ATTRACT COLOR SHIFTER (XORED WITH PLAYFIELD)"},
        {0x0050, "TMPCHR TEMP CHAR STORAGE (DISPLAY HANDLER)"},
        {0x0051, "HOLD1 TEMP STG (DISPLAY HANDLER)"},
        {0x0052, "LMARGN LEFT MARGIN"},
        {0x0053, "RMARGN RIGHT MARGIN"},
        {0x0054, "ROWCRS CURSOR COUNTERS"},
        {0x0055, "COLCRS"},
        {0x0056, "COLCRS+1"},
        {0x0058, "SAVMSC"},
        {0x005C, "OLDCOL+1"},
        {0x005D, "OLDCHR DATA UNDER CURSOR"},
        {0x0060, "NEWROW POINT DRAWS TO HERE"},
        {0x0061, "NEWCOL"},
        {0x0063, "LOGCOL POINTS AT COLUMN IN LOGICAL LINE"},
        {0x0064, "ADRESS INDIRECT POINTER"},
        {0x0067, "TOADR+1"},
        {0x0069, "SAVADR+1"},
        {0x006A, "RAMTOP RAM SIZE DEFINED BY POWER ON LOGIC"},
        {0x0071, "ROWAC+1"},
        {0x0072, "COLAC"},
        {0x0074, "ENDPT"},
        {0x0075, "ENDPT+1"},
        {0x0076, "DELTAR"},
        {0x0078, "DELTAC+1"},
        {0x0079, "ROWINC"},
        {0x007A, "COLINC"},
        {0x007E, "COUNTR DRAW COUNTER"},
        {0x007F, "COUNTR+1"},
        {0x0080, "LOMEM BASIC POINTER TO LOW MEMORY"},
        {0x0081, "LOMEM+1"},
        {0x0082, "VNTP BASIC VARIABLE NAME TABLE"},
        {0x0083, "VNTP+1"},
        {0x0084, "VNTD BASIC VARIABLE NAME TABLE END"},
        {0x0085, "VNTD+1"},
        {0x0087, "VVTP+1"},
        {0x0088, "STMTAB BASIC STATEMENT TABLE"},
        {0x0089, "STMTAB+1"},
        {0x008A, "STMCUR BASIC CURRENT STATEMENT POINTER"},
        {0x008B, "STMCUR+1"},
        {0x008C, "STARP BASIC STRING AND ARRAY POINTER"},
        {0x008E, "RUNSTK BASIC RUNTIME STACK"},
        {0x0090, "MEMTOP BASIC TOP OF MEMORY"},
        {0x0091, "MEMTOP+1"},
        {0x0092, "MEOLFLG"},
        {0x0095, "POKADR"},
        {0x0096, "POKADR+1"},
        {0x0098, "SVESA+1"},
        {0x009A, "MVFA+1"},
        {0x009C, "MVTA+1"},
        {0x009D, "CPC"},
        {0x009E, "CPC+1"},
        {0x00A0, "TSLNUM"},
        {0x00A1, "TSLNUM+1"},
        {0x00A3, "MVLNG+1"},
        {0x00A4, "ECSIZE"},
        {0x00A5, "ECSIZE+1"},
        {0x00A6, "DIRFLG"},
        {0x00A7, "STMLBD"},
        {0x00A9, "OPSTKX"},
        {0x00AA, "ARSTKX"},
        {0x00AB, "EXSVOP"},
        {0x00AD, "LELNUM"},
        {0x00AE, "LELNUM+1"},
        {0x00B0, "COMCNT"},
        {0x00B5, "LISTDTD"},
        {0x00B7, "DATALN"},
        {0x00BA, "STOPLN"},
        {0x00BC, "TRAPLN"},
        {0x00C0, "IOCMD"},
        {0x00C1, "IODVC"},
        {0x00C2, "PROMPT"},
        {0x00C3, "ERRSAV"},
        {0x00C4, "TEMPA"},
        {0x00C5, "TEMPA+1"},
        {0x00C7, "ZTEMP2+1"},
        {0x00CA, "LOADFLG"},
        {0x00D3, "VNUM"},
        {0x00D8, "FR0+4"},
        {0x00DA, "FRE"},
        {0x00DB, "FRE+1"},
        {0x00DE, "FRE+4"},
        {0x00E0, "FR1"},
        {0x00E1, "FR1+1"},
        {0x00E2, "FR1+2"},
        {0x00E3, "FR1+3"},
        {0x00E6, "FR2"},
        {0x00E8, "FR2+2"},
        {0x00EC, "FRX"},
        {0x00ED, "EEXP"},
        {0x00EF, "ESIGN"},
        {0x00F1, "DIGRT"},
        {0x00F2, "CIX"},
        {0x00F4, "INBUFF+1"},
        {0x00F8, "ZTEMP4+1"},
        {0x00FA, "ZTEMP3+1"},
        {0x00FC, "FLPTR"},
        {0x00FD, "FLPTR+1"},
        {0x00FE, "FPTR2"},
        {0x00FF, "FPTR2+1"},

        {0x0206, "VBREAK \"BRK\" VECTOR"},
        {0x022F, "SDMCTL SAVE DMACTL REGISTER"},
        {0x0230, "SDLSTL DISPLAY LIST ADDRESS LOW"},
        {0x0231, "SDLSTH DISPLAY LIST ADDRESS HIGH"},
        {0x02F4, "CHBAS CHARACTER SET BASE SHADOW"},
        {0x02F8, "ROWINC"},
        {0x0306, "DTIMLO DEVICE TIME OUT IN 1 SEC. UNITS"},
        {0x0308, "DBYTLO BYTE COUNT"},
        {0x034C, "ICSPR 4 SPARE BYTES"},
        {0x0388, "B4-ICBLL"},
        {0x03A0, "IOCB6 I/O CONTROL BLOCK 6"},
        {0x03B8, "B7-ICBLL"},

        {0xD01A, "COLBK"},
        {0xD301, "PORTB"},
        {0xD402, "DLISTL DISPLAY LIST ADDRESS LOW"},
        {0xD403, "DLISTH DISPLAY LIST ADDRESS HIGH"},
        {0xD409, "CHBASE CHARACTER SET BASE"},
        {0xD40E, "NMIEN"},
        {0xD40F, "NMIST"},
        {0xD800, "AFP"}
    }};
};

} // namespace atari
