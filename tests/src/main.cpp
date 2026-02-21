#include <gtest/gtest.h>

TEST(MathTest, TwoPlusTwo) {
    int result = 2 + 2;
    EXPECT_EQ(result, 4);
}

TEST(MathTest, TwoPlusTwoNotFive) {
    EXPECT_NE(2 + 2, 5);
}