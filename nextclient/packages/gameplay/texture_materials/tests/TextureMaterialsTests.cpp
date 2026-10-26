#include <gtest/gtest.h>

#include <texture_materials/texture_materials.h>

using texture_materials::MaterialTable;

TEST(TextureMaterials, ReadsLetterAndName)
{
    MaterialTable table;
    table.Load("M FACT_STEEL\nw\tfact_crate\n");

    EXPECT_EQ(table.Find("FACT_STEEL"), 'M');
    EXPECT_EQ(table.Find("FACT_CRATE"), 'W');
    EXPECT_EQ(table.get_materials().size(), 2u);
}

TEST(TextureMaterials, IgnoresCaseOfTheName)
{
    MaterialTable table;
    table.Load("M Fact_Steel");

    EXPECT_EQ(table.Find("fact_steel"), 'M');
    EXPECT_EQ(table.Find("FACT_STEEL"), 'M');
}

TEST(TextureMaterials, ComparesOnlyTheFirstSixteenCharacters)
{
    MaterialTable table;
    table.Load("G abcdefghijklmnopXYZ");

    EXPECT_EQ(table.Find("abcdefghijklmnop"), 'G');
    EXPECT_EQ(table.Find("abcdefghijklmnopQQ"), 'G');
    EXPECT_FALSE(table.Find("abcdefghijklmno"));
}

TEST(TextureMaterials, SkipsCommentsBlankLinesAndLinesWithoutName)
{
    MaterialTable table;
    table.Load("// M commented\n\n   \n1 digit\nM\nM   \n  D dirt // trailing\n");

    EXPECT_EQ(table.get_materials().size(), 1u);
    EXPECT_EQ(table.Find("dirt"), 'D');
}

TEST(TextureMaterials, HandlesWindowsLineEndingsAndNoFinalNewline)
{
    MaterialTable table;
    table.Load("M one\r\nW two");

    EXPECT_EQ(table.Find("one"), 'M');
    EXPECT_EQ(table.Find("two"), 'W');
}

TEST(TextureMaterials, LaterLineReplacesEarlier)
{
    MaterialTable table;
    table.Load("M floor\nW FLOOR\n");

    EXPECT_EQ(table.Find("floor"), 'W');
    EXPECT_EQ(table.get_materials().size(), 1u);
}

TEST(TextureMaterials, ClearForgetsEverything)
{
    MaterialTable table;
    table.Load("M floor");
    table.Clear();

    EXPECT_FALSE(table.Find("floor"));
    EXPECT_TRUE(table.get_materials().empty());
}
