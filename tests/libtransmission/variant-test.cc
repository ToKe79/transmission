/*
 * This file Copyright (C) 2013-2014 Mnemosyne LLC
 *
 * It may be used under the GNU GPL versions 2 or 3
 * or any future license endorsed by Mnemosyne LLC.
 *
 */

#define LIBTRANSMISSION_VARIANT_MODULE

#include "transmission.h"
#include "utils.h" /* tr_free */
#include "variant-common.h"
#include "variant.h"

#include <algorithm>
#include <array>
#include <cmath> // lrint()
#include <cctype> // isspace()
#include <string>
#include <string_view>

#include "gtest/gtest.h"

using namespace std::literals;

class VariantTest : public ::testing::Test
{
protected:
    std::string stripWhitespace(std::string const& in)
    {
        auto s = in;
        s.erase(s.begin(), std::find_if_not(s.begin(), s.end(), ::isspace));
        s.erase(std::find_if_not(s.rbegin(), s.rend(), ::isspace).base(), s.end());
        return s;
    }

    auto bencParseInt(std::string const& in, uint8_t const** end, int64_t* val)
    {
        return tr_bencParseInt(in.data(), in.data() + in.size(), end, val);
    }
};

#ifndef _WIN32
#define STACK_SMASH_DEPTH (1 * 1000 * 1000)
#else
#define STACK_SMASH_DEPTH (100 * 1000)
#endif

TEST_F(VariantTest, getType)
{
    auto i = int64_t{};
    auto b = bool{};
    auto d = double{};
    auto sv = std::string_view{};
    auto v = tr_variant{};

    tr_variantInitInt(&v, 30);
    EXPECT_TRUE(tr_variantGetInt(&v, &i));
    EXPECT_EQ(30, i);
    EXPECT_TRUE(tr_variantGetReal(&v, &d));
    EXPECT_EQ(30, int(d));
    EXPECT_FALSE(tr_variantGetBool(&v, &b));
    EXPECT_FALSE(tr_variantGetStrView(&v, &sv));

    auto strkey = "foo"sv;
    tr_variantInitStr(&v, strkey);
    EXPECT_FALSE(tr_variantGetBool(&v, &b));
    EXPECT_TRUE(tr_variantGetStrView(&v, &sv));
    EXPECT_EQ(strkey, sv);
    EXPECT_NE(std::data(strkey), std::data(sv));

    strkey = "anything"sv;
    tr_variantInitStrView(&v, strkey);
    EXPECT_TRUE(tr_variantGetStrView(&v, &sv));
    EXPECT_EQ(strkey, sv);
    EXPECT_EQ(std::data(strkey), std::data(sv)); // literally the same memory
    EXPECT_EQ(std::size(strkey), std::size(sv));

    strkey = "true"sv;
    tr_variantInitStr(&v, strkey);
    EXPECT_TRUE(tr_variantGetBool(&v, &b));
    EXPECT_TRUE(b);
    EXPECT_TRUE(tr_variantGetStrView(&v, &sv));
    EXPECT_EQ(strkey, sv);

    strkey = "false"sv;
    tr_variantInitStr(&v, strkey);
    EXPECT_TRUE(tr_variantGetBool(&v, &b));
    EXPECT_FALSE(b);
    EXPECT_TRUE(tr_variantGetStrView(&v, &sv));
    EXPECT_EQ(strkey, sv);
}

TEST_F(VariantTest, parseInt)
{
    auto const in = std::string{ "i64e" };
    auto constexpr InitVal = int64_t{ 888 };
    auto constexpr ExpectVal = int64_t{ 64 };

    uint8_t const* end = {};
    auto val = int64_t{ InitVal };
    auto const err = bencParseInt(in, &end, &val);
    EXPECT_EQ(0, err);
    EXPECT_EQ(ExpectVal, val);
    EXPECT_EQ(reinterpret_cast<decltype(end)>(in.data() + in.size()), end);
}

TEST_F(VariantTest, parseIntWithMissingEnd)
{
    auto const in = std::string{ "i64" };
    auto constexpr InitVal = int64_t{ 888 };

    uint8_t const* end = {};
    auto val = int64_t{ InitVal };
    auto const err = bencParseInt(in, &end, &val);
    EXPECT_EQ(EILSEQ, err);
    EXPECT_EQ(InitVal, val);
    EXPECT_EQ(nullptr, end);
}

TEST_F(VariantTest, parseIntEmptyBuffer)
{
    auto const in = std::string{};
    auto constexpr InitVal = int64_t{ 888 };

    uint8_t const* end = {};
    auto val = int64_t{ InitVal };
    auto const err = bencParseInt(in, &end, &val);
    EXPECT_EQ(EILSEQ, err);
    EXPECT_EQ(InitVal, val);
    EXPECT_EQ(nullptr, end);
}

TEST_F(VariantTest, parseIntWithBadDigits)
{
    auto const in = std::string{ "i6z4e" };
    auto constexpr InitVal = int64_t{ 888 };

    uint8_t const* end = {};
    auto val = int64_t{ InitVal };
    auto const err = bencParseInt(in, &end, &val);
    EXPECT_EQ(EILSEQ, err);
    EXPECT_EQ(InitVal, val);
    EXPECT_EQ(nullptr, end);
}

TEST_F(VariantTest, parseNegativeInt)
{
    auto const in = std::string{ "i-3e" };

    uint8_t const* end = {};
    auto val = int64_t{};
    auto const err = bencParseInt(in, &end, &val);
    EXPECT_EQ(0, err);
    EXPECT_EQ(-3, val);
    EXPECT_EQ(reinterpret_cast<decltype(end)>(in.data() + in.size()), end);
}

TEST_F(VariantTest, parseIntZero)
{
    auto const in = std::string{ "i0e" };

    uint8_t const* end = {};
    auto val = int64_t{};
    auto const err = bencParseInt(in, &end, &val);
    EXPECT_EQ(0, err);
    EXPECT_EQ(0, val);
    EXPECT_EQ(reinterpret_cast<decltype(end)>(in.data() + in.size()), end);
}

TEST_F(VariantTest, parseIntWithLeadingZero)
{
    auto const in = std::string{ "i04e" };
    auto constexpr InitVal = int64_t{ 888 };

    uint8_t const* end = {};
    auto val = int64_t{ InitVal };
    auto const err = bencParseInt(in, &end, &val);
    EXPECT_EQ(EILSEQ, err); // no leading zeroes allowed
    EXPECT_EQ(InitVal, val);
    EXPECT_EQ(nullptr, end);
}

TEST_F(VariantTest, str)
{
    auto buf = std::array<uint8_t, 128>{};
    int err;
    int n;
    uint8_t const* end;
    uint8_t const* str;
    size_t len;

    // string len is designed to overflow
    n = tr_snprintf(buf.data(), buf.size(), "%zu:boat", size_t(SIZE_MAX - 2));
    err = tr_bencParseStr(&buf[0], &buf[n], &end, &str, &len);
    EXPECT_EQ(EILSEQ, err);
    EXPECT_EQ(size_t{}, len);
    EXPECT_EQ(nullptr, str);
    EXPECT_EQ(nullptr, end);

    // good string
    n = tr_snprintf(buf.data(), buf.size(), "4:boat");
    err = tr_bencParseStr(&buf[0], &buf[n], &end, &str, &len);
    EXPECT_EQ(0, err);
    EXPECT_EQ(size_t{ 4 }, len);
    EXPECT_EQ(0, memcmp("boat", str, len));
    EXPECT_EQ(buf.data() + n, end);
    str = nullptr;
    end = nullptr;
    len = 0;

    // string goes past end of buffer
    err = tr_bencParseStr(&buf[0], &buf[n - 1], &end, &str, &len);
    EXPECT_EQ(EILSEQ, err);
    EXPECT_EQ(size_t{}, len);
    EXPECT_EQ(nullptr, str);
    EXPECT_EQ(nullptr, end);

    // empty string
    n = tr_snprintf(buf.data(), buf.size(), "0:");
    err = tr_bencParseStr(&buf[0], &buf[n], &end, &str, &len);
    EXPECT_EQ(0, err);
    EXPECT_EQ(size_t{}, len);
    EXPECT_EQ('\0', *str);
    EXPECT_EQ(buf.data() + n, end);
    str = nullptr;
    end = nullptr;
    len = 0;

    // short string
    n = tr_snprintf(buf.data(), buf.size(), "3:boat");
    err = tr_bencParseStr(&buf[0], &buf[n], &end, &str, &len);
    EXPECT_EQ(0, err);
    EXPECT_EQ(size_t{ 3 }, len);
    EXPECT_EQ(0, memcmp("boa", str, len));
    EXPECT_EQ(buf.data() + 5, end);
    str = nullptr;
    end = nullptr;
    len = 0;
}

TEST_F(VariantTest, parse)
{
    auto benc = "i64e"sv;
    auto i = int64_t{};
    auto val = tr_variant{};
    char const* end;
    auto err = tr_variantFromBencFull(&val, benc, &end);
    EXPECT_EQ(0, err);
    EXPECT_TRUE(tr_variantGetInt(&val, &i));
    EXPECT_EQ(int64_t(64), i);
    EXPECT_EQ(std::data(benc) + std::size(benc), end);
    tr_variantFree(&val);

    benc = "li64ei32ei16ee"sv;
    err = tr_variantFromBencFull(&val, benc, &end);
    EXPECT_EQ(0, err);
    EXPECT_EQ(std::data(benc) + std::size(benc), end);
    EXPECT_EQ(size_t{ 3 }, tr_variantListSize(&val));
    EXPECT_TRUE(tr_variantGetInt(tr_variantListChild(&val, 0), &i));
    EXPECT_EQ(64, i);
    EXPECT_TRUE(tr_variantGetInt(tr_variantListChild(&val, 1), &i));
    EXPECT_EQ(32, i);
    EXPECT_TRUE(tr_variantGetInt(tr_variantListChild(&val, 2), &i));
    EXPECT_EQ(16, i);

    auto len = size_t{};
    auto* saved = tr_variantToStr(&val, TR_VARIANT_FMT_BENC, &len);
    EXPECT_EQ(std::size(benc), len);
    EXPECT_EQ(benc, saved);
    tr_free(saved);

    tr_variantFree(&val);
    end = nullptr;

    benc = "lllee"sv;
    err = tr_variantFromBencFull(&val, benc, &end);
    EXPECT_NE(0, err);
    EXPECT_EQ(nullptr, end);

    benc = "le"sv;
    err = tr_variantFromBencFull(&val, benc, &end);
    EXPECT_EQ(0, err);
    EXPECT_EQ(std::data(benc) + std::size(benc), end);

    saved = tr_variantToStr(&val, TR_VARIANT_FMT_BENC, &len);
    EXPECT_EQ(std::size(benc), len);
    EXPECT_EQ(benc, saved);
    tr_free(saved);
    tr_variantFree(&val);
}

TEST_F(VariantTest, bencParseAndReencode)
{
    struct LocalTest
    {
        std::string_view benc;
        bool is_good;
    };

    auto constexpr Tests = std::array<LocalTest, 9>{ {
        { "llleee"sv, true },
        { "d3:cow3:moo4:spam4:eggse"sv, true },
        { "d4:spaml1:a1:bee"sv, true },
        { "d5:greenli1ei2ei3ee4:spamd1:ai123e3:keyi214eee"sv, true },
        { "d9:publisher3:bob17:publisher-webpage15:www.example.com18:publisher.location4:homee"sv, true },
        { "d8:completei1e8:intervali1800e12:min intervali1800e5:peers0:e"sv, true },
        { "d1:ai0e1:be"sv, false }, // odd number of children
        { ""sv, false },
        { " "sv, false },
    } };

    for (auto const& test : Tests)
    {
        tr_variant val;
        char const* end = nullptr;
        auto const err = tr_variantFromBencFull(&val, test.benc, &end);
        if (!test.is_good)
        {
            EXPECT_NE(0, err);
        }
        else
        {
            EXPECT_EQ(0, err);
            EXPECT_EQ(test.benc.data() + test.benc.size(), end);
            auto saved_len = size_t{};
            auto* saved = tr_variantToStr(&val, TR_VARIANT_FMT_BENC, &saved_len);
            EXPECT_EQ(test.benc, std::string(saved, saved_len));
            tr_free(saved);
            tr_variantFree(&val);
        }
    }
}

TEST_F(VariantTest, bencSortWhenSerializing)
{
    auto constexpr In = "lld1:bi32e1:ai64eeee"sv;
    auto constexpr ExpectedOut = "lld1:ai64e1:bi32eeee"sv;

    tr_variant val;
    char const* end;
    auto const err = tr_variantFromBencFull(&val, In, &end);
    EXPECT_EQ(0, err);
    EXPECT_EQ(std::data(In) + std::size(In), end);

    auto len = size_t{};
    auto* saved = tr_variantToStr(&val, TR_VARIANT_FMT_BENC, &len);
    auto sv = std::string_view{ saved, len };
    EXPECT_EQ(ExpectedOut, sv);
    tr_free(saved);

    tr_variantFree(&val);
}

TEST_F(VariantTest, bencMalformedTooManyEndings)
{
    auto constexpr In = "leee"sv;
    auto constexpr ExpectedOut = "le"sv;

    tr_variant val;
    char const* end;
    auto const err = tr_variantFromBencFull(&val, In, &end);
    EXPECT_EQ(0, err);
    EXPECT_EQ(std::data(In) + std::size(ExpectedOut), end);

    auto len = size_t{};
    auto* saved = tr_variantToStr(&val, TR_VARIANT_FMT_BENC, &len);
    auto sv = std::string_view{ saved, len };
    EXPECT_EQ(ExpectedOut, sv);
    tr_free(saved);

    tr_variantFree(&val);
}

TEST_F(VariantTest, bencMalformedNoEnding)
{
    auto constexpr In = "l1:a1:b1:c"sv;
    tr_variant val;
    EXPECT_EQ(EILSEQ, tr_variantFromBenc(&val, In));
}

TEST_F(VariantTest, bencMalformedIncompleteString)
{
    auto constexpr In = "1:"sv;
    tr_variant val;
    EXPECT_EQ(EILSEQ, tr_variantFromBenc(&val, In));
}

TEST_F(VariantTest, bencToJson)
{
    struct LocalTest
    {
        std::string_view benc;
        std::string_view expected;
    };

    auto constexpr Tests = std::array<LocalTest, 5>{
        { { "i6e"sv, "6"sv },
          { "d5:helloi1e5:worldi2ee"sv, R"({"hello":1,"world":2})"sv },
          { "d5:helloi1e5:worldi2e3:fooli1ei2ei3eee"sv, R"({"foo":[1,2,3],"hello":1,"world":2})"sv },
          { "d5:helloi1e5:worldi2e3:fooli1ei2ei3ed1:ai0eeee"sv, R"({"foo":[1,2,3,{"a":0}],"hello":1,"world":2})"sv },
          { "d4:argsd6:statusle7:status2lee6:result7:successe"sv,
            R"({"args":{"status":[],"status2":[]},"result":"success"})"sv } }
    };

    for (auto const& test : Tests)
    {
        tr_variant top;
        tr_variantFromBenc(&top, test.benc);

        auto len = size_t{};
        auto* str = tr_variantToStr(&top, TR_VARIANT_FMT_JSON_LEAN, &len);
        EXPECT_EQ(test.expected, stripWhitespace(std::string(str, len)));
        tr_free(str);
        tr_variantFree(&top);
    }
}

TEST_F(VariantTest, merge)
{
    auto const i1 = tr_quark_new("i1"sv);
    auto const i2 = tr_quark_new("i2"sv);
    auto const i3 = tr_quark_new("i3"sv);
    auto const i4 = tr_quark_new("i4"sv);
    auto const s5 = tr_quark_new("s5"sv);
    auto const s6 = tr_quark_new("s6"sv);
    auto const s7 = tr_quark_new("s7"sv);
    auto const s8 = tr_quark_new("s8"sv);

    /* initial dictionary (default values) */
    tr_variant dest;
    tr_variantInitDict(&dest, 10);
    tr_variantDictAddInt(&dest, i1, 1);
    tr_variantDictAddInt(&dest, i2, 2);
    tr_variantDictAddInt(&dest, i4, -35); /* remains untouched */
    tr_variantDictAddStr(&dest, s5, "abc");
    tr_variantDictAddStr(&dest, s6, "def");
    tr_variantDictAddStr(&dest, s7, "127.0.0.1"); /* remains untouched */

    /* new dictionary, will overwrite items in dest */
    tr_variant src;
    tr_variantInitDict(&src, 10);
    tr_variantDictAddInt(&src, i1, 1); /* same value */
    tr_variantDictAddInt(&src, i2, 4); /* new value */
    tr_variantDictAddInt(&src, i3, 3); /* new key:value */
    tr_variantDictAddStr(&src, s5, "abc"); /* same value */
    tr_variantDictAddStr(&src, s6, "xyz"); /* new value */
    tr_variantDictAddStr(&src, s8, "ghi"); /* new key:value */

    tr_variantMergeDicts(&dest, /*const*/ &src);

    int64_t i;
    EXPECT_TRUE(tr_variantDictFindInt(&dest, i1, &i));
    EXPECT_EQ(1, i);
    EXPECT_TRUE(tr_variantDictFindInt(&dest, i2, &i));
    EXPECT_EQ(4, i);
    EXPECT_TRUE(tr_variantDictFindInt(&dest, i3, &i));
    EXPECT_EQ(3, i);
    EXPECT_TRUE(tr_variantDictFindInt(&dest, i4, &i));
    EXPECT_EQ(-35, i);
    auto sv = std::string_view{};
    EXPECT_TRUE(tr_variantDictFindStrView(&dest, s5, &sv));
    EXPECT_EQ("abc"sv, sv);
    EXPECT_TRUE(tr_variantDictFindStrView(&dest, s6, &sv));
    EXPECT_EQ("xyz"sv, sv);
    EXPECT_TRUE(tr_variantDictFindStrView(&dest, s7, &sv));
    EXPECT_EQ("127.0.0.1"sv, sv);
    EXPECT_TRUE(tr_variantDictFindStrView(&dest, s8, &sv));
    EXPECT_EQ("ghi"sv, sv);

    tr_variantFree(&dest);
    tr_variantFree(&src);
}

TEST_F(VariantTest, stackSmash)
{
    // make a nested list of list of lists.
    int constexpr Depth = STACK_SMASH_DEPTH;
    std::string const in = std::string(Depth, 'l') + std::string(Depth, 'e');

    // confirm that it parses
    char const* end;
    tr_variant val;
    auto err = tr_variantFromBencFull(&val, in, &end);
    EXPECT_EQ(0, err);
    EXPECT_EQ(in.data() + in.size(), end);

    // confirm that we can serialize it back again
    size_t len;
    auto* saved = tr_variantToStr(&val, TR_VARIANT_FMT_BENC, &len);
    EXPECT_NE(nullptr, saved);
    EXPECT_EQ(in, std::string(saved, len));
    tr_free(saved);

    tr_variantFree(&val);
}

TEST_F(VariantTest, boolAndIntRecast)
{
    auto const key1 = tr_quark_new("key1"sv);
    auto const key2 = tr_quark_new("key2"sv);
    auto const key3 = tr_quark_new("key3"sv);
    auto const key4 = tr_quark_new("key4"sv);

    tr_variant top;
    tr_variantInitDict(&top, 10);
    tr_variantDictAddBool(&top, key1, false);
    tr_variantDictAddBool(&top, key2, 0); // NOLINT modernize-use-bool-literals
    tr_variantDictAddInt(&top, key3, true);
    tr_variantDictAddInt(&top, key4, 1);

    // confirm we can read both bools and ints as bools
    bool b;
    EXPECT_TRUE(tr_variantDictFindBool(&top, key1, &b));
    EXPECT_FALSE(b);
    EXPECT_TRUE(tr_variantDictFindBool(&top, key2, &b));
    EXPECT_FALSE(b);
    EXPECT_TRUE(tr_variantDictFindBool(&top, key3, &b));
    EXPECT_TRUE(b);
    EXPECT_TRUE(tr_variantDictFindBool(&top, key4, &b));
    EXPECT_TRUE(b);

    // confirm we can read both bools and ints as ints
    int64_t i;
    EXPECT_TRUE(tr_variantDictFindInt(&top, key1, &i));
    EXPECT_EQ(0, i);
    EXPECT_TRUE(tr_variantDictFindInt(&top, key2, &i));
    EXPECT_EQ(0, i);
    EXPECT_TRUE(tr_variantDictFindInt(&top, key3, &i));
    EXPECT_NE(0, i);
    EXPECT_TRUE(tr_variantDictFindInt(&top, key4, &i));
    EXPECT_NE(0, i);

    tr_variantFree(&top);
}

TEST_F(VariantTest, dictFindType)
{
    auto constexpr ExpectedStr = "this-is-a-string"sv;
    auto constexpr ExpectedBool = bool{ true };
    auto constexpr ExpectedInt = int{ 1234 };
    auto constexpr ExpectedReal = double{ 0.3 };

    auto const key_bool = tr_quark_new("this-is-a-bool"sv);
    auto const key_real = tr_quark_new("this-is-a-real"sv);
    auto const key_int = tr_quark_new("this-is-an-int"sv);
    auto const key_str = tr_quark_new("this-is-a-string"sv);
    auto const key_unknown = tr_quark_new("this-is-a-missing-entry"sv);

    // populate a dict
    tr_variant top;
    tr_variantInitDict(&top, 0);
    tr_variantDictAddBool(&top, key_bool, ExpectedBool);
    tr_variantDictAddInt(&top, key_int, ExpectedInt);
    tr_variantDictAddReal(&top, key_real, ExpectedReal);
    tr_variantDictAddStr(&top, key_str, ExpectedStr.data());

    // look up the keys as strings
    auto sv = std::string_view{};
    EXPECT_FALSE(tr_variantDictFindStrView(&top, key_bool, &sv));
    EXPECT_FALSE(tr_variantDictFindStrView(&top, key_real, &sv));
    EXPECT_FALSE(tr_variantDictFindStrView(&top, key_int, &sv));
    EXPECT_TRUE(tr_variantDictFindStrView(&top, key_str, &sv));
    EXPECT_EQ(ExpectedStr, sv);
    EXPECT_TRUE(tr_variantDictFindStrView(&top, key_str, &sv));
    EXPECT_EQ(ExpectedStr, sv);
    EXPECT_FALSE(tr_variantDictFindStrView(&top, key_unknown, &sv));
    EXPECT_FALSE(tr_variantDictFindStrView(&top, key_unknown, &sv));

    // look up the keys as bools
    auto b = bool{};
    EXPECT_FALSE(tr_variantDictFindBool(&top, key_int, &b));
    EXPECT_FALSE(tr_variantDictFindBool(&top, key_real, &b));
    EXPECT_FALSE(tr_variantDictFindBool(&top, key_str, &b));
    EXPECT_TRUE(tr_variantDictFindBool(&top, key_bool, &b));
    EXPECT_EQ(ExpectedBool, b);

    // look up the keys as doubles
    auto d = double{};
    EXPECT_FALSE(tr_variantDictFindReal(&top, key_bool, &d));
    EXPECT_TRUE(tr_variantDictFindReal(&top, key_int, &d));
    EXPECT_EQ(ExpectedInt, std::lrint(d));
    EXPECT_FALSE(tr_variantDictFindReal(&top, key_str, &d));
    EXPECT_TRUE(tr_variantDictFindReal(&top, key_real, &d));
    EXPECT_EQ(std::lrint(ExpectedReal * 100), std::lrint(d * 100));

    // look up the keys as ints
    auto i = int64_t{};
    EXPECT_TRUE(tr_variantDictFindInt(&top, key_bool, &i));
    EXPECT_EQ(ExpectedBool ? 1 : 0, i);
    EXPECT_FALSE(tr_variantDictFindInt(&top, key_real, &i));
    EXPECT_FALSE(tr_variantDictFindInt(&top, key_str, &i));
    EXPECT_TRUE(tr_variantDictFindInt(&top, key_int, &i));
    EXPECT_EQ(ExpectedInt, i);

    tr_variantFree(&top);
}
