//
// SPDX-License-Identifier: Apache-2.0
//
// Checks PICOGK_IMPLICIT_SERIAL parsing. Built into APITests, and also
// runnable on its own:
//   c++ -std=c++20 -DPICOGK_IMPLICIT_SERIAL_TEST_MAIN APITests/ImplicitSerialParse.cpp
//

#include "../Source/PicoGKImplicitSerial.h"

#include <cstdio>
#include <cstring>
#include <iostream>
#include <string>

#if defined(_WIN32)
#include <io.h>
#else
#include <unistd.h>
#endif

namespace
{

void SetImplicitSerialEnv(const char* psz)
{
#if defined(_WIN32)
    if (psz == nullptr)
        _putenv("PICOGK_IMPLICIT_SERIAL=");
    else
        _putenv_s("PICOGK_IMPLICIT_SERIAL", psz);
#else
    if (psz == nullptr)
        unsetenv("PICOGK_IMPLICIT_SERIAL");
    else
        setenv("PICOGK_IMPLICIT_SERIAL", psz, 1);
#endif
}

class StderrCapture
{
public:
    StderrCapture()
    {
        std::fflush(stderr);
#if defined(_WIN32)
        m_iSaved = _dup(_fileno(stderr));
        m_pFile = std::tmpfile();
        if (m_iSaved >= 0 && m_pFile != nullptr)
            _dup2(_fileno(m_pFile), _fileno(stderr));
#else
        m_iSaved = dup(fileno(stderr));
        m_pFile = std::tmpfile();
        if (m_iSaved >= 0 && m_pFile != nullptr)
            dup2(fileno(m_pFile), fileno(stderr));
#endif
    }

    ~StderrCapture()
    {
        Restore();
        if (m_pFile != nullptr)
            std::fclose(m_pFile);
    }

    bool bOk() const
    {
        return m_iSaved >= 0 && m_pFile != nullptr;
    }

    std::string strTaken()
    {
        std::fflush(stderr);
        Restore();
        std::rewind(m_pFile);
        std::string str;
        char ac[256];
        while (std::fgets(ac, static_cast<int>(sizeof(ac)), m_pFile) != nullptr)
            str += ac;
        return str;
    }

private:
    void Restore()
    {
        if (m_iSaved < 0)
            return;
        std::fflush(stderr);
#if defined(_WIN32)
        _dup2(m_iSaved, _fileno(stderr));
        _close(m_iSaved);
#else
        dup2(m_iSaved, fileno(stderr));
        close(m_iSaved);
#endif
        m_iSaved = -1;
    }

    std::FILE* m_pFile = nullptr;
    int m_iSaved = -1;
};

struct ParseCase
{
    const char* psz;
    PicoGK::EImplicitSerial eExpected;
};

const ParseCase aParseCases[] =
{
    {nullptr, PicoGK::EImplicitSerial::Parallel},
    {"", PicoGK::EImplicitSerial::Parallel},
    {"   ", PicoGK::EImplicitSerial::Parallel},
    {"\t\r\n", PicoGK::EImplicitSerial::Parallel},
    {"0", PicoGK::EImplicitSerial::Parallel},
    {"false", PicoGK::EImplicitSerial::Parallel},
    {"FALSE", PicoGK::EImplicitSerial::Parallel},
    {"no", PicoGK::EImplicitSerial::Parallel},
    {"No", PicoGK::EImplicitSerial::Parallel},
    {"off", PicoGK::EImplicitSerial::Parallel},
    {"OFF", PicoGK::EImplicitSerial::Parallel},
    {"  off  ", PicoGK::EImplicitSerial::Parallel},
    {"\tNo\n", PicoGK::EImplicitSerial::Parallel},
    {"1", PicoGK::EImplicitSerial::Serial},
    {"true", PicoGK::EImplicitSerial::Serial},
    {"TRUE", PicoGK::EImplicitSerial::Serial},
    {"yes", PicoGK::EImplicitSerial::Serial},
    {"YeS", PicoGK::EImplicitSerial::Serial},
    {"on", PicoGK::EImplicitSerial::Serial},
    {"On", PicoGK::EImplicitSerial::Serial},
    {"  yes  ", PicoGK::EImplicitSerial::Serial},
    {"1 ", PicoGK::EImplicitSerial::Serial},
    {"\tTRUE\r\n", PicoGK::EImplicitSerial::Serial},
    {"2", PicoGK::EImplicitSerial::Unrecognized},
    {"10", PicoGK::EImplicitSerial::Unrecognized},
    {"truee", PicoGK::EImplicitSerial::Unrecognized},
    {"yes please", PicoGK::EImplicitSerial::Unrecognized},
    {"enable", PicoGK::EImplicitSerial::Unrecognized},
    {"maybe", PicoGK::EImplicitSerial::Unrecognized},
};

const char* pszMode(PicoGK::EImplicitSerial eMode)
{
    switch (eMode)
    {
    case PicoGK::EImplicitSerial::Parallel:
        return "parallel";
    case PicoGK::EImplicitSerial::Serial:
        return "serial";
    case PicoGK::EImplicitSerial::Unrecognized:
        return "unrecognized";
    default:
        {
            const PicoGK::EImplicitSerial eUnhandled = eMode;
            static_cast<void>(eUnhandled);
            return "unhandled";
        }
    }
}

} // namespace

bool bRunImplicitSerialParseTests()
{
    const char* pszSaved = std::getenv("PICOGK_IMPLICIT_SERIAL");
    const bool bWasSet = pszSaved != nullptr;
    const std::string strSaved = bWasSet ? std::string(pszSaved) : std::string();
    int nFailed = 0;

    for (const ParseCase& oCase : aParseCases)
    {
        const PicoGK::EImplicitSerial eGot = PicoGK::eParseImplicitSerial(oCase.psz);
        if (eGot != oCase.eExpected)
        {
            std::cerr << "parse [" << (oCase.psz != nullptr ? oCase.psz : "<unset>")
                      << "] got " << pszMode(eGot)
                      << ", expected " << pszMode(oCase.eExpected) << "\n";
            ++nFailed;
        }
    }

    SetImplicitSerialEnv(nullptr);
    if (PicoGK::bImplicitSerialRequested())
    {
        std::cerr << "unset PICOGK_IMPLICIT_SERIAL selected serial\n";
        ++nFailed;
    }

    SetImplicitSerialEnv("");
    if (PicoGK::bImplicitSerialRequested())
    {
        std::cerr << "empty PICOGK_IMPLICIT_SERIAL selected serial\n";
        ++nFailed;
    }

    SetImplicitSerialEnv("  YeS  ");
    if (!PicoGK::bImplicitSerialRequested())
    {
        std::cerr << "\"  YeS  \" selected parallel\n";
        ++nFailed;
    }

    SetImplicitSerialEnv("  off  ");
    if (PicoGK::bImplicitSerialRequested())
    {
        std::cerr << "\"  off  \" selected serial\n";
        ++nFailed;
    }

    bool bTrueIsSerial = false;
    std::string strTrueWarn;
    {
        StderrCapture oCapture;
        if (!oCapture.bOk())
        {
            std::cerr << "could not capture stderr\n";
            ++nFailed;
        }
        else
        {
            SetImplicitSerialEnv("true");
            bTrueIsSerial = PicoGK::bImplicitSerialRequested();
            strTrueWarn = oCapture.strTaken();
        }
    }
    if (!bTrueIsSerial)
    {
        std::cerr << "\"true\" selected parallel\n";
        ++nFailed;
    }
    if (!strTrueWarn.empty())
    {
        std::cerr << "recognized value wrote a warning: " << strTrueWarn;
        ++nFailed;
    }

    bool bMaybeIsSerial = false;
    bool bTwoIsSerial = false;
    std::string strUnrecognizedWarn;
    {
        StderrCapture oCapture;
        if (!oCapture.bOk())
        {
            std::cerr << "could not capture stderr\n";
            ++nFailed;
        }
        else
        {
            SetImplicitSerialEnv("maybe");
            bMaybeIsSerial = PicoGK::bImplicitSerialRequested();
            SetImplicitSerialEnv("2");
            bTwoIsSerial = PicoGK::bImplicitSerialRequested();
            strUnrecognizedWarn = oCapture.strTaken();
        }
    }
    if (!bMaybeIsSerial)
    {
        std::cerr << "unrecognized \"maybe\" selected parallel\n";
        ++nFailed;
    }
    if (!bTwoIsSerial)
    {
        std::cerr << "unrecognized \"2\" selected parallel\n";
        ++nFailed;
    }
    const bool bWarnedOnce = strUnrecognizedWarn.find("=\"maybe\"") != std::string::npos
        && strUnrecognizedWarn.find("=\"2\"") == std::string::npos
        && strUnrecognizedWarn.find('\n') != std::string::npos
        && strUnrecognizedWarn.find('\n') == strUnrecognizedWarn.size() - 1;
    if (!bWarnedOnce)
    {
        std::cerr << "expected one warning for \"maybe\" and none for \"2\", got:\n" << strUnrecognizedWarn;
        ++nFailed;
    }
    else
    {
        std::cout << "one-time unrecognized warning: " << strUnrecognizedWarn;
    }

    if (bWasSet)
        SetImplicitSerialEnv(strSaved.c_str());
    else
        SetImplicitSerialEnv(nullptr);

    if (nFailed != 0)
    {
        std::cerr << "implicit serial parse checks failed: " << nFailed << "\n";
        return false;
    }

    std::cout << "implicit serial parse checks passed\n";
    return true;
}

#ifdef PICOGK_IMPLICIT_SERIAL_TEST_MAIN
int main()
{
    return bRunImplicitSerialParseTests() ? 0 : 1;
}
#endif
