//
// SPDX-License-Identifier: Apache-2.0
//
// PicoGK ("peacock") is a compact software kernel for computational geometry,
// specifically for use in Computational Engineering Models (CEM).
//
// For more information, please visit https://picogk.org
//
// PicoGK is developed and maintained by LEAP 71 - © 2023-2026 by LEAP 71
// https://leap71.com
//
// Computational Engineering will profoundly change our physical world in the
// years ahead. Thank you for being part of the journey.
//
// We have developed this library to be used widely, for both commercial and
// non-commercial projects alike. Therefore, have released it under a permissive
// open-source license.
//
// The foundation of PicoGK is a thin layer on top of the powerful open-source
// OpenVDB project, which in turn uses many other Free and Open Source Software
// libraries. We are grateful to be able to stand on the shoulders of giants.
//
// LEAP 71 licenses this file to you under the Apache License, Version 2.0
// (the "License"); you may not use this file except in compliance with the
// License. You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, THE SOFTWARE IS
// PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED.
//
// See the License for the specific language governing permissions and
// limitations under the License.
//

#ifndef PICOGKIMPLICITSERIAL_H_
#define PICOGKIMPLICITSERIAL_H_

#include <atomic>
#include <cstdio>
#include <cstdlib>

namespace PicoGK
{

// How PICOGK_IMPLICIT_SERIAL selects the RenderImplicit loop.
// The string is trimmed of ASCII whitespace (space, tab, CR, LF, VT, FF)
// and compared case-insensitively.
//   Serial:    1, true, yes, on
//   Parallel:  unset, empty, 0, false, no, off
//   Anything else is unrecognized. The caller takes the serial path and
//   warns once on stderr.
enum class EImplicitSerial
{
    Parallel,
    Serial,
    Unrecognized
};

inline bool bImplicitSerialIsAsciiSpace(unsigned char uc)
{
    return uc == ' ' || uc == '\t' || uc == '\n' || uc == '\r' || uc == '\v' || uc == '\f';
}

inline bool bImplicitSerialTokenEquals( const char* pszBegin,
                                        const char* pszEnd,
                                        const char* pszLiteral)
{
    const char* psz = pszBegin;
    while (psz != pszEnd && *pszLiteral != '\0')
    {
        const unsigned char uc = static_cast<unsigned char>(*psz);
        const char cFolded = (uc >= 'A' && uc <= 'Z')
            ? static_cast<char>(uc - 'A' + 'a')
            : static_cast<char>(uc);
        if (cFolded != *pszLiteral)
            return false;
        ++psz;
        ++pszLiteral;
    }
    return psz == pszEnd && *pszLiteral == '\0';
}

inline EImplicitSerial eParseImplicitSerial(const char* psz)
{
    if (psz == nullptr)
        return EImplicitSerial::Parallel;

    while (*psz != '\0' && bImplicitSerialIsAsciiSpace(static_cast<unsigned char>(*psz)))
        ++psz;

    const char* pszEnd = psz;
    while (*pszEnd != '\0')
        ++pszEnd;
    while (pszEnd > psz && bImplicitSerialIsAsciiSpace(static_cast<unsigned char>(pszEnd[-1])))
        --pszEnd;

    if (psz == pszEnd)
        return EImplicitSerial::Parallel;

    if (    bImplicitSerialTokenEquals(psz, pszEnd, "1")
        ||  bImplicitSerialTokenEquals(psz, pszEnd, "true")
        ||  bImplicitSerialTokenEquals(psz, pszEnd, "yes")
        ||  bImplicitSerialTokenEquals(psz, pszEnd, "on"))
        return EImplicitSerial::Serial;

    if (    bImplicitSerialTokenEquals(psz, pszEnd, "0")
        ||  bImplicitSerialTokenEquals(psz, pszEnd, "false")
        ||  bImplicitSerialTokenEquals(psz, pszEnd, "no")
        ||  bImplicitSerialTokenEquals(psz, pszEnd, "off"))
        return EImplicitSerial::Parallel;

    return EImplicitSerial::Unrecognized;
}

// True when RenderImplicit must stay on the calling thread.
// getenv("PICOGK_IMPLICIT_SERIAL") is read on every call; the mode is not cached.
// An unrecognized PICOGK_IMPLICIT_SERIAL value is serial, and the first
// such value in the process prints one warning to stderr.
inline bool bImplicitSerialRequested()
{
    const char* psz = std::getenv("PICOGK_IMPLICIT_SERIAL");
    const EImplicitSerial eMode = eParseImplicitSerial(psz);
    switch (eMode)
    {
    case EImplicitSerial::Parallel:
        return false;
    case EImplicitSerial::Serial:
        return true;
    case EImplicitSerial::Unrecognized:
        {
            static std::atomic<bool> bWarned{false};
            if (!bWarned.exchange(true))
            {
                std::fprintf(stderr,
                    "PICOGK_IMPLICIT_SERIAL=\"%s\" is not recognized; RenderImplicit is using the serial path. "
                    "Serial values: 1, true, yes, on. Parallel values: unset, empty, 0, false, no, off.\n",
                    psz != nullptr ? psz : "");
            }
            return true;
        }
    default:
        {
            // A new enumerator has to be handled above. Stay on the serial path.
            const EImplicitSerial eUnhandled = eMode;
            static_cast<void>(eUnhandled);
            return true;
        }
    }
}

} // namespace PicoGK

#endif // PICOGKIMPLICITSERIAL_H_
